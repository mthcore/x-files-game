"""Unit tests for the GAM savegame wrapper.

These build synthetic minimal containers; they don't require a real
XFILES.GAM. See ``tests/integration/test_gam_real.py`` for the real-file
round-trip test.
"""
from __future__ import annotations

import struct

import pytest

from hdb_extract.container.container import HdbContainer
from hdb_extract.extractors.gam_extract import GAM_FILE_VERSION, GamFile


def _synth_gam_bytes(version: int) -> bytes:
    """Build the smallest legal HDB/GAM-like buffer for testing."""
    # 32-byte header (BE), one empty page, no trailer.
    header = struct.pack(">IIIIIIII",
                          version,  # version
                          0,        # root_or_size
                          0,        # record_count
                          0,        # class_count
                          0,        # unk_94982
                          0,        # unk_1281
                          0,        # max_or_limit
                          256,      # page_size
                          )
    page = b"\x00" * 256
    return header + page


def test_gam_is_recognised_by_version():
    raw = _synth_gam_bytes(version=GAM_FILE_VERSION)
    gam = GamFile(HdbContainer(raw))
    assert gam.is_gam() is True
    assert gam.header.version == GAM_FILE_VERSION


def test_hdb_is_not_gam():
    raw = _synth_gam_bytes(version=3)  # HDB
    gam = GamFile(HdbContainer(raw))
    assert gam.is_gam() is False


def test_summary_includes_required_keys():
    raw = _synth_gam_bytes(version=GAM_FILE_VERSION)
    summary = GamFile(HdbContainer(raw)).summary()
    for key in ["file_size", "header", "pages", "trailer",
                "roundtrip_byte_identical", "is_gam"]:
        assert key in summary, f"missing key {key}"
    assert summary["is_gam"] is True
    assert summary["roundtrip_byte_identical"] is True


def test_roundtrip_byte_identical_on_synthetic():
    raw = _synth_gam_bytes(version=GAM_FILE_VERSION)
    gam = GamFile(HdbContainer(raw))
    assert gam.roundtrip_ok()


def test_extract_writes_json(tmp_path):
    from hdb_extract.extractors.gam_extract import extract_gam_to_json

    src = tmp_path / "x.gam"
    src.write_bytes(_synth_gam_bytes(version=GAM_FILE_VERSION))
    out = tmp_path / "out" / "summary.json"
    summary = extract_gam_to_json(src, out)
    assert out.is_file()
    assert summary["is_gam"] is True
