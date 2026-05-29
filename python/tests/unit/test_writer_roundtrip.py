"""Roundtrip tests for HDBWriter / apply_write.

These tests build a record dict, write it via :func:`apply_write`, then
re-read it via :func:`apply_read` and check we got back the same field
values.

This is the symmetric byte-direct guarantee for the Python write path.
"""
from __future__ import annotations

from hdb_extract.classes.registry import apply_read, CLASSES_BY_NAME
from hdb_extract.classes.spec import (
    BaseInit, Byte, ClassSpec, Dword, Short, String,
)
from hdb_extract.serializer.cursor import HDBSerializer
from hdb_extract.serializer.writer import HDBWriter, apply_write


def _roundtrip(spec: ClassSpec, src: dict, *, version: int = 1) -> dict:
    w = HDBWriter()
    w.version = version
    apply_write(w, spec, src)
    buf = w.getvalue()
    ser = HDBSerializer(buf)
    return apply_read(ser, spec)


def test_simple_dword_roundtrip():
    spec = ClassSpec(name="X", class_id=1, size_bytes=0, parent="VCObject",
                     ops=(BaseInit(), Dword(0x65, "value")))
    src = {"__npfl__": b"\xab\xcd", "__vers__": 1, "value": 0xDEADBEEF}
    out = _roundtrip(spec, src)
    assert out["value"] == 0xDEADBEEF
    assert out["__class__"] == "X"


def test_byte_and_short_roundtrip():
    spec = ClassSpec(name="Y", class_id=2, size_bytes=0, parent="VCObject",
                     ops=(BaseInit(),
                          Byte(0x66, "b"),
                          Short(0x67, "s")))
    src = {"__npfl__": b"\x01\x02", "__vers__": 1, "b": 0xFE, "s": -123}
    out = _roundtrip(spec, src)
    assert out["b"] == 0xFE
    assert out["s"] == -123


def test_string_roundtrip():
    spec = ClassSpec(name="S", class_id=3, size_bytes=0, parent="VCObject",
                     ops=(BaseInit(), String(0x68, "name")))
    src = {"__npfl__": b"\x00\x00", "__vers__": 1, "name": "Mulder"}
    out = _roundtrip(spec, src)
    assert out["name"] == "Mulder"


def test_default_zero_when_field_missing():
    spec = ClassSpec(name="D", class_id=4, size_bytes=0, parent="VCObject",
                     ops=(BaseInit(), Dword(0x65, "value")))
    src = {"__npfl__": b"\x00\x00", "__vers__": 1}  # 'value' missing
    out = _roundtrip(spec, src)
    assert out["value"] == 0


def test_vctrigger_real_spec_roundtrip():
    """Use the real shipping ClassSpec for VCTrigger (corrected in v0.1)."""
    spec = CLASSES_BY_NAME["VCTrigger"]
    src = {
        "__npfl__": b"\xCC\xCC",
        "__vers__": 1,
        "e": 0xCAFEF00D,
        "f": 0x42,
    }
    out = _roundtrip(spec, src)
    assert out["__class__"] == "VCTrigger"
    assert out["e"] == 0xCAFEF00D
    assert out["f"] == 0x42


def test_writer_position_advances():
    w = HDBWriter()
    assert w.position == 0
    w.write_byte(0x65, 0x42)
    assert w.position == 2
    w.write_dword(0x66, 0xDEADBEEF)
    assert w.position == 2 + 5
