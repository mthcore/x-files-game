"""Serializer wire format on synthetic blobs (literal FOURCCs).

Mirrors the C++ test_hdb_serializer.cpp expectations. The on-disk HDB
uses a PACKED wire (FOURCC → byte-tag remap) — see docs/wire_packed.md.
"""
import struct

from hdb_extract.format.fourcc import FOURCC
from hdb_extract.serializer.cursor import HDBSerializer


def _be_fourcc(s: str) -> bytes:
    return s.encode("ascii")


def _le_u32(v: int) -> bytes:
    return struct.pack("<I", v)


def _le_u16(v: int) -> bytes:
    return struct.pack("<H", v)


def test_read_byte_tagged():
    blob = bytes([0x65, 0x2a])  # tag 'e' + value 0x2a
    ser = HDBSerializer(blob)
    assert ser.read_byte(0x65) == 0x2a
    assert not ser.error
    assert ser.eof


def test_read_dword_tagged():
    blob = bytes([0x66]) + _le_u32(0x12345678)
    ser = HDBSerializer(blob)
    assert ser.read_dword(0x66) == 0x12345678
    assert not ser.error


def test_read_short_signed():
    blob = bytes([0x6c]) + _le_u16(0xffff)
    ser = HDBSerializer(blob)
    assert ser.read_short(0x6c) == -1


def test_read_string_var():
    blob = bytes([0x70]) + _le_u16(5) + b"hello"
    ser = HDBSerializer(blob)
    assert ser.read_string(0x70) == "hello"


def test_read_buf_fourcc_npfl():
    blob = _be_fourcc("NPfl") + b"\x12\x34"
    ser = HDBSerializer(blob)
    assert ser.read_buf_fourcc(2, FOURCC.NPfl) == b"\x12\x34"
    assert not ser.error


def test_read_dword_fourcc_vers():
    blob = _be_fourcc("vers") + _le_u32(3)
    ser = HDBSerializer(blob)
    assert ser.read_dword_fourcc(FOURCC.vers) == 3
    assert not ser.error


def test_tag_mismatch_marks_error():
    blob = bytes([0x65, 0x00])
    ser = HDBSerializer(blob)
    ser.read_byte(0x66)  # wrong tag
    assert ser.error


def test_eof_marks_error():
    ser = HDBSerializer(b"")
    ser.read_byte(0x65)
    assert ser.error
