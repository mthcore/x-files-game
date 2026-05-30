"""HSPT decoder tests: synthetic round-trips + optional real-file scan."""
from __future__ import annotations

import json
import os
import struct
from pathlib import Path

import pytest

from hdb_extract.extractors.hotspots import (
    build_hotspots_inventory,
    parse_hot_bytes,
    parse_hot_file,
    write_hotspots_inventory,
)


def _synth_hot(rects: list[tuple[int, int, int, int, int, int, int]]) -> bytes:
    """Build a synthetic HSPT blob.

    Each tuple is ``(type_zorder, x_min, y_min, x_max, y_max, action1, action2)``.
    """
    buf = b"HSPT" + struct.pack("<I", len(rects))
    for tz, xmin, ymin, xmax, ymax, a1, a2 in rects:
        buf += struct.pack("<Ihhhh", tz, xmin, ymin, xmax, ymax)
        buf += struct.pack("<II", a1, a2)
    return buf


def test_parse_empty_hsp() -> None:
    rec = parse_hot_bytes(_synth_hot([]), scene_id=42, source="42.HOT")
    assert rec is not None
    assert rec["scene_id"] == 42
    assert rec["rect_count"] == 0
    assert rec["rects"] == []


def test_parse_two_rects_round_trip() -> None:
    rects = [
        (0, 10, 20, 30, 40, 1234, 0),
        (1, -32000, 0, 32000, 480, 5678, 9999),
    ]
    rec = parse_hot_bytes(_synth_hot(rects), scene_id=7)
    assert rec is not None
    assert rec["rect_count"] == 2
    r0 = rec["rects"][0]
    assert (r0["x_min"], r0["y_min"], r0["x_max"], r0["y_max"]) == (10, 20, 30, 40)
    assert r0["action_id_1"] == 1234
    assert r0["action_id_2"] == 0
    r1 = rec["rects"][1]
    assert r1["x_min"] == -32000   # signed i16
    assert r1["x_max"] == 32000


def test_reject_bad_header() -> None:
    assert parse_hot_bytes(b"NOPE" + b"\x00" * 4) is None


def test_reject_wrong_size() -> None:
    bad = b"HSPT" + struct.pack("<I", 2) + b"\x00" * 8   # claims 2 entries, only 8 B
    assert parse_hot_bytes(bad) is None


@pytest.mark.skipif(not os.environ.get("XFILES_XV_DIR"),
                     reason="requires XFILES_XV_DIR pointing at the game's XV directory")
def test_real_inventory_smoke(tmp_path: Path) -> None:
    xv = Path(os.environ["XFILES_XV_DIR"])
    if not xv.is_dir():
        pytest.skip("XFILES_XV_DIR is not a directory")
    out = tmp_path / "inv.json"
    stats = write_hotspots_inventory(xv, out)
    assert out.is_file()
    inv = json.loads(out.read_text(encoding="utf-8"))
    assert inv["stats"] == stats
    assert inv["stats"]["scenes_total"] >= 1
    assert inv["stats"]["rects_total"] >= 1
