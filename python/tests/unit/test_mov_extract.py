"""Unit tests for the QuickTime MOV atom parser."""
from __future__ import annotations

import struct

import pytest

from hdb_extract.extractors.mov_extract import CONTAINER_ATOMS, MovFile


def _atom(type_: str, payload: bytes = b"") -> bytes:
    """Build a single atom with the given type and payload."""
    size = 8 + len(payload)
    return size.to_bytes(4, "big") + type_.encode("ascii") + payload


def test_empty_file_yields_no_atoms():
    mov = MovFile(b"")
    assert mov.atoms == []


def test_single_flat_atom():
    raw = _atom("ftyp", b"qt  ")
    mov = MovFile(raw)
    assert len(mov.atoms) == 1
    assert mov.atoms[0].type == "ftyp"
    assert mov.atoms[0].size == 12


def test_container_atom_is_recursed():
    # moov is a container; inside, mvhd is a leaf.
    mvhd = _atom("mvhd", b"\x00" * 100)
    moov = _atom("moov", mvhd)
    mov = MovFile(moov)
    assert len(mov.atoms) == 1
    assert mov.atoms[0].type == "moov"
    assert len(mov.atoms[0].children) == 1
    assert mov.atoms[0].children[0].type == "mvhd"


def test_find_walks_atom_path():
    inner = _atom("stsd", b"\x00" * 32)
    minf = _atom("minf", _atom("stbl", inner))
    mdia = _atom("mdia", minf)
    trak = _atom("trak", mdia)
    moov = _atom("moov", trak)
    mov = MovFile(moov)
    a = mov.find("moov", "trak", "mdia", "minf", "stbl", "stsd")
    assert a is not None
    assert a.type == "stsd"


def test_find_returns_none_on_missing():
    mov = MovFile(_atom("moov"))
    assert mov.find("moov", "trak") is None
    assert mov.find("xxxx") is None


def test_truncated_size_handled():
    # Atom claims size=100 but file only has 20 bytes -> stop parsing.
    raw = (100).to_bytes(4, "big") + b"ftyp" + b"\x00" * 12
    mov = MovFile(raw)
    assert mov.atoms == []


def test_size_zero_means_eof():
    """size==0 means 'atom extends to end of file' per QuickTime spec."""
    raw = (0).to_bytes(4, "big") + b"mdat" + b"PAYLOAD"
    mov = MovFile(raw)
    assert len(mov.atoms) == 1
    assert mov.atoms[0].type == "mdat"
    assert mov.atoms[0].size == len(raw)


def test_extended_64bit_size():
    """size==1 means 'next 8 bytes are the 64-bit size'."""
    payload = b"PAYLOAD"
    size = 16 + len(payload)
    raw = (1).to_bytes(4, "big") + b"mdat" + size.to_bytes(8, "big") + payload
    mov = MovFile(raw)
    assert len(mov.atoms) == 1
    assert mov.atoms[0].type == "mdat"
    assert mov.atoms[0].size == size


def test_container_atoms_set():
    """The CONTAINER_ATOMS set must include the standard QT containers."""
    for c in ["moov", "trak", "mdia", "minf", "stbl"]:
        assert c in CONTAINER_ATOMS
