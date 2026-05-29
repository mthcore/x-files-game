"""Integration: build the unified game_definition.json (Schema v1).

Skipped unless ``XFILES_HDB`` is set. Asserts:
* every required top-level section is present
* every leaf wrapped with a ``certainty`` marker uses the closed vocabulary
* the byte-direct percentage is in [0, 100]
* the ``source_hdb_sha256`` matches the actual file's SHA-256

The test writes the artifact to a temporary file by default and also
exercises the in-memory ``build_game_definition`` API.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

import pytest

from hdb_extract.extractors.game_definition import (
    ALLOWED_CERTAINTY,
    ALLOWED_REASONS,
    build_game_definition,
    verify_certainty,
    write_game_definition,
)


HDB_PATH = Path(os.environ.get("XFILES_HDB", "XFILES.HDB"))

REQUIRED_TOP_LEVEL_SECTIONS = (
    "_meta",
    "scenes",
    "triggers",
    "conversations",
    "actions_vocabulary",
    "variables",
    "flow",
    "dialogues",
)


@pytest.fixture(scope="module")
def gamedef() -> dict:
    if not HDB_PATH.is_file():
        pytest.skip("XFILES_HDB not set or file not found")
    return build_game_definition(HDB_PATH)


def test_required_sections_present(gamedef):
    """Every required top-level section must be present (schema §10)."""
    missing = [s for s in REQUIRED_TOP_LEVEL_SECTIONS if s not in gamedef]
    assert not missing, f"missing top-level sections: {missing}"


def test_meta_source_hdb_sha256_matches(gamedef):
    """``_meta.source_hdb_sha256`` must equal the actual file's SHA-256."""
    expected = hashlib.sha256(HDB_PATH.read_bytes()).hexdigest()
    sha = gamedef["_meta"]["source_hdb_sha256"]
    assert sha["certainty"] == "byte-direct"
    assert sha["value"] == expected


def test_meta_source_hdb_size_matches(gamedef):
    """The wrapped HDB size must equal the actual file size on disk."""
    expected = HDB_PATH.stat().st_size
    size = gamedef["_meta"]["source_hdb_size"]
    assert size["certainty"] == "byte-direct"
    assert size["value"] == expected


def test_byte_direct_pct_in_range(gamedef):
    """``_meta.byte_direct_pct`` must be in [0.0, 100.0]."""
    pct = gamedef["_meta"]["byte_direct_pct"]
    assert pct["certainty"] == "byte-direct"
    v = pct["value"]
    assert isinstance(v, (int, float))
    assert 0.0 <= float(v) <= 100.0


def test_every_certainty_in_allowed_vocab(gamedef):
    """Recursive scan: every node with a ``certainty`` field uses the
    closed vocabulary, and every ``undetermined`` carries a known reason."""
    audit = verify_certainty(gamedef)
    assert audit["ok"], f"certainty violations: {audit['violations'][:10]}"
    # Sanity : we touch all three buckets at least once.
    counts = audit["certainty_counts"]
    for c in ("byte-direct", "undetermined"):
        assert counts.get(c, 0) > 0, f"no fields with certainty={c}"


def test_field_counts_consistent(gamedef):
    """``_meta.field_counts`` totals must reconcile."""
    fc = gamedef["_meta"]["field_counts"]
    total = fc["total"]["value"]
    bd = fc["byte_direct"]["value"]
    ud = fc["undetermined"]["value"]
    wt = fc["walkthrough"]["value"]
    assert total == bd + ud + wt, (
        f"sum mismatch: total={total} bd={bd} ud={ud} wt={wt}"
    )
    assert total > 0


def test_flow_has_29_steps(gamedef):
    """The canonical walkthrough exposes 29 steps."""
    flow = gamedef["flow"]
    steps_total = sum(len(d["scenes"]) for d in flow["days"])
    # Allow >= 29 in case days carry extra epilogue rows.
    assert steps_total >= 29, f"only {steps_total} flow steps emitted"
    assert flow["total_steps"]["certainty"] == "byte-direct"


def test_scenes_carry_byte_direct_video_stats(gamedef):
    """Every scene's video_stats must be byte-direct."""
    for loc, sc in gamedef["scenes"].items():
        for k in ("views", "clips_xmv", "video_seconds"):
            wrapped = sc["video_stats"][k]
            assert wrapped["certainty"] == "byte-direct", (
                f"scene {loc!r} stat {k} not byte-direct"
            )


def test_triggers_byte_direct(gamedef):
    """All triggers must carry a byte-direct trigger_id."""
    for t in gamedef["triggers"]:
        assert t["trigger_id"]["certainty"] == "byte-direct"
        # The runtime-only relation byte stays undetermined per schema §3.
        assert t["relation_meaning"]["certainty"] == "undetermined"
        assert (t["relation_meaning"]["reason"]
                == "relation-byte-meaning-runtime-only")


def test_actions_vocabulary_candidate_effect_undetermined(gamedef):
    """Every opcode's candidate_effect must be undetermined (schema §5)."""
    for op, entry in gamedef["actions_vocabulary"]["opcodes"].items():
        assert entry["candidate_effect"]["certainty"] == "undetermined"
        assert (entry["candidate_effect"]["reason"]
                == "opcode-to-verb-table-not-in-public-format")


def test_round_trip_via_write(tmp_path):
    """``write_game_definition`` produces valid JSON we can reload."""
    if not HDB_PATH.is_file():
        pytest.skip("XFILES_HDB not set or file not found")
    out = tmp_path / "game_definition.json"
    gd = write_game_definition(HDB_PATH, out)
    assert out.exists()
    reloaded = json.loads(out.read_text(encoding="utf-8"))
    assert reloaded["_meta"]["source_hdb_sha256"]["value"] == (
        gd["_meta"]["source_hdb_sha256"]["value"]
    )


def test_closed_reason_vocabulary_unchanged():
    """Guard against accidental expansion of the closed reason vocabulary."""
    assert ALLOWED_REASONS == {
        "field-not-present-in-payload",
        "u32-not-in-dll-string-range-and-not-in-extended-index",
        "handle-not-in-extended-index",
        "scene-binding-not-resolved-byte-direct",
        "relation-byte-meaning-runtime-only",
        "opcode-to-verb-table-not-in-public-format",
        "no-authoring-annotation-in-GAM",
        "value-only-known-from-walkthrough",
    }
    assert ALLOWED_CERTAINTY == {"byte-direct", "undetermined", "walkthrough"}
