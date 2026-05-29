"""ByteUsageMap — parallel bitmap recording which bytes were consumed and why.

Each byte of the HDB gets a usage code. After full extraction, ranges still
marked UNKNOWN are the residual bytes that mark gaps in the format coverage.
"""
from __future__ import annotations

from collections import Counter
from dataclasses import dataclass
from enum import IntEnum
from typing import Iterator


class UsageCode(IntEnum):
    UNKNOWN = 0
    HEADER = 1
    PAGE_TAG = 2
    PAGE_HEADER = 3
    PAGE_EMPTY = 4
    PAGE_FREED = 5
    PAGE_INTERNAL = 6
    BASE_INIT = 7
    TLV_TAG = 8
    TLV_VALUE = 9
    FOURCC_TAG = 10
    FOURCC_VALUE = 11
    NEOIDLIST_HEADER = 12
    RECORD_HEADER = 13
    PRODUCTION_PATH = 14
    NL_BYTECODE = 15
    TRAILER_ZERO = 16
    TRAILER_OTHER = 17
    PAGE_LEAF_BODY = 18
    PAGE_DATA = 19


@dataclass
class UnknownRange:
    abs_off: int
    length: int


class ByteUsageMap:
    """Bytemap parallel to the raw HDB. One byte per file byte."""

    def __init__(self, size: int):
        self.size = size
        self._map = bytearray(size)
        self._context: dict[int, dict] = {}

    def mark(self, off: int, length: int, code: UsageCode, **ctx) -> None:
        end = min(off + length, self.size)
        if off < 0 or off >= self.size:
            return
        for i in range(off, end):
            self._map[i] = code
        if ctx:
            self._context[off] = {"code": int(code), **ctx}

    def code_at(self, off: int) -> UsageCode:
        return UsageCode(self._map[off])

    def coverage(self) -> dict[str, int]:
        c = Counter(self._map)
        return {UsageCode(k).name: v for k, v in c.items()}

    def consumed_count(self) -> int:
        return self.size - self._map.count(0)

    def consumed_pct(self) -> float:
        return self.consumed_count() * 100.0 / self.size

    def iter_unknown_ranges(self, min_len: int = 1) -> Iterator[UnknownRange]:
        i = 0
        while i < self.size:
            if self._map[i] == UsageCode.UNKNOWN:
                start = i
                while i < self.size and self._map[i] == UsageCode.UNKNOWN:
                    i += 1
                if i - start >= min_len:
                    yield UnknownRange(start, i - start)
            else:
                i += 1
