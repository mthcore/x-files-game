"""Smoke tests for the playthrough_trace extractor."""
from __future__ import annotations

import json
import os
from pathlib import Path

import pytest

from hdb_extract.extractors.playthrough_trace import (
    build_playthrough_trace,
    write_playthrough_trace,
)


_HDB = os.environ.get("XFILES_HDB")
_OSS_ROOT = Path(__file__).resolve().parents[2].parent  # OSS/
_GAME_DEF = _OSS_ROOT / "examples" / "outputs" / "game_definition.json"


@pytest.mark.skipif(not _HDB or not _GAME_DEF.is_file(),
                     reason="requires XFILES_HDB + a generated game_definition.json")
def test_trace_29_steps_split(tmp_path: Path) -> None:
    game_def = json.loads(_GAME_DEF.read_text(encoding="utf-8"))
    trace = build_playthrough_trace(game_def)

    # canonical flow has exactly 29 steps
    assert trace["stats"]["steps_total"] == 29

    # the byte-direct catalog + walkthrough-only split should add up
    bd = trace["stats"]["byte_direct_catalog_steps"]
    wo = trace["stats"]["walkthrough_only_steps"]
    assert bd + wo == 29
    assert bd >= 1 and wo >= 1, "both buckets should be non-empty"


@pytest.mark.skipif(not _HDB or not _GAME_DEF.is_file(),
                     reason="requires XFILES_HDB + a generated game_definition.json")
def test_every_field_has_certainty(tmp_path: Path) -> None:
    game_def = json.loads(_GAME_DEF.read_text(encoding="utf-8"))
    trace = build_playthrough_trace(game_def)
    for step in trace["steps"]:
        for required in ("step", "day", "location", "actions_walkthrough"):
            assert "certainty" in step[required]
        for t in step["triggers_in_scope"]:
            assert t["certainty"] == "byte-direct"
        for c in step["conversations_in_scope"]:
            assert c["certainty"] == "byte-direct"


@pytest.mark.skipif(not _HDB or not _GAME_DEF.is_file(),
                     reason="requires XFILES_HDB + a generated game_definition.json")
def test_write_round_trip(tmp_path: Path) -> None:
    out = tmp_path / "trace.json"
    stats = write_playthrough_trace(_GAME_DEF, out)
    assert out.is_file()
    loaded = json.loads(out.read_text(encoding="utf-8"))
    assert loaded["stats"] == stats
    assert "steps" in loaded
    assert len(loaded["steps"]) == 29
