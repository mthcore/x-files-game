"""Drive a couple ClassSpec against synthetic TLV blobs.

Validates that the data-driven apply_read driver matches the hand-written
C++ Read body for the same class on equivalent input.
"""
import struct

from hdb_extract.classes.registry import apply_read, spec_for
from hdb_extract.format.fourcc import FOURCC
from hdb_extract.serializer.cursor import HDBSerializer


def _build_base_init(npfl_bytes: bytes = b"\x00\x00", vers: int = 3) -> bytes:
    out = bytearray()
    out += b"NPfl"
    out += npfl_bytes
    out += b"vers"
    out += struct.pack("<I", vers)
    return bytes(out)


def test_vc_nav_three_dwords():
    """VCNav (0x33) = base + 3 dwords (e, f, g)."""
    spec = spec_for(0x33)
    assert spec is not None and spec.name == "VCNav"

    blob = bytearray()
    blob += _build_base_init(vers=3)
    blob += bytes([0x65]) + struct.pack("<I", 100)  # e
    blob += bytes([0x66]) + struct.pack("<I", 200)  # f
    blob += bytes([0x67]) + struct.pack("<I", 300)  # g

    ser = HDBSerializer(bytes(blob))
    result = apply_read(ser, spec)
    assert not ser.error
    assert result["__class__"] == "VCNav"
    assert result["__vers__"] == 3
    assert result["e_at_0x28"] == 100
    assert result["f_at_0x2c"] == 200
    assert result["g_at_0x30"] == 300


def test_vc_conversation_versioned_fields():
    """VCConversation (0x31) version-gated fields :
       v=2 : just e/f/h/j
       v=3 : adds g, i
       v=4 : adds k (byte_alt)
    """
    spec = spec_for(0x31)
    assert spec is not None and spec.name == "VCConversation"

    # v=2 — only e, f, h, j
    blob = bytearray()
    blob += _build_base_init(vers=2)
    for tag in (0x65, 0x66, 0x68, 0x6a):
        blob += bytes([tag]) + struct.pack("<I", tag)
    ser = HDBSerializer(bytes(blob))
    r = apply_read(ser, spec)
    assert not ser.error
    assert r["e_at_0x28"] == 0x65
    assert r["j_at_0x3c"] == 0x6a
    assert "g_at_0x30" not in r

    # v=4 — adds g, i, k
    blob = bytearray()
    blob += _build_base_init(vers=4)
    for tag in (0x65, 0x66, 0x68, 0x6a):
        blob += bytes([tag]) + struct.pack("<I", tag)
    for tag in (0x67, 0x69):
        blob += bytes([tag]) + struct.pack("<I", tag)
    blob += bytes([0x6b, 0xbb])  # k = byte_alt (slot 0x7c) value 0xbb
    ser = HDBSerializer(bytes(blob))
    r = apply_read(ser, spec)
    assert not ser.error
    assert r["g_at_0x30"] == 0x67
    assert r["i_at_0x38"] == 0x69
    assert r["k_at_0x40"] == 0xbb


def test_vc_character_one_dword():
    """VCCharacter (0x2c) = base + 1 dword 'e'."""
    spec = spec_for(0x2c)
    assert spec.name == "VCCharacter"
    blob = _build_base_init(vers=1) + bytes([0x65]) + struct.pack("<I", 42)
    ser = HDBSerializer(blob)
    r = apply_read(ser, spec)
    assert not ser.error
    assert r["e"] == 42
