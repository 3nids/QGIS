"""Locating SIP_ macros in the source text and binding them to their owner.

Why this is not simply "ask clang which cursor contains the macro": the macros
expand to nothing, so they fall *outside* the extents of the things they
decorate.  Measured::

    void setR( Renderer *r SIP_TRANSFER );
        method extent  -> 'void setR( Renderer *r SIP_TRANSFER )'
        param  extent  -> 'Renderer *r'            <-- macro not included
    Geometry *clone() const SIP_FACTORY;
        method extent  -> 'Geometry *clone() const' <-- macro not included
    QString load( bool &ok SIP_OUT, int n SIP_PYARGREMOVE = 3 );
        param  extent  -> 'int n SIP_PYARGREMOVE = 3'  <-- included, because
                                                           the default value
                                                           extends the extent

So containment is inconsistent, and a rule built on it silently mis-attributes.

Instead clang is used for what it is reliable at -- saying *where* each
parameter begins -- and the text between those anchors is sliced up here.  A
macro is owned by the parameter slice it falls in, or by the declaration itself
if it falls after the closing parenthesis.  This also sidesteps re-implementing
C++ argument splitting: no counting of commas inside ``QMap<QString, int>``.
"""

from __future__ import annotations

import re
from dataclasses import dataclass

from .spans import Span

SIP_MACRO = re.compile(r"\bSIP_[A-Z0-9_]+\b")


@dataclass(frozen=True)
class MacroUse:
    name: str
    args: tuple[str, ...]
    span: Span  # covers the macro and its argument list, if any

    def __str__(self) -> str:
        return self.name + (f"({', '.join(self.args)})" if self.args else "")


def skip_trivia(text: str, i: int, end: int) -> int:
    """Advance past whitespace and comments."""
    while i < end:
        ch = text[i]
        if ch.isspace():
            i += 1
        elif ch == "/" and i + 1 < end and text[i + 1] == "/":
            nl = text.find("\n", i)
            i = end if nl < 0 else nl
        elif ch == "/" and i + 1 < end and text[i + 1] == "*":
            close = text.find("*/", i + 2)
            i = end if close < 0 else close + 2
        else:
            break
    return i


def matching_paren(text: str, open_at: int) -> int:
    """Offset of the ``)`` closing the ``(`` at ``open_at``.

    Strings, character literals and comments are skipped; digit separators
    (``500'000``) are not mistaken for character literals.
    """
    assert text[open_at] == "("
    depth, i, end = 0, open_at, len(text)
    while i < end:
        ch = text[i]
        if ch == "/" and i + 1 < end and text[i + 1] in "/*":
            i = skip_trivia(text, i, end)
            continue
        if (
            ch == "'"
            and i
            and text[i - 1].isalnum()
            and i + 1 < end
            and (text[i + 1].isalnum() or text[i + 1] == "'")
        ):
            i += 1  # C++14 digit separator
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
                i += 1
            continue
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise ValueError(f"unbalanced '(' at offset {open_at}")


def in_comment_or_string(text: str, span: Span, offset: int) -> bool:
    """Is ``offset`` inside a comment or literal within ``span``?

    Headers talk *about* the macros as well as using them --
    ``// TODO QGIS 5.0 -- drop SIP_SKIP from the following constructors`` is a
    comment, not an annotation -- and treating prose as a directive would skip
    two real constructors.
    """
    i = span.start
    while i < offset:
        ch = text[i]
        if ch == "/" and i + 1 < span.end and text[i + 1] == "/":
            nl = text.find("\n", i)
            nl = span.end if nl < 0 else nl
            if i <= offset < nl:
                return True
            i = nl
            continue
        if ch == "/" and i + 1 < span.end and text[i + 1] == "*":
            close = text.find("*/", i + 2)
            close = span.end if close < 0 else close + 2
            if i <= offset < close:
                return True
            i = close
            continue
        if ch == '"':
            j = i + 1
            while j < span.end:
                if text[j] == "\\":
                    j += 2
                    continue
                if text[j] == '"':
                    j += 1
                    break
                j += 1
            if i <= offset < j:
                return True
            i = j
            continue
        i += 1
    return False


def macro_uses(text: str, span: Span) -> list[MacroUse]:
    """Every ``SIP_*`` use inside ``span``, with its arguments and full extent.

    Occurrences inside comments and string literals are prose, not uses.
    """
    uses: list[MacroUse] = []
    for match in SIP_MACRO.finditer(text, span.start, span.end):
        start, end = match.start(), match.end()
        if in_comment_or_string(text, span, start):
            continue
        args: tuple[str, ...] = ()
        after = skip_trivia(text, end, span.end)
        if after < span.end and text[after] == "(":
            close = matching_paren(text, after)
            inner = text[after + 1 : close]
            args = (
                tuple(a.strip() for a in _split_top_level(inner))
                if inner.strip()
                else ()
            )
            end = close + 1
        uses.append(MacroUse(match.group(0), args, Span(start, end)))
    return uses


def _split_top_level(text: str) -> list[str]:
    """Split on commas that are not nested in (), [], {} or <>."""
    parts, depth, angle, start = [], 0, 0, 0
    for i, ch in enumerate(text):
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        elif ch == "<":
            angle += 1
        elif ch == ">" and angle:
            angle -= 1
        elif ch == "," and depth == 0 and angle == 0:
            parts.append(text[start:i])
            start = i + 1
    parts.append(text[start:])
    return parts


@dataclass(frozen=True)
class Signature:
    """The textual anatomy of one declaration, in original-file offsets."""

    span: Span  # first token through the terminating ';'
    paren_open: int | None
    paren_close: int | None
    arg_slices: tuple[Span, ...]  # one per declared parameter, in order

    @property
    def trailer(self) -> Span | None:
        """Everything after the parameter list -- where method macros live."""
        if self.paren_close is None:
            return None
        return Span(self.paren_close + 1, self.span.end)


def declaration_end(text: str, start: int) -> int:
    """Offset just past the ``;`` (or ``}``) that ends the declaration at ``start``.

    clang's extent stops at the last token it parsed, which for a method is
    before any trailing macro, ``= 0``, initializer list or inline body.  The
    declaration's real text runs to the terminating semicolon.
    """
    i, end = start, len(text)
    depth = 0
    while i < end:
        ch = text[i]
        if ch == "/" and i + 1 < end and text[i + 1] in "/*":
            i = skip_trivia(text, i, end)
            continue
        if ch in "\"'":
            j = i + 1
            while j < end:
                if text[j] == "\\":
                    j += 2
                    continue
                if text[j] == ch:
                    j += 1
                    break
                j += 1
            i = j
            continue
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
            if depth == 0 and ch == "}":
                return i + 1  # inline body
        elif ch == ";" and depth == 0:
            return i + 1
        i += 1
    return end


def analyse(text: str, decl_start: int, arg_starts: list[int]) -> Signature:
    """Slice one declaration into its parameter regions and trailer.

    ``arg_starts`` are the parameters' start offsets, taken from clang -- the
    one thing it reports dependably here.  Each parameter's slice runs to the
    start of the next, so trailing macros and default values stay with the
    parameter they belong to.
    """
    end = declaration_end(text, decl_start)
    span = Span(decl_start, end)

    paren_open = _find_arg_paren(text, decl_start, end)
    if paren_open is None:
        return Signature(span, None, None, ())
    paren_close = matching_paren(text, paren_open)

    bounds = [s for s in arg_starts if paren_open < s < paren_close]
    if not bounds:
        return Signature(span, paren_open, paren_close, ())
    # Extend the first slice back to just after '(' so a macro written before
    # the first parameter still lands on it.
    bounds[0] = paren_open + 1
    slices = [
        Span(bounds[k], bounds[k + 1] if k + 1 < len(bounds) else paren_close)
        for k in range(len(bounds))
    ]
    return Signature(span, paren_open, paren_close, tuple(slices))


def _find_arg_paren(text: str, start: int, end: int) -> int | None:
    """The ``(`` opening the parameter list, skipping ones inside macro args.

    ``QgsFoo SIP_PYALTERNATIVETYPE( int ) bar( int n )`` has two; the parameter
    list is the last one that is not consumed by a macro's own arguments.
    """
    consumed: list[tuple[int, int]] = []
    for use in macro_uses(text, Span(start, end)):
        if use.args:
            consumed.append((use.span.start, use.span.end))
    i = start
    while i < end:
        ch = text[i]
        if ch == "/" and i + 1 < end and text[i + 1] in "/*":
            i = skip_trivia(text, i, end)
            continue
        if ch == "(" and not any(a <= i < b for a, b in consumed):
            return i
        if ch == ";":
            return None
        i += 1
    return None


def head_span(text: str, start: int) -> Span:
    """A type's declaration head: from ``start`` to just before its ``{``.

    A class cursor's extent covers the whole definition, so macros on member
    declarations fall inside it too.  Restricting attribution to the head is
    what keeps ``struct Setting SIP_SKIP`` distinct from a ``SIP_SKIP`` on one
    of its members.  A forward declaration has no brace and ends at the ``;``.
    """
    i, end = start, len(text)
    while i < end:
        ch = text[i]
        if ch == "/" and i + 1 < end and text[i + 1] in "/*":
            i = skip_trivia(text, i, end)
            continue
        if ch in "\"'":
            j = i + 1
            while j < end:
                if text[j] == "\\":
                    j += 2
                    continue
                if text[j] == ch:
                    j += 1
                    break
                j += 1
            i = j
            continue
        if ch in "{;":
            return Span(start, i)
        i += 1
    return Span(start, end)
