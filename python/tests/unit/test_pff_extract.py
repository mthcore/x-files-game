"""Unit tests for the PFF archive parser.

These tests use synthetic PFF buffers built in-memory; they do not need
the real game files. For integration testing against the real game
archives, see ``tests/integration/test_pff_real.py``.
"""
from __future__ import annotations

import struct

import pytest

from hdb_extract.extractors.pff_extract import (
    PFF_MAGIC,
    PADDING_ALIGN,
    PffArchive,
)


def _build_pff(entries: list[bytes]) -> bytes:
    """Construct a syntactically valid PFF archive from a list of payloads."""
    count = len(entries)
    header_size = 8 + (count + 1) * 4
    # First-entry offset is aligned to 0x200.
    first_offset = ((header_size + PADDING_ALIGN - 1) // PADDING_ALIGN) * PADDING_ALIGN
    offsets = [first_offset]
    cur = first_offset
    for e in entries:
        cur += len(e)
        offsets.append(cur)
    out = bytearray()
    out += PFF_MAGIC
    out += struct.pack("<I", count)
    for off in offsets:
        out += struct.pack("<I", off)
    out += b"\x00" * (first_offset - header_size)
    for e in entries:
        out += e
    return bytes(out)


def test_empty_archive():
    raw = _build_pff([])
    pff = PffArchive(raw)
    assert pff.entry_count == 0
    assert pff.roundtrip_ok()


def test_single_entry():
    raw = _build_pff([b"hello world"])
    pff = PffArchive(raw)
    assert pff.entry_count == 1
    assert pff.entry(0) == b"hello world"
    assert pff.entry_size(0) == 11
    assert pff.roundtrip_ok()


def test_multiple_entries():
    payloads = [b"PICT-1", b"PICT-2 (longer)", b"P-3"]
    raw = _build_pff(payloads)
    pff = PffArchive(raw)
    assert pff.entry_count == 3
    for i, expected in enumerate(payloads):
        assert pff.entry(i) == expected
    assert pff.roundtrip_ok()


def test_iter_entries():
    payloads = [b"a", b"bb", b"ccc"]
    raw = _build_pff(payloads)
    pff = PffArchive(raw)
    seen = list(pff.iter_entries())
    assert [b for _, b in seen] == payloads
    assert [i for i, _ in seen] == [0, 1, 2]


def test_bad_magic_rejected():
    raw = bytearray(_build_pff([b"x"]))
    raw[0] = ord("X")
    with pytest.raises(ValueError, match="bad magic"):
        PffArchive(bytes(raw))


def test_size_mismatch_rejected():
    raw = _build_pff([b"x"]) + b"trailing-garbage"
    with pytest.raises(ValueError, match="offsets\\[count\\]"):
        PffArchive(raw)


def test_nonzero_padding_rejected():
    raw = bytearray(_build_pff([b"x"]))
    header_size = 8 + (1 + 1) * 4
    raw[header_size] = 0xFF
    with pytest.raises(ValueError, match="non-zero bytes"):
        PffArchive(bytes(raw))


def test_too_small_rejected():
    with pytest.raises(ValueError, match="too small"):
        PffArchive(b"PF")


def test_oob_entry_index():
    raw = _build_pff([b"x"])
    pff = PffArchive(raw)
    with pytest.raises(IndexError):
        pff.entry(5)
    with pytest.raises(IndexError):
        pff.entry(-1)


def test_padding_alignment():
    """First entry must always be at a 0x200 boundary."""
    raw = _build_pff([b"x"] * 100)
    pff = PffArchive(raw)
    assert pff.offsets[0] % PADDING_ALIGN == 0


def test_extract_to_dir(tmp_path):
    from hdb_extract.extractors.pff_extract import extract_pff_to_dir

    src = tmp_path / "sample.pff"
    src.write_bytes(_build_pff([b"alpha", b"beta", b"gamma"]))
    out = tmp_path / "out"
    n = extract_pff_to_dir(src, out)
    assert n == 3
    assert (out / "entry_0000.bin").read_bytes() == b"alpha"
    assert (out / "entry_0001.bin").read_bytes() == b"beta"
    assert (out / "entry_0002.bin").read_bytes() == b"gamma"
