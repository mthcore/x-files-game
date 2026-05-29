"""ClassSpec — declarative description of a VC*::Read body.

Each VC* class's `Read` method is a sequence of typed reads against the
HDBSerializer. We model that as a list of `Op` objects so a single driver
can execute any class.

Ops capture native semantics 1:1 :

    BaseInit            VCObject::Read base init (NPfl + vers)
    Dword(tag,field)    serializer.read_dword(tag)
    Byte(tag,field)     serializer.read_byte(tag)
    ByteAlt(tag,field)  serializer.read_byte_alt(tag) (alt slot 0x7c)
    Short(tag,field)    serializer.read_short(tag)   sign-extended
    String(tag,field)   serializer.read_string(tag)
    DwordF(fc,field)    serializer.read_dword_fourcc(fourcc)
    BufF(fc,n,field)    serializer.read_buf_fourcc(n, fourcc)
    SubObj(name)        read_obj (NPTc polymorphic)
    ObjList(name)       read_obj_list (NPlt + NPTc)
    Versioned(min,ops)  conditional on serializer.version > min
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Sequence, Union


@dataclass(frozen=True)
class BaseInit:
    """Invoke VCObject::Read base init (NPfl + vers + flag housekeeping)."""


@dataclass(frozen=True)
class Dword:
    tag: int
    field: str


@dataclass(frozen=True)
class Byte:
    tag: int
    field: str


@dataclass(frozen=True)
class ByteAlt:
    tag: int
    field: str


@dataclass(frozen=True)
class Short:
    tag: int
    field: str


@dataclass(frozen=True)
class String:
    tag: int
    field: str


@dataclass(frozen=True)
class DwordF:
    fourcc: int
    field: str


@dataclass(frozen=True)
class BufF:
    fourcc: int
    n: int
    field: str


@dataclass(frozen=True)
class SubObj:
    field: str


@dataclass(frozen=True)
class ObjList:
    field: str


@dataclass(frozen=True)
class Versioned:
    min_version: int
    ops: tuple


Op = Union[
    BaseInit, Dword, Byte, ByteAlt, Short, String, DwordF, BufF,
    SubObj, ObjList, Versioned,
]


@dataclass(frozen=True)
class ClassSpec:
    """A `Read` body, fully data-driven.

    Parameters
    ----------
    name : str
        e.g. ``"VCConversation"``.
    class_id : int
        Master registry id (0x27..0x5c for user classes).
    size_bytes : int
        Total in-memory object size (matches the on-disk layout).
    parent : str
        Parent class name (e.g. ``"VCObject"`` or ``"VCNode"``).
    ops : tuple[Op, ...]
        Ordered sequence of Op describing the Read body.
    notes : str
        Free-form notes (decoder reference, gotchas, version semantics).
    """
    name: str
    class_id: int
    size_bytes: int
    parent: str
    ops: tuple
    notes: str = ""
