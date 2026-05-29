"""CSV loaders for the auxiliary format-analysis datasets.

Each loader returns a list of dicts (or None if the CSV is unavailable).
The directory can be overridden via the env var `DECOMP_INTEL_DIR` for tests.
"""
from __future__ import annotations

import csv
import os
from pathlib import Path

DEFAULT_DECOMP_INTEL = Path(os.environ.get(
    "DECOMP_INTEL_DIR",
    "datasets/flow",
))


def _load_csv(name: str, base: Path | None = None) -> list[dict]:
    base = base or DEFAULT_DECOMP_INTEL
    p = base / name
    if not p.exists():
        return []
    with p.open(newline="", encoding="utf-8") as fp:
        return list(csv.DictReader(fp))


def load_hotspots(base: Path | None = None) -> list[dict]:
    """2,278 hotspots with coords + 2 action_ids each.

    Schema: scene_id, hotspot_idx, type, x_min, y_min, x_max, y_max,
    action_id_1, action_id_2.
    """
    rows = _load_csv("hotspots_inventory.csv", base)
    return [{
        "scene_id": int(r["scene_id"]),
        "hotspot_idx": int(r["hotspot_idx"]),
        "type": int(r["type"]),
        "coords": {
            "x_min": int(r["x_min"]), "y_min": int(r["y_min"]),
            "x_max": int(r["x_max"]), "y_max": int(r["y_max"]),
        },
        "action_id_1": int(r["action_id_1"]) if r["action_id_1"] else None,
        "action_id_2": int(r["action_id_2"]) if r["action_id_2"] else None,
    } for r in rows]


def load_actions(base: Path | None = None) -> list[dict]:
    """807 action_ids resolved with frequency + classification."""
    rows = _load_csv("action_ids_resolved.csv", base)
    return [{
        "action_id": int(r["action_id"]),
        "frequency": int(r["frequency"]),
        "scenes_using": int(r["scenes_using"]),
        "classification": r["classification"],
        "confidence": r["confidence"],
    } for r in rows]


def load_triggers(base: Path | None = None) -> list[dict]:
    """61 triggers with scope + action_label + effect + condition_hint."""
    rows = _load_csv("triggers_instances.csv", base)
    return [{
        "trigger_id": r["trigger_id"],
        "scene_scope": r["scene_scope"],
        "action_label": r["action_label"],
        "effect": r["effect"],
        "condition_hint": r["condition_hint"],
    } for r in rows]


def load_gam_variables(base: Path | None = None) -> list[dict]:
    """670 game state variable names."""
    rows = _load_csv("gam_variables.csv", base)
    return [{
        "name": r["variable_name"],
        "type_prefix": r["type_prefix"],
        "occurrences": int(r["occurrences"]),
    } for r in rows]


def load_gam_variables_semantics(base: Path | None = None) -> list[dict]:
    """Semantic interpretation of game state variables (when known).

    Schema : variable_name, type_prefix, occurrences, cpp_type, scope,
    role_category, confidence.
    """
    rows = _load_csv("gam_variables_semantics.csv", base)
    out: list[dict] = []
    for r in rows:
        try:
            occ = int(r.get("occurrences") or 0)
        except (ValueError, TypeError):
            occ = 0
        out.append({
            "name": r.get("variable_name") or "",
            "type_prefix": r.get("type_prefix") or "",
            "occurrences": occ,
            "cpp_type": r.get("cpp_type") or "",
            "scope": r.get("scope") or "",
            "role_category": r.get("role_category") or "",
            "confidence": r.get("confidence") or "",
        })
    return out


def load_variables_full(base: Path | None = None) -> list[dict]:
    """Merge gam_variables.csv (counts) with gam_variables_semantics.csv.

    Returns 670 entries each with name, type, scope, role, confidence.
    """
    base_vars = load_gam_variables(base)
    sem = {s["name"]: s for s in load_gam_variables_semantics(base)}
    out: list[dict] = []
    for v in base_vars:
        s = sem.get(v["name"], {})
        out.append({
            "name": v["name"],
            "type_prefix": v["type_prefix"],
            "occurrences": v["occurrences"],
            "cpp_type": s.get("cpp_type") or _infer_cpp_type(v["type_prefix"]),
            "scope": s.get("scope") or "Title",
            "role_category": s.get("role_category") or "",
            "confidence": s.get("confidence") or "",
        })
    return out


def _infer_cpp_type(prefix: str) -> str:
    """Infer cpp type from Hungarian prefix when explicit semantics missing."""
    return {
        "b": "bool",
        "i": "int32_t",
        "n": "uint32_t",
        "c": "uint8_t",
        "l": "int32_t",
        "s": "char*",
        "sc": "const char*",
        "S": "struct",
        "I": "scope_prefix",
    }.get(prefix, "unknown")


def load_scene_encyclopedia(base: Path | None = None) -> list[dict]:
    """680 scenes with asset counts + AI checkpoints."""
    rows = _load_csv("scene_encyclopedia.csv", base)
    return [{
        "scene_path": r["scene_path"],
        "n_xmv": int(r["n_xmv"]),
        "n_amv": int(r["n_amv"]),
        "n_mus": int(r["n_mus"]),
        "n_xtx": int(r["n_xtx"]),
        "video_dur_s": int(r["video_dur_s"]),
        "total_bytes": int(r["total_bytes"]),
        "n_ai_checkpoints": int(r["n_ai_checkpoints"]),
        "ai_checkpoints": [c.strip() for c in r["ai_checkpoints"].split(";") if c.strip()],
        "n_hot_files": int(r["n_hot_files"]),
    } for r in rows]


def load_conversations(base: Path | None = None) -> list[dict]:
    """Conversation instances with prompt + response IDs + next link."""
    rows = _load_csv("conversations_instances.csv", base)
    return [{
        "conversation_id": int(r["conversation_id"]),
        "speaker_topic_id": int(r["speaker_topic_id"]),
        "prompt_string_id": int(r["prompt_string_id"]),
        "response_id_1": int(r["response_id_1"]) if r["response_id_1"] else None,
        "response_id_2": int(r["response_id_2"]) if r["response_id_2"] else None,
        "next_conversation_id": int(r["next_conversation_id"]),
        "flags": int(r["flags"]),
        "source_offset": r["source_offset"],
    } for r in rows]


def load_email_tree(base: Path | None = None) -> list[dict]:
    """Email tree : sender / recipient / day / next email link."""
    return _load_csv("email_tree.csv", base)


def load_endings_map(base: Path | None = None) -> list[dict]:
    """8 multiple endings + conditions."""
    return _load_csv("endings_map.csv", base)
