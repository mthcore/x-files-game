"""scenes_with_hotspots cross-link smoke test."""
from __future__ import annotations

import json
import os
from pathlib import Path

import pytest

from hdb_extract.extractors.scenes_with_hotspots import (
    build_scenes_with_hotspots, write_scenes_with_hotspots,
)


_OSS_ROOT = Path(__file__).resolve().parents[2].parent
_SAM = _OSS_ROOT / "examples" / "outputs" / "scene_asset_map.json"
_INV = _OSS_ROOT / "examples" / "outputs" / "hotspots_inventory.json"


@pytest.mark.skipif(
    not _SAM.is_file() or not _INV.is_file(),
    reason="requires both scene_asset_map.json and hotspots_inventory.json",
)
def test_partitions_are_disjoint_and_total(tmp_path: Path) -> None:
    out = tmp_path / "sw.json"
    stats = write_scenes_with_hotspots(_SAM, _INV, out)
    data = json.loads(out.read_text(encoding="utf-8"))
    assert data["stats"] == stats

    interactive_ids = {e["scene_id"] for e in data["interactive"]}
    cinematic_ids   = {e["scene_id"] for e in data["cinematic_only"]}
    orphan_ids      = {e["scene_id"] for e in data["orphan_hot_files"]}

    # interactive + cinematic_only partition the labeled set; both disjoint
    # from each other and from the orphans.
    assert interactive_ids.isdisjoint(cinematic_ids)
    assert interactive_ids.isdisjoint(orphan_ids)
    assert cinematic_ids.isdisjoint(orphan_ids)

    # Headline numbers are coherent.
    assert (stats["interactive"] + stats["cinematic_only"]
            == stats["scenes_total"])
    assert stats["scenes_total"] >= 1
    assert stats["interactive"] >= 1


@pytest.mark.skipif(
    not _SAM.is_file() or not _INV.is_file(),
    reason="requires both source artifacts",
)
def test_every_entry_carries_certainty() -> None:
    data = build_scenes_with_hotspots(_SAM, _INV)
    for bucket in ("interactive", "cinematic_only", "orphan_hot_files"):
        for e in data[bucket][:50]:
            assert e["certainty"] == "byte-direct"
            assert "source" in e
