"""Timing for the whole front end, single- and multi-process."""

from __future__ import annotations

import os
import sys
import time
from concurrent.futures import ProcessPoolExecutor

from sipify import clangenv
from sipify.preprocess import preprocess
from sipify.sourcefile import SourceFile

HEADERS = [l.strip() for l in open(sys.argv[1]) if l.strip()]


def one(path: str) -> int:
    source = SourceFile.read(path)
    pre = preprocess(source)
    tu = clangenv.parse(path, pre.clang_text)
    return sum(1 for _ in tu.cursor.walk_preorder())


if __name__ == "__main__":
    sample = HEADERS[:150]
    t = time.time()
    for h in sample:
        one(h)
    serial = time.time() - t
    print(
        f"serial : {len(sample)} headers in {serial:.2f}s "
        f"({serial / len(sample) * 1000:.1f} ms/header)"
    )
    print(
        f"         -> {len(HEADERS)} headers would take ~{serial / len(sample) * len(HEADERS):.1f}s"
    )

    workers = min(10, (os.cpu_count() or 4))
    t = time.time()
    with ProcessPoolExecutor(max_workers=workers) as pool:
        list(pool.map(one, HEADERS, chunksize=8))
    parallel = time.time() - t
    print(
        f"parallel: all {len(HEADERS)} headers in {parallel:.2f}s on {workers} workers"
    )
