"""HDB header — 32 bytes big-endian.

Layout (from `the format notes`) :

    struct HdbHeader {
        uint32_t version;          // 5
        uint32_t root_or_size;     // 5,310,632 in XFILES.HDB
        uint32_t record_count;     // 64,896
        uint32_t class_count;      // 39 (master registered classes)
        uint32_t unk_94982;
        uint32_t unk_1281;         // constant 1281
        uint32_t max_or_limit;     // = 262,144
        uint32_t page_size;        // 256
    };

All fields are big-endian (NeoLogic Hammer convention, Mac/Apple heritage).
"""
from __future__ import annotations

import struct
from dataclasses import dataclass

HEADER_SIZE = 32
HEADER_STRUCT = struct.Struct(">8I")


@dataclass(frozen=True)
class HdbHeader:
    version: int
    root_or_size: int
    record_count: int
    class_count: int
    unk_94982: int
    unk_1281: int
    max_or_limit: int
    page_size: int

    def to_bytes(self) -> bytes:
        return HEADER_STRUCT.pack(
            self.version, self.root_or_size, self.record_count,
            self.class_count, self.unk_94982, self.unk_1281,
            self.max_or_limit, self.page_size,
        )


def parse_header(raw: bytes) -> HdbHeader:
    """Parse the 32-byte big-endian header from the start of `raw`."""
    if len(raw) < HEADER_SIZE:
        raise ValueError(f"raw too small ({len(raw)} < {HEADER_SIZE})")
    fields = HEADER_STRUCT.unpack_from(raw, 0)
    return HdbHeader(*fields)
