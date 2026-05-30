"""navigation_targets smoke test."""
from __future__ import annotations

import json
import os
from pathlib import Path

import pytest

from hdb_extract.extractors.navigation_targets import (
    build_navigation_targets, write_navigation_targets,
)


_HDB = os.environ.get("XFILES_HDB")


@pytest.mark.skipif(not _HDB, reason="requires XFILES_HDB")
def test_basic_shape(tmp_path: Path) -> None:
    out = tmp_path / "nt.json"
    stats = write_navigation_targets(_HDB, out)
    data = json.loads(out.read_text(encoding="utf-8"))
    assert data["stats"] == stats
    # Real HDB ships 68 VCNav records — assert at least a handful.
    assert stats["count"] >= 10
    assert stats["with_label"] + stats["without_label"] == stats["count"]
    # Honesty: every entry carries certainty + source.
    for e in data["entries"]:
        assert e["certainty"] == "byte-direct"
        assert e["source"] == "HDB:VCNav-inline-label"
        assert e["payload_size"] >= 0


@pytest.mark.skipif(not _HDB, reason="requires XFILES_HDB")
def test_at_least_one_label_resolves_a_scene() -> None:
    nt = build_navigation_targets(_HDB)
    has_video_link = any(
        "Video" in (e["label"] or "") or "Node" in (e["label"] or "")
        for e in nt["entries"]
    )
    assert has_video_link, (
        "no VCNav record carries a Video/Node label; "
        "regression in the byte-direct walker"
    )
