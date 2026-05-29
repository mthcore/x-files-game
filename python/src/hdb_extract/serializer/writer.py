"""HDBWriter — symmetric counterpart to :class:`HDBSerializer`.

Produces the same byte-direct TLV wire format that the reader consumes.
Round-trip: ``apply_write(W, spec, d)`` then ``apply_read(R, spec)``
must return ``d`` (up to default-zero substitutions for missing fields).

Slot layout mirror (see :mod:`hdb_extract.serializer.cursor`):

  Slot +0x14 : write_string(tag, s)        -> [tag(1B)][len(2B LE)][bytes(len)]
  Slot +0x74 : write_buf_fourcc(fc, buf)   -> [fourcc(4B BE)][bytes(len)]
  Slot +0x80 : write_byte(tag, v)          -> [tag(1B)][value(1B)]
  Slot +0x9c : write_dword(tag, v)         -> [tag(1B)][value(4B LE)]
  Slot +0xb0 : write_short(tag, v)         -> [tag(1B)][value(2B LE)]
  Slot +0xbc : write_dword_fourcc(fc, v)   -> [fourcc(4B BE)][value(4B LE)]
"""
from __future__ import annotations

import struct
from typing import Any

from hdb_extract.classes.spec import (
    BaseInit, BufF, Byte, ByteAlt, ClassSpec, Dword, DwordF,
    ObjList, Op, Short, String, SubObj, Versioned,
)


class HDBWriter:
    """Buffer-backed sequential writer producing byte-direct TLV."""

    def __init__(self) -> None:
        self._buf = bytearray()
        self.version = 0
        self.has_version_field = True
        self.class_id: int | None = None

    # --- low-level append helpers --------------------------------------

    def _put(self, data: bytes) -> None:
        self._buf.extend(data)

    @property
    def position(self) -> int:
        return len(self._buf)

    def getvalue(self) -> bytes:
        return bytes(self._buf)

    # --- typed writes (mirror cursor.HDBSerializer slot ports) ---------

    def write_byte(self, tag: int, value: int) -> None:
        self._put(bytes([tag & 0xFF, value & 0xFF]))

    def write_byte_alt(self, tag: int, value: int) -> None:
        self.write_byte(tag, value)

    def write_short(self, tag: int, value: int) -> None:
        self._put(bytes([tag & 0xFF]))
        self._put(struct.pack("<h", value))

    def write_dword(self, tag: int, value: int) -> None:
        self._put(bytes([tag & 0xFF]))
        self._put(struct.pack("<I", value & 0xFFFFFFFF))

    def write_string(self, tag: int, value: str) -> None:
        self._put(bytes([tag & 0xFF]))
        data = value.encode("latin-1", errors="replace")
        self._put(struct.pack("<H", len(data)))
        self._put(data)

    def write_dword_fourcc(self, fourcc: int, value: int) -> None:
        self._put(struct.pack(">I", fourcc & 0xFFFFFFFF))
        self._put(struct.pack("<I", value & 0xFFFFFFFF))

    def write_buf_fourcc(self, n: int, fourcc: int, buf: bytes) -> None:
        if len(buf) != n:
            raise ValueError(
                f"write_buf_fourcc({fourcc=:#x}, n={n}) received buffer of size {len(buf)}"
            )
        self._put(struct.pack(">I", fourcc & 0xFFFFFFFF))
        self._put(buf)

    def begin_record(self) -> None:
        # No-op for in-memory write (matches reader.begin_record).
        pass


def apply_write(w: HDBWriter, spec: ClassSpec, src: dict[str, Any]) -> None:
    """Symmetric counterpart to ``apply_read``.

    Walks ``spec.ops`` in order, looking up each field's value in ``src``.
    Missing fields default to zero (or empty for strings/buffers).
    """
    from hdb_extract.format.fourcc import FOURCC

    def _apply_ops(ops: tuple[Op, ...]) -> None:
        for op in ops:
            if isinstance(op, BaseInit):
                _apply_base_init()
            elif isinstance(op, Byte):
                w.write_byte(op.tag, int(src.get(op.field, 0)))
            elif isinstance(op, ByteAlt):
                w.write_byte_alt(op.tag, int(src.get(op.field, 0)))
            elif isinstance(op, Short):
                w.write_short(op.tag, int(src.get(op.field, 0)))
            elif isinstance(op, Dword):
                w.write_dword(op.tag, int(src.get(op.field, 0)))
            elif isinstance(op, String):
                w.write_string(op.tag, str(src.get(op.field, "")))
            elif isinstance(op, DwordF):
                w.write_dword_fourcc(op.fourcc, int(src.get(op.field, 0)))
            elif isinstance(op, BufF):
                v = src.get(op.field, b"\x00" * op.n)
                if isinstance(v, str):
                    v = v.encode("latin-1", errors="replace")
                if len(v) < op.n:
                    v = bytes(v) + b"\x00" * (op.n - len(v))
                elif len(v) > op.n:
                    v = bytes(v[:op.n])
                w.write_buf_fourcc(op.n, op.fourcc, v)
            elif isinstance(op, (SubObj, ObjList)):
                # Polymorphic sub-objects are not yet handled by the write
                # path. They require a full ClassRegistry round-trip
                # symmetric to read_obj/read_obj_list ; planned for v0.4.
                # For now we skip — the slot stays zero-length on the wire.
                # (Round-trip tests therefore restrict themselves to scalar
                # specs ; see tests/unit/test_writer_roundtrip.py.)
                pass
            elif isinstance(op, Versioned):
                if w.version > op.min_version:
                    _apply_ops(op.ops)
            else:
                raise TypeError(f"unknown op: {type(op).__name__}")

    def _apply_base_init() -> None:
        w.begin_record()
        npfl = src.get("__npfl__", b"\x00\x00")
        if isinstance(npfl, int):
            npfl = struct.pack(">H", npfl)
        if len(npfl) < 2:
            npfl = bytes(npfl) + b"\x00" * (2 - len(npfl))
        elif len(npfl) > 2:
            npfl = bytes(npfl[:2])
        w.write_buf_fourcc(2, FOURCC.NPfl, bytes(npfl))
        if w.has_version_field:
            vers = int(src.get("__vers__", w.version or 1))
            w.write_dword_fourcc(FOURCC.vers, vers)
            w.version = vers

    _apply_ops(spec.ops)
