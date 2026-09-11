"""Compare the clang front end's declarations against the committed .sip files.

The 1522 checked-in .sip.in files are a perfect oracle -- regenerating them is
byte-identical today -- so "did we find every declaration?" has an exact answer
rather than a plausible one.

Names are compared, not signatures: the spike is answering "is anything
missing?", and the emitter (which renders signatures from source spans) is not
built yet.
"""

from __future__ import annotations

import collections
import os
import re
import sys

import clang.cindex as ci
from sipify import clangenv, exportmacros
from sipify.preprocess import Output, preprocess
from sipify.sourcefile import SourceFile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from spike import DECL_KINDS, MOC_ARTEFACTS, TYPE_KINDS  # noqa: E402

# A declaration line in a .sip: `void foo( int a );`, `class QgsFoo`, `enum X`.
SIP_DECL = re.compile(
    r"^\s*(?:virtual\s+|static\s+|explicit\s+)*"
    r"[\w:<>,\s*&~]*?(\w+)\s*\("
)
SIP_TYPE = re.compile(r"^\s*(?:class|struct|enum(?:\s+class)?)\s+(\w+)")


def sip_path(header: str) -> str:
    parts = header.split("/")
    module, rest = parts[1], "/".join(parts[2:])[:-2]
    return f"python/PyQt6/{module}/auto_generated/{rest}.sip.in"


# Anything inside a %Directive ... %End block is hand-written SIP or C, not a
# declaration the C++ front end is responsible for.
NOT_DECLARATIONS = {
    "if",
    "for",
    "while",
    "switch",
    "catch",
    "return",
    "sizeof",
    "else",
    "do",
    "delete",
    "new",
    "throw",
    "Q_UNUSED",
    "Q_ASSERT",
}


def names_in_sip(path: str) -> set[str]:
    names: set[str] = set()
    depth = 0
    for line in open(path, errors="replace"):
        stripped = line.strip()
        if stripped.startswith("%End"):
            depth = max(0, depth - 1)
            continue
        if stripped.startswith("%"):
            # %Directive opens a block unless it is a one-liner (%Include, %If).
            if not stripped.startswith(
                (
                    "%Include",
                    "%If",
                    "%Import",
                    "%Feature",
                    "%Module",
                    "%Property",
                    "%Timeline",
                    "%Platforms",
                    "%DefaultDocstring",
                )
            ):
                depth += 1
            continue
        if depth or not stripped or stripped.startswith(("/", "*", "//")):
            continue
        m = SIP_TYPE.match(line) or SIP_DECL.match(line)
        if m and m.group(1) not in NOT_DECLARATIONS:
            names.add(m.group(1))
    return names


def names_from_clang(header: str) -> set[str]:
    """Names the pipeline accounts for: parsed by clang, or spliced verbatim.

    A declaration inside ``#ifdef SIP_RUN`` is hand-written SIP that the
    emitter copies through untouched; clang never sees it, and should not.  It
    is covered all the same, so it belongs on this side of the comparison.
    """
    source = SourceFile.read(header)
    exportmacros.check(header, source.text)
    pre = preprocess(source)
    tu = clangenv.parse(header, pre.clang_text)
    names: set[str] = set()
    for cursor in tu.cursor.walk_preorder():
        loc = cursor.location
        if not loc.file or loc.file.name != header:
            continue
        if cursor.kind not in DECL_KINDS | TYPE_KINDS:
            continue
        if cursor.spelling in MOC_ARTEFACTS or not cursor.spelling:
            continue
        if pre.output_of(loc.line) is not Output.EMIT:
            continue
        spelled = cursor.spelling.split("::")[-1].lstrip("~")
        names.add(spelled)
        if spelled.startswith("operator "):  # operator QVariant()
            names.add(spelled.split(" ", 1)[1])
    for region in pre.verbatim_regions():
        for n in range(region.first_line, region.last_line + 1):
            line = source.lines[n - 1]
            m = SIP_TYPE.match(line) or SIP_DECL.match(line)
            if m:
                names.add(m.group(1))
    return names


def main(headers: list[str]) -> int:
    missing_total = collections.Counter()
    perfect = 0
    reports = []
    for header in headers:
        sp = sip_path(header)
        if not os.path.exists(sp):
            continue
        try:
            found = names_from_clang(header)
        except Exception as exc:  # noqa: BLE001
            reports.append((header, f"FAILED {type(exc).__name__}: {exc}"))
            continue
        expected = names_in_sip(sp)
        # Operators and dunders are spelled differently on the two sides; the
        # spike is not chasing those yet.
        expected = {n for n in expected if not n.startswith("__") and n != "operator"}
        missing = expected - found
        if missing:
            missing_total[header] = len(missing)
            reports.append(
                (
                    header,
                    f"missing {len(missing)}/{len(expected)}: {sorted(missing)[:8]}",
                )
            )
        else:
            perfect += 1
    print(f"headers fully covered : {perfect}/{perfect + len(missing_total)}")
    print(f"headers with gaps     : {len(missing_total)}")
    print(f"total missing names   : {sum(missing_total.values())}")
    for header, msg in reports[:20]:
        print(f"   {header}: {msg}")
    return 0 if not missing_total else 1


if __name__ == "__main__":
    args = sys.argv[1:]
    if len(args) == 1 and args[0].endswith(".txt"):
        args = [l.strip() for l in open(args[0]) if l.strip()]
    raise SystemExit(main(args))
