"""B-tree page inventory smoke test."""
from __future__ import annotations

import json
import os
from pathlib import Path

import pytest

from hdb_extract.extractors.btree_pages import (
    PAGE_SIZE,
    build_btree_inventory,
    write_btree_inventory,
)


_HDB = os.environ.get("XFILES_HDB")


@pytest.mark.skipif(not _HDB, reason="requires XFILES_HDB")
def test_inventory_shape(tmp_path: Path) -> None:
    out = tmp_path / "inv.json"
    stats = write_btree_inventory(_HDB, out)
    inv = json.loads(out.read_text(encoding="utf-8"))
    assert inv["stats"] == stats
    assert stats["page_size"] == PAGE_SIZE
    assert stats["pages_total"] >= 100
    assert stats["tag_kind_combinations"] >= 1

    # Every entry carries the required certainty + shape fields.
    for e in inv["distribution"]:
        assert e["certainty"] == "byte-direct"
        assert e["semantic_meaning"]["certainty"] == "undetermined"
        assert "page_count" in e and e["page_count"] >= 1
        assert "marker_per_page" in e
        assert e["marker_per_page"]["min"] <= e["marker_per_page"]["max"]


@pytest.mark.skipif(not _HDB, reason="requires XFILES_HDB")
def test_btree_tags_present() -> None:
    inv = build_btree_inventory(_HDB)
    tags = {e["tag_hex"] for e in inv["by_tag_summary"]}
    # B-tree tags should all show up in a real HDB.
    for needed in ("0xc0", "0xc2", "0xc3", "0xd2"):
        assert needed in tags, f"{needed} missing"
