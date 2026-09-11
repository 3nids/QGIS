"""The line-preserving preprocessor pre-pass.

This module owns *every* preprocessor conditional, and it is the reason the
whole design works.  clang must be allowed to evaluate none of them:

* ``#ifdef SIP_RUN`` bodies are SIP syntax, not C++, so clang would choke.
* Conversely, sipify keeps **all** branches of every non-SIP conditional -- it
  drops the directive line and nothing else (``sipify.py:2174``).  If clang
  preprocesses normally it keeps one branch, silently, and 880 declarations
  across 49 headers disappear: the whole of ``qgssfcgalgeometry.h`` behind
  ``#ifdef WITH_SFCGAL``, ``QgsSerialPortSensor``, 34 names in
  ``qgsauthcertutils.h``.
* A prelude that defines the ``SIP_*`` macros empty would make
  ``#ifdef SIP_PYQT5_RUN`` *true* and pull 68 PyQt5-only declarations into the
  PyQt6 output.

So we hand clang a buffer in which every directive line and every region clang
must not see has been overwritten **with spaces**.  Nothing is ever deleted:
the buffer keeps byte-for-byte the same length and line count as the original,
so every clang ``SourceLocation`` maps back to the original file with zero
arithmetic and columns stay valid even on partially blanked lines.

Two orthogonal axes
-------------------
"What clang sees" and "what reaches the ``.sip``" are *not* the same question,
and conflating them corrupts 16 real headers.  The pattern that proves it
(``qgsimagecache.h:171``, ``qgshighlight.h:61``, 14 more) straddles the class
head to work around SIP's inability to inherit from a template::

    #ifdef SIP_RUN
    class CORE_EXPORT QgsImageCache : public QgsAbstractContentCacheBase
    {
    #else
    class CORE_EXPORT QgsImageCache : public QgsAbstractContentCache< QgsImageCacheEntry >
    {
    #endif
        Q_OBJECT
        ...
    };            <-- the closing brace belongs to both branches

Each branch opens a brace that only the shared body closes.  The SIP branch is
what gets *emitted*; the C++ branch is what clang must *parse*, or the class
head vanishes while its ``}`` remains and every declaration after it is
mis-parsed.  Hence:

``clang_visible``
    per line: does this byte range survive into the buffer clang parses?
``Output``
    per line: ``EMIT`` (render from IR), ``PASSTHROUGH`` (copy verbatim),
    ``DROP`` (neither), ``DIRECTIVE`` (the directive line itself -- dropped,
    but tracked apart so the ledger can tell "sipify deleted this" from "the
    author wrote nothing here").
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from enum import Enum, auto

from .sourcefile import SourceFile
from .spans import Span


class Output(Enum):
    EMIT = auto()
    PASSTHROUGH = auto()
    DROP = auto()
    DIRECTIVE = auto()


class RegionKind(Enum):
    SIP_ONLY = auto()  # #ifdef SIP_RUN, or the #else of #ifndef SIP_RUN
    CPP_ONLY = auto()  # #ifndef SIP_RUN, or the #else of #ifdef SIP_RUN
    PYQT5_ONLY = auto()  # #ifdef SIP_PYQT5_RUN -- dropped wholesale
    DISABLED = auto()  # #if 0 / #if defined(Q_OS_WIN)


@dataclass(frozen=True)
class Region:
    kind: RegionKind
    output: Output
    clang_visible: bool
    first_line: int
    last_line: int
    span: Span

    @property
    def is_verbatim(self) -> bool:
        return self.output is Output.PASSTHROUGH


class ClangBufferCorrupt(Exception):
    """Blanking left the buffer clang must parse brace-unbalanced.

    A silent desynchronised parse is the failure mode this whole design has to
    avoid, so it is a hard error rather than something to recover from.
    """


# Anchored exactly as sipify.py has them, including where that is a latent bug.
_DIRECTIVE = re.compile(r"^\s*#")
_DISABLED_IF = re.compile(r"^\s*#if (0|defined\(Q_OS_WIN\))")
_IFDEF_SIP_RUN = re.compile(r"^\s*#ifdef SIP_RUN")
_IFNDEF_SIP_RUN = re.compile(r"^\s*#ifndef SIP_RUN")
_IFDEF_PYQT5 = re.compile(r"^\s*#ifdef SIP_PYQT5_RUN")
_ENDIF = re.compile(r"^\s*#endif")
_ELSE = re.compile(r"^\s*#else")
# NOTE(bug-for-bug): sipify counts nesting with `#if(def)?\s+`, which does NOT
# match `#ifndef` -- so an `#ifndef` nested inside a SIP_RUN block does not
# increment the nesting counter.  Mirrored deliberately; "fixing" it would
# change the output of any header that relies on it.
_NESTING_IF = re.compile(r"^\s*#if(def)?\s+")
# `process_pyqt_ifdefs` terminates on `^#endif`, with no leading whitespace.
_PYQT5_ENDIF = re.compile(r"^#endif")


@dataclass
class PreprocessResult:
    source: SourceFile
    clang_text: str
    output: list[Output]  # 1-based; index 0 is filler
    clang_visible: list[bool]  # 1-based; index 0 is filler
    regions: list[Region] = field(default_factory=list)

    def output_of(self, line: int) -> Output:
        return self.output[line]

    def verbatim_regions(self) -> list[Region]:
        return [r for r in self.regions if r.is_verbatim]

    def dropped_lines(self) -> set[int]:
        return {
            n
            for n in range(1, self.source.line_count + 1)
            if self.output[n] is Output.DROP
        }

    def lines_with(self, output: Output) -> list[int]:
        return [
            n for n in range(1, self.source.line_count + 1) if self.output[n] is output
        ]


def preprocess(source: SourceFile) -> PreprocessResult:
    lines = source.lines
    n = len(lines)
    output: list[Output] = [Output.DIRECTIVE] + [Output.EMIT] * n
    visible: list[bool] = [False] + [True] * n
    regions: list[Region] = []

    def mark(first: int, last: int, out: Output, vis: bool, kind: RegionKind) -> None:
        if last < first:
            return
        for k in range(first, last + 1):
            output[k] = out
            visible[k] = vis
        regions.append(
            Region(kind, out, vis, first, last, source.span_of_lines(first, last))
        )

    def directive(line: int) -> None:
        output[line] = Output.DIRECTIVE
        visible[line] = False

    i = 1
    while i <= n:
        text = lines[i - 1]

        if not _DIRECTIVE.match(text):
            i += 1
            continue

        # --- #if 0 / #if defined(Q_OS_WIN): first branch gone, #else kept.
        if _DISABLED_IF.match(text):
            directive(i)
            body_start, depth, j = i + 1, 0, i + 1
            while j <= n:
                cur = lines[j - 1]
                if _NESTING_IF.match(cur):
                    depth += 1
                elif depth == 0 and (_ENDIF.match(cur) or _ELSE.match(cur)):
                    break
                elif depth != 0 and _ENDIF.match(cur):
                    depth -= 1
                j += 1
            # Dead code need not even compile, so clang must not see it.
            mark(body_start, j - 1, Output.DROP, False, RegionKind.DISABLED)
            if j <= n:
                directive(j)
            i = j + 1
            continue

        # --- #ifdef SIP_PYQT5_RUN: the whole region goes, wholesale.
        if _IFDEF_PYQT5.match(text):
            j = i + 1
            while j <= n and not _PYQT5_ENDIF.match(lines[j - 1]):
                j += 1
            mark(i, min(j, n), Output.DROP, False, RegionKind.PYQT5_ONLY)
            i = j + 1
            continue

        # --- the two SIP_RUN forms.
        if _IFDEF_SIP_RUN.match(text):
            directive(i)
            i = _scan_sip_conditional(lines, n, i + 1, mark, directive, sip_first=True)
            continue

        if _IFNDEF_SIP_RUN.match(text):
            directive(i)
            i = _scan_sip_conditional(lines, n, i + 1, mark, directive, sip_first=False)
            continue

        # --- every other directive: drop the line, keep every branch.
        directive(i)
        i += 1

    clang_lines = [
        lines[k - 1] if visible[k] else " " * len(lines[k - 1]) for k in range(1, n + 1)
    ]
    result = PreprocessResult(source, "\n".join(clang_lines), output, visible, regions)
    _assert_parseable(result)
    return result


def _scan_sip_conditional(lines, n, start, mark, directive, *, sip_first: bool) -> int:
    """Scan one SIP_RUN conditional from ``start``; return the line after it.

    ``sip_first`` says whether the first branch is the SIP one (``#ifdef``) or
    the C++ one (``#ifndef``).  The SIP branch is emitted verbatim and hidden
    from clang; the C++ branch is the reverse -- clang parses it, nothing of it
    reaches the ``.sip``.
    """
    sip_branch = sip_first
    depth = 0
    branch_start = start
    j = start
    while j <= n:
        cur = lines[j - 1]
        if _NESTING_IF.match(cur):
            depth += 1
        elif depth == 0 and _ELSE.match(cur):
            _close_branch(mark, branch_start, j - 1, sip_branch)
            directive(j)
            sip_branch = not sip_branch
            branch_start = j + 1
        elif _ENDIF.match(cur):
            if depth == 0:
                _close_branch(mark, branch_start, j - 1, sip_branch)
                directive(j)
                return j + 1
            depth -= 1
        j += 1
    _close_branch(mark, branch_start, n, sip_branch)
    return n + 1


def _close_branch(mark, first: int, last: int, sip_branch: bool) -> None:
    if sip_branch:
        mark(first, last, Output.PASSTHROUGH, False, RegionKind.SIP_ONLY)
    else:
        mark(first, last, Output.DROP, True, RegionKind.CPP_ONLY)


def brace_balance(text: str) -> int:
    """``{`` minus ``}``, ignoring comments, strings and character literals.

    Raw counting is not good enough: doxygen ``\\code`` blocks and string
    literals both carry braces, and 425 headers would false-alarm.
    """
    depth = 0
    i, end = 0, len(text)
    while i < end:
        ch = text[i]
        if ch == "/" and i + 1 < end:
            nxt = text[i + 1]
            if nxt == "/":
                i = text.find("\n", i)
                if i < 0:
                    break
                continue
            if nxt == "*":
                close = text.find("*/", i + 2)
                i = end if close < 0 else close + 2
                continue
        if (
            ch == "'"
            and i
            and text[i - 1].isalnum()
            and i + 1 < end
            and (text[i + 1].isalnum() or text[i + 1] == "'")
        ):
            # C++14 digit separator, e.g. `500'000` -- not a character literal.
            i += 1
            continue
        if ch in "\"'":
            quote, i = ch, i + 1
            while i < end:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == quote:
                    i += 1
                    break
                if text[i] == "\n" and quote == "'":
                    break  # unterminated: a stray apostrophe in prose
                i += 1
            continue
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
        i += 1
    return depth


def _assert_parseable(result: PreprocessResult) -> None:
    """The buffer clang parses must be brace-balanced.

    The *original* often is not -- a class head straddling ``#ifdef SIP_RUN``
    contributes two opening braces for one closing brace -- so this is a
    property of the projection, not of the header.
    """
    depth = brace_balance(result.clang_text)
    if depth != 0:
        raise ClangBufferCorrupt(
            f"{result.source.path}: clang buffer brace balance is {depth:+d}, "
            f"expected 0 -- blanking desynchronised the parse"
        )
