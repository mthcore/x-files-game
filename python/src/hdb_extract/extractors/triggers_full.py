"""Triggers output — clean & honest.

Each of the 61 triggers exposes only its byte-direct fields from
`the research datasets`. The W20.4 architecture is
referenced once at the top for context, NOT injected as placeholders
inside each trigger.
"""
from __future__ import annotations

from typing import Any


def enrich_trigger(raw: dict) -> dict:
    """Return the clean byte-direct record for one trigger."""
    return {
        "trigger_id": raw["trigger_id"],
        "scene_scope": raw["scene_scope"],
        "action_label": raw["action_label"],
        "effect": raw["effect"],
        "condition_hint": raw["condition_hint"],
        "vars_written": [raw["trigger_id"]],
    }


def build_triggers_full(
    triggers: list[dict],
    variables_with_sem: list[dict] | None = None,
) -> dict[str, Any]:
    """Build clean triggers JSON without any placeholders inside each entry."""
    return {
        "_source": "the research datasets (byte-direct scan of HDB)",
        "_architecture_reference": (
            "Each VCTrigger record (class_id 0x51) holds : "
            "Conditions[<=8] polymorphic, Actions[<=12] polymorphic, "
            "Targets[<=4] 1-byte. Full grammar in out/json/trigger_system.json"
        ),
        "_decoded_records_pointer": (
            "Concrete decoded VCTrigger record instances are in "
            "out/json/decoded_records.json (filter by_class['VCTrigger']). "
            "Wire format established byte-direct from the on-disk structure ; "
            "bit-reversal + BE->LE byte-swap on the typed reads."
        ),
        "total": len(triggers),
        "triggers": [enrich_trigger(t) for t in triggers],
    }
