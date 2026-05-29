"""Triggers JSON : clean byte-direct, no internal placeholders.

Once the wire format was established byte-direct, per-trigger placeholders
are no longer appropriate. The 61 trigger entries expose only their
byte-direct fields ; the trigger architecture is referenced once at
top-level, and decoded record instances live in out/json/decoded_records.json.
"""
import os
import pytest
from pathlib import Path

from hdb_extract.ext.decomp_intel import load_triggers, load_variables_full
from hdb_extract.extractors.triggers_full import build_triggers_full, enrich_trigger

DECOMP_INTEL = Path(os.environ.get("DECOMP_INTEL_DIR", "datasets/flow"))


@pytest.fixture(scope="module")
def triggers_full() -> dict:
    if not DECOMP_INTEL.exists():
        pytest.skip("format-analysis datasets not available")
    raw = load_triggers()
    vars_full = load_variables_full()
    return build_triggers_full(raw, vars_full)


def test_total_triggers(triggers_full):
    assert triggers_full["total"] == 61


def test_clean_byte_direct_fields(triggers_full):
    """Each trigger only has byte-direct fields. No internal placeholders."""
    for t in triggers_full["triggers"]:
        assert "trigger_id" in t
        assert "scene_scope" in t
        assert "action_label" in t
        assert "effect" in t
        assert "condition_hint" in t
        assert "vars_written" in t
        forbidden = {"_packed_in_hdb", "_g1_pending", "_decode_blocker",
                     "official_architecture", "source"}
        for k in forbidden:
            assert k not in t, f"forbidden key {k} in {t['trigger_id']}"


def test_architecture_referenced_once(triggers_full):
    """Architecture is documented at top level, not per-trigger."""
    assert "_architecture_reference" in triggers_full
    assert "_decoded_records_pointer" in triggers_full


def test_enrich_trigger_shape():
    sample = {
        "trigger_id": "bAIWong_ShowBadge",
        "scene_scope": "Wong",
        "action_label": "ShowBadge",
        "effect": "set bAIWong_ShowBadge = true",
        "condition_hint": "player completes action",
    }
    out = enrich_trigger(sample)
    assert out["trigger_id"] == "bAIWong_ShowBadge"
    assert out["vars_written"] == ["bAIWong_ShowBadge"]
    assert "_packed_in_hdb" not in str(out)
