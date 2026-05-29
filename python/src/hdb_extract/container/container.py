"""HdbContainer — top-level wrapper over the bytes of XFILES.HDB.

Provides :
  - parsed header (32B BE)
  - page enumeration (256B chunks)
  - trailer (176B padding for XFILES.HDB)
  - byte-identical roundtrip (read → emit → equal)
"""
from __future__ import annotations

from collections import Counter
from pathlib import Path
from typing import Iterator

from hdb_extract.container.header import HEADER_SIZE, HdbHeader, parse_header
from hdb_extract.container.pages import (
    PAGE_SIZE,
    PageType,
    classify_page,
)
from hdb_extract.container.trailer import Trailer, parse_trailer


class HdbContainer:
    """Owning wrapper over the raw bytes of an HDB file."""

    def __init__(self, raw: bytes, *, path: Path | None = None):
        self.raw = raw
        self.path = path
        if len(raw) < HEADER_SIZE:
            raise ValueError(f"HDB too small ({len(raw)} bytes)")
        self.header = parse_header(raw)
        self.page_count = (len(raw) - HEADER_SIZE) // PAGE_SIZE
        trailer_off = HEADER_SIZE + self.page_count * PAGE_SIZE
        self.trailer = parse_trailer(raw[trailer_off:])

    @classmethod
    def from_path(cls, path: str | Path) -> "HdbContainer":
        p = Path(path)
        return cls(p.read_bytes(), path=p)

    def page(self, idx: int) -> bytes:
        if idx < 0 or idx >= self.page_count:
            raise IndexError(f"page {idx} out of range (0..{self.page_count - 1})")
        off = HEADER_SIZE + idx * PAGE_SIZE
        return self.raw[off:off + PAGE_SIZE]

    def page_offset(self, idx: int) -> int:
        return HEADER_SIZE + idx * PAGE_SIZE

    def iter_pages(self) -> Iterator[tuple[int, bytes]]:
        for i in range(self.page_count):
            yield i, self.page(i)

    def page_type_distribution(self) -> Counter[int]:
        c: Counter[int] = Counter()
        for _, page in self.iter_pages():
            c[page[0]] += 1
        return c

    def pages_with_tag(self, tag: int) -> list[int]:
        return [i for i, p in self.iter_pages() if p[0] == tag]

    def to_bytes(self) -> bytes:
        """Reconstruct the file bytes (must match the original)."""
        out = bytearray(self.header.to_bytes())
        for i in range(self.page_count):
            out.extend(self.page(i))
        out.extend(self.trailer.raw)
        return bytes(out)

    def roundtrip_ok(self) -> bool:
        return self.to_bytes() == self.raw
