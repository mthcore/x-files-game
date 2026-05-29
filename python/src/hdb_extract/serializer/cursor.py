"""HDBSerializer — Python port of the C++ reference decoder (see `cpp/`).

TLV wire format (per typed slot of the CNeoPersist-derived vtable) :

  Slot +0x14 : read_string(tag)        -> [tag(1B)][len(2B LE)][bytes(len)]
  Slot +0x74 : read_buf_fourcc(fc, n)  -> [fourcc(4B BE)][bytes(n)]
  Slot +0x7c : read_byte_alt(tag)      -> [tag(1B)][value(1B)]
  Slot +0x80 : read_byte(tag)          -> [tag(1B)][value(1B)]
  Slot +0x9c : read_dword(tag)         -> [tag(1B)][value(4B LE)]
  Slot +0xb0 : read_short(tag)         -> [tag(1B)][value(2B LE)] sign-extended
  Slot +0xbc : read_dword_fourcc(fc)   -> [fourcc(4B BE)][value(4B LE)]

The HDB container header is big-endian ; record payloads are
little-endian. FOURCC tags are stored in stream order so 'NPfl' as text
matches the constant 0x4e50666c.

Reads track byte consumption into a `ByteUsageMap` for audit reporting.
"""
from __future__ import annotations

import struct
from typing import Optional

from hdb_extract.serializer.tracking import ByteUsageMap, UsageCode


class TagMismatch(Exception):
    def __init__(self, expected: int, got: int, off: int, kind: str = "byte"):
        super().__init__(
            f"{kind} tag mismatch at off={off:#x} : expected 0x{expected:x}, got 0x{got:x}"
        )
        self.expected = expected
        self.got = got
        self.off = off
        self.kind = kind


class UnexpectedEof(Exception):
    pass


class HDBSerializer:
    """Sequential TLV reader over a byte buffer.

    Parameters
    ----------
    data : bytes
        The bytes to read from.
    start : int
        Cursor start (default 0).
    absolute_offset : int
        File offset of `data[0]` — used to record consumed bytes in the
        ByteUsageMap of the full container. Default 0 (in-memory).
    usage : ByteUsageMap, optional
        If provided, every typed read marks the consumed bytes here.
    """

    def __init__(
        self,
        data: bytes,
        *,
        start: int = 0,
        absolute_offset: int = 0,
        usage: Optional[ByteUsageMap] = None,
    ):
        self.data = data
        self.cursor = start
        self.abs_off = absolute_offset
        self.usage = usage
        self.error = False
        self.version = 0
        self.has_version_field = True
        self.class_id: Optional[int] = None

    @property
    def size(self) -> int:
        return len(self.data)

    @property
    def eof(self) -> bool:
        return self.cursor >= self.size

    def _ensure(self, n: int) -> bool:
        if self.error:
            return False
        if self.cursor + n > self.size:
            self.error = True
            return False
        return True

    def _abs(self, local_off: int) -> int:
        return self.abs_off + local_off

    def _mark(self, off: int, n: int, code: UsageCode, **ctx) -> None:
        if self.usage is not None:
            self.usage.mark(self._abs(off), n, code, **ctx)

    # --- raw read helpers (no tag) ------------------------------------

    def _read_u16_le(self) -> int:
        v = struct.unpack_from("<H", self.data, self.cursor)[0]
        self.cursor += 2
        return v

    def _read_u32_le(self) -> int:
        v = struct.unpack_from("<I", self.data, self.cursor)[0]
        self.cursor += 4
        return v

    def _read_i16_le(self) -> int:
        v = struct.unpack_from("<h", self.data, self.cursor)[0]
        self.cursor += 2
        return v

    def _read_u32_be(self) -> int:
        v = struct.unpack_from(">I", self.data, self.cursor)[0]
        self.cursor += 4
        return v

    # --- tag consumption ----------------------------------------------

    def _expect_byte_tag(self, expected: int) -> bool:
        if not self._ensure(1):
            return False
        got = self.data[self.cursor]
        off = self.cursor
        self.cursor += 1
        if got != expected:
            self.error = True
            return False
        self._mark(off, 1, UsageCode.TLV_TAG, tag=expected)
        return True

    def _expect_fourcc_tag(self, expected: int) -> bool:
        if not self._ensure(4):
            return False
        off = self.cursor
        got = self._read_u32_be()
        if got != expected:
            self.cursor = off  # roll back
            self.error = True
            return False
        self._mark(off, 4, UsageCode.FOURCC_TAG, fourcc=expected)
        return True

    # --- typed reads (slot ports) -------------------------------------

    def read_byte(self, tag: int) -> int:
        """Slot +0x80 : `[tag(1B)][value(1B)]`."""
        if not self._expect_byte_tag(tag):
            return 0
        if not self._ensure(1):
            return 0
        off = self.cursor
        v = self.data[self.cursor]
        self.cursor += 1
        self._mark(off, 1, UsageCode.TLV_VALUE, kind="byte", tag=tag)
        return v

    def read_byte_alt(self, tag: int) -> int:
        """Slot +0x7c : alternate single byte reader (v3+ records).

        On disk this uses a separate vtable slot but the wire shape is
        identical to read_byte ; we keep the alias for parity.
        """
        return self.read_byte(tag)

    def read_short(self, tag: int) -> int:
        """Slot +0xb0 : `[tag(1B)][value(2B LE)]` sign-extended."""
        if not self._expect_byte_tag(tag):
            return 0
        if not self._ensure(2):
            return 0
        off = self.cursor
        v = self._read_i16_le()
        self._mark(off, 2, UsageCode.TLV_VALUE, kind="short", tag=tag)
        return v

    def read_dword(self, tag: int) -> int:
        """Slot +0x9c : `[tag(1B)][value(4B LE)]`."""
        if not self._expect_byte_tag(tag):
            return 0
        if not self._ensure(4):
            return 0
        off = self.cursor
        v = self._read_u32_le()
        self._mark(off, 4, UsageCode.TLV_VALUE, kind="dword", tag=tag)
        return v

    def read_string(self, tag: int) -> str:
        """Slot +0x14 : `[tag(1B)][len(2B LE)][bytes(len)]`."""
        if not self._expect_byte_tag(tag):
            return ""
        if not self._ensure(2):
            return ""
        len_off = self.cursor
        n = self._read_u16_le()
        self._mark(len_off, 2, UsageCode.TLV_VALUE, kind="string_len", tag=tag)
        if not self._ensure(n):
            return ""
        data_off = self.cursor
        s = self.data[self.cursor:self.cursor + n].decode("latin-1", errors="replace")
        self.cursor += n
        self._mark(data_off, n, UsageCode.TLV_VALUE, kind="string_data", tag=tag)
        return s

    def read_dword_fourcc(self, fourcc: int) -> int:
        """Slot +0xbc : `[fourcc(4B BE)][value(4B LE)]`."""
        if not self._expect_fourcc_tag(fourcc):
            return 0
        if not self._ensure(4):
            return 0
        off = self.cursor
        v = self._read_u32_le()
        self._mark(off, 4, UsageCode.FOURCC_VALUE, kind="dword", fourcc=fourcc)
        return v

    def read_buf_fourcc(self, n: int, fourcc: int) -> bytes:
        """Slot +0x74 : `[fourcc(4B BE)][bytes(n)]`."""
        if not self._expect_fourcc_tag(fourcc):
            return b""
        if not self._ensure(n):
            return b""
        off = self.cursor
        buf = bytes(self.data[self.cursor:self.cursor + n])
        self.cursor += n
        self._mark(off, n, UsageCode.FOURCC_VALUE, kind="buf", fourcc=fourcc, count=n)
        return buf

    def begin_record(self) -> None:
        """Slot +0xa8 : called once at the start of each Read body.

        In-memory implementation has no per-record framing ; on disk
        this advances past the leaf record header.
        """
        pass
