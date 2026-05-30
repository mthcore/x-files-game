"""scene_asset_map.json byte-direct extractor smoke tests."""
from __future__ import annotations

import json
import os
from pathlib import Path

import pytest

from hdb_extract.extractors.scene_asset_map import (
    build_scene_asset_map, write_scene_asset_map,
)


_HDB = os.environ.get("XFILES_HDB")


@pytest.mark.skipif(not _HDB, reason="requires XFILES_HDB")
def test_map_basic_shape(tmp_path: Path) -> None:
    out = tmp_path / "sm.json"
    stats = write_scene_asset_map(_HDB, out)
    sm = json.loads(out.read_text(encoding="utf-8"))
    assert sm["stats"] == stats
    assert stats["entries_total"] >= 100, (
        f"expected at least 100 inline labels, got {stats['entries_total']}"
    )
    # Both XV and XN paths should show up in a real HDB.
    assert "XV" in stats["by_asset_dir"]
    # The canonical 10-node story should be represented.
    assert any(int(n) == 1 for n in stats["by_node"]), "Node 1 missing"
    # Honesty: every entry carries certainty + source.
    for e in sm["entries"][:50]:
        assert e["certainty"] == "byte-direct"
        assert e["source"] == "HDB:inline-label"


@pytest.mark.skipif(not _HDB, reason="requires XFILES_HDB")
def test_known_logo_present() -> None:
    sm = build_scene_asset_map(_HDB)
    game = [e for e in sm["entries"] if e["kind"] == "game"]
    names = {e["scene_name"] for e in game}
    # The VCLogo + a couple of studio logos must show up by name.
    assert "VCLogo" in names, f"VCLogo missing in game entries: {names}"


@pytest.mark.skipif(not _HDB, reason="requires XFILES_HDB")
def test_node1_field_office_resolves() -> None:
    sm = build_scene_asset_map(_HDB)
    fo1 = [e for e in sm["entries"]
            if e["kind"] == "scene"
            and e["node"] == 1
            and e["location"] == "Field Office"]
    assert len(fo1) >= 10, (
        f"expected many Node 1 / Field Office entries, got {len(fo1)}"
    )
    # asset_ids should be a sorted-ish set of positive integers.
    ids = sorted(e["asset_id"] for e in fo1)
    assert ids[0] >= 1 and ids[0] < ids[-1]
