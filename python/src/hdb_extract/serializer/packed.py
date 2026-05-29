"""PackedHDBSerializer — decoder for the on-disk HDB wire format.

G1 wire format (HdbSerializer, 119 vtable slots) :

The HdbSerializer exposes a fixed set of primitive readers. Each slot
decodes as :

  slot 32 (+0x80) read_byte  : tag ARGUMENT IGNORED, reads 1 raw byte.
  slot 39 (+0x9c) read_dword  : tag IGNORED, reads 4 raw BE bytes.
  slot 44 (+0xb0) read_short  : tag IGNORED, 2 BE bytes.
  slot 29 (+0x74) read_buf_fourcc  : fc IGNORED, N bytes bit-reversed.
  slot 47 (+0xbc) read_dword_fourcc : same as read_dword.

Transform table : table[i] = bit_reverse_8(i).
Transform function : byte-swap (BE→LE 4 bytes).

CONCRETE wire format per field :
  read_byte(tag)        : [value(1B)]              tag NOT on disk
  read_dword(tag)        : [value(4B BE)]          tag NOT on disk
  read_short(tag)        : [value(2B BE)]          tag NOT on disk
  read_string(tag)       : [len(2B BE)][bytes]     tag NOT on disk
  read_buf_fourcc(fc, n) : [n bytes bit-reversed]  fourcc NOT on disk
  read_dword_fourcc(fc)  : [4B BE value]           fourcc NOT on disk

The serializer reads fields SEQUENTIALLY in the order defined by the
class grammar — there are NO tags on disk to identify fields.
"""
from __future__ import annotations

import struct

from hdb_extract.serializer.cursor import HDBSerializer
from hdb_extract.serializer.tracking import UsageCode


# Bit-reversal lookup table used by the on-disk format.
BIT_REVERSE_TABLE = bytes(
    int(f"{i:08b}"[::-1], 2) for i in range(256)
)


class PackedHDBSerializer(HDBSerializer):
    """Decode the on-disk HDB wire format.

    The typed read slots IGNORE their tag/fourcc arguments — those are only
    for application-side validation. On disk, the wire is a sequence of
    raw values in grammar-defined order, with two transforms :
      - 4-byte / 2-byte reads are BIG-ENDIAN (auto byte-swapped to LE)
      - buf_fourcc reads have each byte bit-reversed
    """

    def read_byte(self, tag: int) -> int:
        """slot +0x80 : raw 1-byte read (tag ignored on disk)."""
        if not self._ensure(1):
            return 0
        off = self.cursor
        v = self.data[self.cursor]
        self.cursor += 1
        self._mark(off, 1, UsageCode.TLV_VALUE, kind="byte_packed", tag=tag)
        return v

    def read_byte_alt(self, tag: int) -> int:
        return self.read_byte(tag)

    def read_short(self, tag: int) -> int:
        """slot +0xb0 : raw 2-byte BE read."""
        if not self._ensure(2):
            return 0
        off = self.cursor
        v = struct.unpack_from(">h", self.data, self.cursor)[0]
        self.cursor += 2
        self._mark(off, 2, UsageCode.TLV_VALUE,
                   kind="short_packed_be", tag=tag)
        return v

    def read_dword(self, tag: int) -> int:
        """slot +0x9c : raw 4-byte BE read."""
        if not self._ensure(4):
            return 0
        off = self.cursor
        v = struct.unpack_from(">I", self.data, self.cursor)[0]
        self.cursor += 4
        self._mark(off, 4, UsageCode.TLV_VALUE,
                   kind="dword_packed_be", tag=tag)
        return v

    def read_string(self, tag: int) -> str:
        """slot +0x14 : `[len(2B BE)][bytes(len)]`."""
        if not self._ensure(2):
            return ""
        off = self.cursor
        n = struct.unpack_from(">H", self.data, self.cursor)[0]
        self.cursor += 2
        if not self._ensure(n):
            return ""
        s = self.data[self.cursor:self.cursor + n].decode("latin-1",
                                                           errors="replace")
        self.cursor += n
        self._mark(off, 2 + n, UsageCode.TLV_VALUE,
                   kind="string_packed_be", tag=tag)
        return s

    def read_buf_fourcc(self, n: int, fourcc: int) -> bytes:
        """slot +0x74 : N bytes bit-reversed (no FOURCC prefix on disk)."""
        if not self._ensure(n):
            return b""
        off = self.cursor
        raw_bytes = self.data[self.cursor:self.cursor + n]
        decoded = bytes(BIT_REVERSE_TABLE[b] for b in raw_bytes)
        self.cursor += n
        self._mark(off, n, UsageCode.FOURCC_VALUE,
                   kind="buf_packed_bitrev", fourcc=fourcc, count=n)
        return decoded

    def read_dword_fourcc(self, fourcc: int) -> int:
        """slot +0xbc : 4 bytes BE (no FOURCC prefix on disk)."""
        if not self._ensure(4):
            return 0
        off = self.cursor
        v = struct.unpack_from(">I", self.data, self.cursor)[0]
        self.cursor += 4
        self._mark(off, 4, UsageCode.FOURCC_VALUE,
                   kind="dword_packed_be_nofcc", fourcc=fourcc)
        return v

    def begin_record(self) -> None:
        """slot +0xa8 : no on-disk consumption."""
        pass


def bit_reverse(b: int) -> int:
    """Apply the HDB bit-reversal transform (table )."""
    return BIT_REVERSE_TABLE[b]


def byte_swap_u32(value_or_bytes) -> int:
    """Equivalent : 4 bytes ABCD → DCBA = BE→LE."""
    if isinstance(value_or_bytes, (bytes, bytearray)):
        return struct.unpack(">I", value_or_bytes[:4])[0]
    return (((value_or_bytes & 0xFF000000) >> 24) |
            ((value_or_bytes & 0x00FF0000) >> 8) |
            ((value_or_bytes & 0x0000FF00) << 8) |
            ((value_or_bytes & 0x000000FF) << 24))
