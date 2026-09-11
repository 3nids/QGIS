"""The single source of truth for positions in a header."""

from __future__ import annotations

import bisect
from functools import cached_property

from .spans import Span


class SourceFile:
    """A header's text plus bidirectional offset <-> (line, col) maps.

    Lines and columns are 1-based, matching both libclang and every diagnostic
    QGIS developers already read.
    """

    def __init__(self, path: str, text: str):
        self.path = path
        self.text = text

    @classmethod
    def read(cls, path: str) -> SourceFile:
        with open(path, encoding="utf-8") as handle:
            return cls(path, handle.read())

    @cached_property
    def _line_starts(self) -> list[int]:
        starts = [0]
        for i, ch in enumerate(self.text):
            if ch == "\n":
                starts.append(i + 1)
        return starts

    @cached_property
    def lines(self) -> list[str]:
        """Line contents without their terminators, 0-indexed."""
        return self.text.split("\n")

    @property
    def line_count(self) -> int:
        return len(self._line_starts)

    def offset(self, line: int, col: int = 1) -> int:
        return self._line_starts[line - 1] + (col - 1)

    def position(self, offset: int) -> tuple[int, int]:
        line = bisect.bisect_right(self._line_starts, offset)
        return line, offset - self._line_starts[line - 1] + 1

    def line_span(self, line: int) -> Span:
        """The span of one line, excluding its newline."""
        start = self._line_starts[line - 1]
        end = self._line_starts[line] - 1 if line < self.line_count else len(self.text)
        return Span(start, end)

    def span_of_lines(self, first: int, last: int) -> Span:
        return Span(self.line_span(first).start, self.line_span(last).end)

    def text_of(self, span: Span) -> str:
        return self.text[span.start : span.end]
