"""libclang setup: one fixed, deliberately minimal invocation.

Every flag is load-bearing:

``-std=c++20``
    ``CMakeLists.txt:80`` sets ``CMAKE_CXX_STANDARD 20``.  At ``c++17`` a
    header using concepts mis-parses.
``-nostdinc -nostdinc++``
    No Qt, no libc++, no build tree -- which is what keeps sipify a pip-only
    job in CI.  sipify needs structure, not semantics, so opaque types cost
    nothing; see ``frontend_cpp`` for why types must never be read from clang.
``-ferror-limit=0``
    Never stop early.  Diagnostics are not a safety net here anyway -- the
    first unresolved ``#include`` is fatal and suppresses everything after it,
    so a header with 394 real errors reports 1.  ``failclosed.py`` is the net.
``PARSE_SKIP_FUNCTION_BODIES``
    Inline bodies are never emitted, and skipping them is most of the speed.

The prelude and ``qgis_sip.h`` come in via ``-include`` rather than being
prepended to the buffer, so buffer line 1 stays file line 1 and every
``SourceLocation`` is directly a location in the original header.
"""

from __future__ import annotations

import os
from functools import cache, lru_cache

import clang.cindex as ci

from . import forward

CPP_STANDARD = "c++20"
_HERE = os.path.dirname(os.path.abspath(__file__))
PRELUDE = os.path.join(_HERE, "prelude.h")
# The real macro header, included rather than copied so the two cannot drift.
QGIS_SIP_H = os.path.normpath(
    os.path.join(_HERE, os.pardir, os.pardir, "src", "core", "qgis_sip.h")
)

_LIBRARY_CANDIDATES = (
    "/Library/Developer/CommandLineTools/usr/lib/libclang.dylib",
    "/opt/homebrew/opt/llvm/lib/libclang.dylib",
    "/usr/lib/llvm-18/lib/libclang.so.1",
)


class ClangUnavailable(Exception):
    pass


def configure(library_file: str | None = None) -> None:
    """Point the bindings at a libclang.  Idempotent."""
    if getattr(ci.Config, "loaded", False):
        return
    if library_file:
        ci.Config.set_library_file(library_file)
        return
    try:  # the pip `libclang` wheel ships one
        import clang.native  # noqa: F401

        return
    except ImportError:
        pass
    for candidate in _LIBRARY_CANDIDATES:
        if os.path.exists(candidate):
            ci.Config.set_library_file(candidate)
            return
    raise ClangUnavailable(
        "no libclang found; `pip install libclang`, or set SIPIFY_LIBCLANG to a "
        "libclang shared library"
    )


# Where the per-header synthesised forward declarations are injected from.
# It is a virtual path: the file never exists on disk, only as an unsaved file.
FORWARD_DECLS = "/sipify/forward_decls.h"


def flags() -> list[str]:
    return [
        "-x",
        "c++",
        f"-std={CPP_STANDARD}",
        "-nostdinc",
        "-nostdinc++",
        "-ferror-limit=0",
        "-Wno-everything",
        "-fno-spell-checking",
        "-include",
        PRELUDE,
        "-include",
        QGIS_SIP_H,
        "-include",
        FORWARD_DECLS,
    ]


@cache
def _index() -> ci.Index:
    configure(os.environ.get("SIPIFY_LIBCLANG") or None)
    return ci.Index.create()


def parse(path: str, buffer: str) -> ci.TranslationUnit:
    """Parse ``buffer`` as if it were the file at ``path``.

    ``buffer`` is the preprocessed projection rather than the bytes on disk,
    but it has the same length and line count, so every location it yields is
    equally a location in the original header.

    The synthesised forward declarations go in as a separate unsaved file
    pulled in by ``-include``, never prepended, so the header's own line
    numbering is untouched.
    """
    return _index().parse(
        path,
        args=flags(),
        unsaved_files=[
            (FORWARD_DECLS, forward.synthesize(buffer)),
            (path, buffer),
        ],
        options=ci.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES,
    )
