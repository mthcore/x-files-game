"""CNeoIDList record — 30-byte header preceding each 'ID  ' marker.

Layout (from `the format notes`,
validated on page 91 of XFILES.HDB) :

    +0x00  uint32 flag_const   =     (BE)
    +0x04  uint32 count                          (BE)
    +0x08  uint32 offset_or_size                 (BE)
    +0x0c  uint32 class_id     = 0x0b CNeoIDList (BE)
    +0x10  char[4] fourcc      = 'ID  '
    +0x14  uint32 value                          (BE)
    +0x18  uint8[6] pad        = ff 00 00 00 00 00

Total : 30 bytes.
"""
from __future__ import annotations

import struct
from dataclasses import dataclass

CNEOIDLIST_RECORD_SIZE = 30
CNEOIDLIST_CLASS_ID = 0x0b
CNEOIDLIST_FOURCC = b"ID  "

_STRUCT = struct.Struct(">IIII4sI6s")


@dataclass(frozen=True)
class CNeoIDListRecord:
    flag_const: int
    count: int
    offset_or_size: int
    class_id: int
    fourcc: bytes
    value: int
    pad: bytes

    @property
    def is_valid(self) -> bool:
        return (
            self.flag_const == 1
            and self.class_id == CNEOIDLIST_CLASS_ID
            and self.fourcc == CNEOIDLIST_FOURCC
        )


def parse_neoidlist_record(data: bytes, off: int = 0) -> CNeoIDListRecord:
    fields = _STRUCT.unpack_from(data, off)
    return CNeoIDListRecord(*fields)
