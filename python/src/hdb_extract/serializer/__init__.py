"""HDB serializer : TLV wire format + byte usage tracking.

Mirrors the C++ reference decoder (see `cpp/`).
Reads sequentially from a buffer ; each typed read consumes
`[tag(s)][value(s)]` bytes from the cursor.

Each read also records the consumed bytes in a `ByteUsageMap` so we can
audit which bytes of XFILES.HDB are still unaccounted for.
"""
from hdb_extract.serializer.cursor import HDBSerializer
from hdb_extract.serializer.packed import (
    BIT_REVERSE_TABLE,
    PackedHDBSerializer,
    bit_reverse,
    byte_swap_u32,
)
from hdb_extract.serializer.tracking import ByteUsageMap, UsageCode

__all__ = [
    "BIT_REVERSE_TABLE",
    "ByteUsageMap",
    "HDBSerializer",
    "PackedHDBSerializer",
    "UsageCode",
    "bit_reverse",
    "byte_swap_u32",
]
