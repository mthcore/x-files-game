"""Per-scene structured JSON (replaces the Markdown scripts).

Each scene produces a single `scenes/scene_<name>.json` file describing
the complete gameplay flow in structured form, machine-readable for any
re-implementation engine.

Top-level JSON shape :

    {
      "name": "Wong",
      "summary": {...},
      "checkpoints": [...],
      "triggers": [...],
      "flow": [
        {
          "order_index": 0,
          "sequence": "009",
          "label": "Sequence 009 — daytime arrival (no boat)",
          "viewpoints": {
            "11w": [{"file_id": "12XN/32365.xmv", ...}, ...],
            "2w":  [...]
          }
        },
        {
          "order_index": 1,
          "sequence": "010",
          ...
        }
      ],
      "non_video_assets": [...]
    }

The `flow` array is **ordered** by `order_index` :
the gameplay starts at order_index=0 (lowest sequence), progresses
to the next sequence when the player completes its required actions
(see triggers), and ends when the highest-order sequence is reached
or a transition fires.
"""
from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any


def _slug(s: str) -> str:
    return re.sub(r"[^a-zA-Z0-9]+", "_", s).strip("_").lower()


_RE_NUMBERED = re.compile(
    r"^(?P<seq>\d{2,3})[_\s-]+(?P<rest>.+?)\.(mov|xmv|amv|mus|xtx|pff|dmv)$",
    re.IGNORECASE,
)
_RE_VIEWPOINT = re.compile(r"(?P<view>\d{1,2}[neswNESW]{1,2})")


def _parse_production(name: str) -> dict[str, Any]:
    out: dict[str, Any] = {"raw": name}
    m = _RE_NUMBERED.match(name)
    if m:
        out["seq"] = m.group("seq")
        rest = m.group("rest")
    else:
        rest = name.rsplit(".", 1)[0]
    out["rest"] = rest
    vm = _RE_VIEWPOINT.search(rest)
    if vm:
        out["viewpoint"] = vm.group("view").lower()
    rl = rest.lower()
    states: list[str] = []
    if "no_boat" in rl or "noboat" in rl:
        states.append("no_boat")
    elif "boat" in rl:
        states.append("boat")
    if "day" in rl:
        states.append("day")
    if "night" in rl:
        states.append("night")
    take_m = re.search(r"tk(\d)", rl)
    if take_m:
        out["take"] = int(take_m.group(1))
    if states:
        out["states"] = states
    return out


def _build_sequences(productions: list[dict]) -> tuple[list[dict], list[dict]]:
    """Return (flow_ordered, non_video_assets)."""
    by_seq: dict[str, list[dict]] = {}
    non_video: list[dict] = []
    for p in productions:
        info = _parse_production(p["production_name"])
        if p["asset_type"] not in ("xmv", "dmv"):
            non_video.append({
                "file": f"{p['storage_dir']}/{p['file_id']}.{p['asset_type']}",
                "production_name": p["production_name"],
                "asset_type": p["asset_type"],
                "size_bytes": p["size_bytes"],
            })
            continue
        seq = info.get("seq", "no_seq")
        by_seq.setdefault(seq, []).append({**p, **info})

    flow: list[dict] = []
    for order_index, seq in enumerate(sorted(by_seq.keys())):
        items = by_seq[seq]
        # Group by viewpoint within the sequence.
        viewpoints: dict[str, list[dict]] = {}
        for it in items:
            vp = it.get("viewpoint", "none")
            viewpoints.setdefault(vp, []).append({
                "file": f"{it['storage_dir']}/{it['file_id']}.{it['asset_type']}",
                "production_name": it["production_name"],
                "asset_type": it["asset_type"],
                "size_bytes": it["size_bytes"],
                "duration_s": it["duration_s"],
                "states": it.get("states", []),
                "take": it.get("take"),
                "video_codec": it.get("video_codec", ""),
                "video_dim": it.get("video_dim", ""),
            })
        # Sort viewpoints by their numeric position when possible.
        sorted_vps = dict(sorted(viewpoints.items(),
                                 key=lambda kv: _viewpoint_sort_key(kv[0])))
        # Within each viewpoint, sort by state (default first, then variants).
        for vp in sorted_vps:
            sorted_vps[vp].sort(key=lambda v: (len(v["states"]), v["states"]))
        flow.append({
            "order_index": order_index,
            "sequence": seq,
            "label": _label_for_seq(seq, items),
            "n_videos": len(items),
            "viewpoints": sorted_vps,
        })
    return flow, non_video


def _viewpoint_sort_key(vp: str) -> tuple:
    """Sort viewpoints by numeric position then by cardinal direction.
    e.g. '1n', '2w', '11se', '12w'.
    """
    if vp == "none":
        return (999, "")
    m = re.match(r"(\d+)([neswNESW]{1,2})", vp)
    if m:
        return (int(m.group(1)), m.group(2).lower())
    return (998, vp)


def _label_for_seq(seq: str, items: list[dict]) -> str:
    """Generate a short label describing what this sequence contains."""
    if seq == "no_seq":
        return "Unsequenced videos"
    # Look for common state hints.
    states_seen: set[str] = set()
    for it in items:
        for s in it.get("states", []):
            states_seen.add(s)
    parts = [f"Sequence {seq}"]
    if "day" in states_seen and "night" not in states_seen:
        parts.append("daytime")
    if "night" in states_seen and "day" not in states_seen:
        parts.append("nighttime")
    if "boat" in states_seen and "no_boat" not in states_seen:
        parts.append("boat present")
    if "no_boat" in states_seen and "boat" not in states_seen:
        parts.append("boat absent")
    return " — ".join(parts)


def build_scene_json(
    scene: dict,
    productions: list[dict],
    hotspots_for_scene: list[dict] | None = None,
    var_semantics: dict[str, dict] | None = None,
    full_triggers: list[dict] | None = None,
) -> dict:
    name = scene["scene_path"]
    flow, non_video = _build_sequences(productions)
    var_semantics = var_semantics or {}

    checkpoints = []
    for cp in scene.get("ai_checkpoints", []):
        var = f"bAI{name.replace(' ', '')}_{cp}"
        sem = var_semantics.get(var)
        cp_record = {
            "variable": var,
            "set_by_action": cp,
            "type": "bool",  # always bool by Hungarian convention for bAI*
        }
        if sem:
            if sem.get("scope"):
                cp_record["scope"] = sem["scope"]
            if sem.get("role_category"):
                cp_record["role"] = sem["role_category"]
        checkpoints.append(cp_record)

    # If we got the full enriched triggers, prefer those (richer structure).
    full_by_id: dict[str, dict] = {}
    if full_triggers:
        for ft in full_triggers:
            full_by_id[ft["trigger_id"]] = ft

    triggers_struct = []
    for t in scene.get("triggers", []):
        ft = full_by_id.get(t["trigger_id"])
        if ft:
            triggers_struct.append(ft)
            continue
        triggers_struct.append({
            "trigger_id": t["trigger_id"],
            "fires_when": {
                "player_action": t["action_label"],
                "in_scene": t["scene_scope"],
                "condition_hint": t["condition_hint"],
            },
            "effect": t["effect"],
        })

    return {
        "name": name,
        "summary": {
            "n_videos_xmv": scene["n_xmv"],
            "n_audio_amv": scene["n_amv"],
            "n_music_mus": scene["n_mus"],
            "n_text_xtx": scene["n_xtx"],
            "total_bytes": scene["total_bytes"],
            "video_duration_s": scene["video_dur_s"],
            "video_duration_min": scene["video_dur_s"] // 60,
            "hotspot_files": scene["n_hot_files"],
            "n_productions_mapped": len(productions),
            "n_checkpoints": len(checkpoints),
            "n_triggers": len(triggers_struct),
        },
        "checkpoints": checkpoints,
        "triggers": triggers_struct,
        "hotspots": hotspots_for_scene or [],
        "flow": flow,
        "non_video_assets": non_video,
        "reimplementation_notes": [
            "Load assets from XV/, XN/, XG/, XT/, XS/ as needed",
            "Start at flow[0] (lowest order_index sequence) with default state",
            "Each hotspot has 2 action_ids — fire trigger matching the action",
            "Variants in a viewpoint differ by 'states' (boat/no_boat, day/night, take)",
            "Pick the matching state set when playing a viewpoint video",
            "Transition to the next scene when navigation hotspot fires",
        ],
    }


def write_scenes_json(
    tree: dict,
    out_dir: Path,
    *,
    scene_asset_map: dict[str, list[dict]] | None = None,
    var_semantics: dict[str, dict] | None = None,
    full_triggers: list[dict] | None = None,
) -> dict:
    """Write per-scene JSON + return summary stats."""
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    scene_asset_map = scene_asset_map or {}
    var_semantics = var_semantics or {}
    full_triggers = full_triggers or []

    index_entries = []
    for s in tree["scenes"]:
        productions = scene_asset_map.get(s["scene_path"], [])
        scene_obj = build_scene_json(
            s, productions,
            var_semantics=var_semantics,
            full_triggers=full_triggers,
        )
        slug = _slug(s["scene_path"])
        out_file = out_dir / f"scene_{slug}.json"
        out_file.write_text(
            json.dumps(scene_obj, indent=2, ensure_ascii=False),
            encoding="utf-8",
        )
        index_entries.append({
            "scene": s["scene_path"],
            "file": f"scene_{slug}.json",
            "n_videos": s["n_xmv"],
            "n_productions": len(productions),
            "n_checkpoints": scene_obj["summary"]["n_checkpoints"],
            "n_triggers": scene_obj["summary"]["n_triggers"],
            "n_flow_steps": len(scene_obj["flow"]),
            "duration_min": s["video_dur_s"] // 60,
        })

    index_obj = {
        "total_scenes": len(index_entries),
        "scenes": index_entries,
        "structure_notes": {
            "flow": "Each scene has an ordered `flow` array. order_index "
                    "starts at 0 = the lowest sequence number. Sequences "
                    "represent gameplay phases ; viewpoints within a "
                    "sequence are the Myst-style 360° nodes (1N, 2W, "
                    "3E, ...). Variants in each viewpoint differ by "
                    "state (boat/no_boat, day/night).",
            "checkpoints": "Game state flags this scene can flip "
                           "(`bAI<Scene>_<Action>`).",
            "triggers": "When a player action completes in a given scene, "
                        "set the named variable.",
        },
    }
    (out_dir / "00_index.json").write_text(
        json.dumps(index_obj, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    return index_obj
