"""Class registry + Read driver.

Looks up `ClassSpec` by_class_id or name, and applies a spec against a
`HDBSerializer` to produce a populated dict.
"""
from __future__ import annotations

from typing import Any

from hdb_extract.classes.spec import (
    BaseInit, Byte, ByteAlt, BufF, ClassSpec, Dword, DwordF, ObjList,
    Op, Short, String, SubObj, Versioned,
)
from hdb_extract.classes.generated.all_classes import ALL_CLASSES
from hdb_extract.format.fourcc import FOURCC
from hdb_extract.serializer.cursor import HDBSerializer

CLASSES_BY_ID: dict[int, ClassSpec] = {c.class_id: c for c in ALL_CLASSES}
CLASSES_BY_NAME: dict[str, ClassSpec] = {c.name: c for c in ALL_CLASSES}


def spec_for(class_id: int) -> ClassSpec | None:
    return CLASSES_BY_ID.get(class_id)


def apply_read(ser: HDBSerializer, spec: ClassSpec) -> dict[str, Any]:
    """Execute `spec.ops` against `ser`, returning a field dict."""
    from hdb_extract.polymorphe.dispatcher import read_obj, read_obj_list

    out: dict[str, Any] = {"__class__": spec.name, "__class_id__": spec.class_id}
    ser.class_id = spec.class_id

    def _apply_ops(ops: tuple) -> None:
        for op in ops:
            if ser.error:
                return
            if isinstance(op, BaseInit):
                _apply_base_init()
            elif isinstance(op, Dword):
                out[op.field] = ser.read_dword(op.tag)
            elif isinstance(op, Byte):
                out[op.field] = ser.read_byte(op.tag)
            elif isinstance(op, ByteAlt):
                out[op.field] = ser.read_byte_alt(op.tag)
            elif isinstance(op, Short):
                out[op.field] = ser.read_short(op.tag)
            elif isinstance(op, String):
                out[op.field] = ser.read_string(op.tag)
            elif isinstance(op, DwordF):
                out[op.field] = ser.read_dword_fourcc(op.fourcc)
            elif isinstance(op, BufF):
                out[op.field] = ser.read_buf_fourcc(op.n, op.fourcc)
            elif isinstance(op, SubObj):
                out[op.field] = read_obj(ser, FOURCC.NPTc)
            elif isinstance(op, ObjList):
                out[op.field] = read_obj_list(ser, FOURCC.NPlt, FOURCC.NPTc)
            elif isinstance(op, Versioned):
                if ser.version > op.min_version:
                    _apply_ops(op.ops)
            else:
                raise TypeError(f"unknown op: {type(op).__name__}")

    def _apply_base_init() -> None:
        # Mirror of VCObject::Read in the C++ reference decoder (see cpp/).
        ser.begin_record()
        npfl = ser.read_buf_fourcc(2, FOURCC.NPfl)
        out["__npfl__"] = npfl
        if ser.has_version_field:
            vers = ser.read_dword_fourcc(FOURCC.vers)
            ser.version = vers
            out["__vers__"] = vers

    _apply_ops(spec.ops)
    return out
