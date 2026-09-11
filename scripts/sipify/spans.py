"""Source spans and the edit primitive.

Everything sipify emits is a slice of the original header with a list of edits
applied.  libclang is only ever asked *where* a construct is, never what its
type spells: without Qt headers its type spellings are wrong (25% of return
types in qgsvectorlayer.h come back as a bogus ``int``) while its source ranges
stay exact.
"""

from __future__ import annotations

from dataclasses import dataclass, replace


@dataclass(frozen=True, slots=True, order=True)
class Span:
    """A half-open byte range [start, end) in the original header."""

    start: int
    end: int

    def __post_init__(self):
        if self.start > self.end:
            raise ValueError(f"inverted span {self.start}..{self.end}")

    def __len__(self) -> int:
        return self.end - self.start

    def contains(self, other: Span) -> bool:
        return self.start <= other.start and other.end <= self.end

    def overlaps(self, other: Span) -> bool:
        return self.start < other.end and other.start < self.end

    def shifted(self, delta: int) -> Span:
        return replace(self, start=self.start + delta, end=self.end + delta)


@dataclass(frozen=True, slots=True)
class Edit:
    """Replace ``span`` with ``replacement`` when rendering.

    ``priority`` only orders edits that start at the same offset; it never
    lets two overlapping edits coexist.
    """

    span: Span
    replacement: str
    priority: int = 0
    note: str = ""


def apply_edits(text: str, base: int, edits: list[Edit]) -> str:
    """Render ``text`` -- which starts at byte offset ``base`` -- under ``edits``.

    Overlapping edits are a bug in the planner, not something to paper over at
    render time, so they raise.
    """
    ordered = sorted(edits, key=lambda e: (e.span.start, e.priority))
    for a, b in zip(ordered, ordered[1:]):
        if a.span.overlaps(b.span):
            raise ValueError(
                f"overlapping edits: {a.span} {a.note!r} and {b.span} {b.note!r}"
            )

    out: list[str] = []
    cursor = base
    for edit in ordered:
        if edit.span.start < cursor:
            raise ValueError(f"edit {edit.span} starts before cursor {cursor}")
        out.append(text[cursor - base : edit.span.start - base])
        out.append(edit.replacement)
        cursor = edit.span.end
    out.append(text[cursor - base :])
    return "".join(out)
