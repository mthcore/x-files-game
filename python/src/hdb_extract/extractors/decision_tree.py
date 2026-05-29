"""Build the complete decision tree of the game.

Unifies multiple data sources :

  the research datasets     → 2,278 hotspots
  the research datasets    → 807 actions
  the research datasets     → 61 triggers
  the research datasets          → 670 game variables
  the research datasets     → 680 scenes
  the research datasets → conversation graph
  the research datasets             → email arc
  the research datasets            → 8 multiple endings

Plus the HDB itself :
  - production_paths.json (this project)
  - neoidlist.json
  - strings.json

Result : a single `decision_tree.json` that lets you navigate
scene → hotspots → actions → effects (variable updates) → triggers
→ conditions on variables → branches → endings.
"""
from __future__ import annotations

from typing import Any

from hdb_extract.container.container import HdbContainer
from hdb_extract.ext import (
    load_actions,
    load_conversations,
    load_email_tree,
    load_endings_map,
    load_gam_variables,
    load_gam_variables_semantics,
    load_hotspots,
    load_scene_encyclopedia,
    load_triggers,
)


def build_decision_tree(container: HdbContainer | None = None) -> dict[str, Any]:
    """Build the complete decision tree by joining all data sources."""
    hotspots = load_hotspots()
    actions = load_actions()
    triggers = load_triggers()
    variables = load_gam_variables()
    var_semantics = load_gam_variables_semantics()
    scenes = load_scene_encyclopedia()
    conversations = load_conversations()
    emails = load_email_tree()
    endings = load_endings_map()

    # Index hotspots by scene_id for quick lookup.
    hotspots_by_scene: dict[int, list[dict]] = {}
    for h in hotspots:
        hotspots_by_scene.setdefault(h["scene_id"], []).append(h)

    # Index actions by action_id.
    actions_by_id: dict[int, dict] = {a["action_id"]: a for a in actions}

    # Index triggers by scene scope.
    triggers_by_scene: dict[str, list[dict]] = {}
    for t in triggers:
        triggers_by_scene.setdefault(t["scene_scope"], []).append(t)

    # Index variable semantics by name.
    var_sem_by_name: dict[str, dict] = {}
    for s in var_semantics:
        name = s.get("variable_name") or s.get("name") or ""
        if name:
            var_sem_by_name[name] = s

    # Enrich each variable with its semantics.
    for v in variables:
        sem = var_sem_by_name.get(v["name"])
        if sem:
            v["semantics"] = sem

    # Build the per-scene rollup : scene + its hotspots + its triggers.
    def _norm(s: str) -> str:
        """Normalize : lowercase, strip spaces (since triggers use 'FieldOffice'
        but scenes use 'Field Office')."""
        return s.lower().replace(" ", "").replace("_", "")

    # Dedup scenes that map to the same normalized name. Keep the entry
    # with the most assets (the asset scan sometimes records both
    # "Secret Base" (89 vids) and "secret base" (1 vid) — duplicate).
    by_norm: dict[str, dict] = {}
    for s in scenes:
        key = _norm(s["scene_path"])
        if key not in by_norm:
            by_norm[key] = s
        elif s["n_xmv"] > by_norm[key]["n_xmv"]:
            by_norm[key] = s
    deduped_scenes = list(by_norm.values())
    deduped_scenes.sort(
        key=lambda x: -(x["n_xmv"] + x["n_amv"] + x["n_xtx"] + x["n_mus"])
    )

    enriched_scenes: list[dict] = []
    for s in deduped_scenes:
        scene_norm = _norm(s["scene_path"])
        scene_triggers = [
            t for t in triggers
            if _norm(t["scene_scope"]).startswith(scene_norm)
            or scene_norm.startswith(_norm(t["scene_scope"]))
        ]
        enriched_scenes.append({
            **s,
            "triggers": scene_triggers,
            "n_triggers": len(scene_triggers),
        })

    # Build action → hotspot → scene cross-refs.
    hotspots_by_action: dict[int, list[dict]] = {}
    for h in hotspots:
        for aid_key in ("action_id_1", "action_id_2"):
            aid = h.get(aid_key)
            if aid:
                hotspots_by_action.setdefault(aid, []).append({
                    "scene_id": h["scene_id"],
                    "hotspot_idx": h["hotspot_idx"],
                    "via": aid_key,
                })
    enriched_actions: list[dict] = []
    for a in actions:
        aid = a["action_id"]
        enriched_actions.append({
            **a,
            "hotspots_invoking": hotspots_by_action.get(aid, []),
        })

    return {
        "summary": {
            "scenes": len(enriched_scenes),
            "hotspots": len(hotspots),
            "actions": len(enriched_actions),
            "triggers": len(triggers),
            "variables": len(variables),
            "variables_with_semantics": sum(1 for v in variables if "semantics" in v),
            "conversations": len(conversations),
            "emails": len(emails),
            "endings": len(endings),
        },
        "scenes": enriched_scenes,
        "hotspots": hotspots,
        "actions": enriched_actions,
        "triggers": triggers,
        "variables": variables,
        "conversations": conversations,
        "emails": emails,
        "endings": endings,
    }
