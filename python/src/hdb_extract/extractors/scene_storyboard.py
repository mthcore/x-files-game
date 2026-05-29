"""Generate a narrative storyboard for each scene.

Unlike the per-scene "data dump" MD/JSON, the storyboard reads like a
movie script : entry → setup → available actions → conversations → exits.
It threads together :

  - scene_encyclopedia (overview + AI checkpoints + assets)
  - triggers (scripts that fire on actions)
  - variables_full (game state vars + semantics)
  - production_paths (videos with descriptive names + viewpoints)
  - hotspots_inventory (clickable regions with action_ids)
  - action_ids_resolved (action catalogue with frequency + classification)
  - UDFs (89 C++ handlers like SetAIList, SendEmail)
  - conversations (20 dialog nodes)
"""
from __future__ import annotations

import csv
import re
from collections import defaultdict
from pathlib import Path
from typing import Any

from hdb_extract.ext import (
    load_actions,
    load_conversations,
    load_hotspots,
    load_scene_asset_map,
    load_triggers,
    load_variables_full,
)


def _slug(s: str) -> str:
    return re.sub(r"[^a-zA-Z0-9]+", "_", s).strip("_").lower()


def _humanize_action(label: str, scene: str) -> str:
    """Turn 'ShowBadge' / 'BeginTalkArley' into readable English."""
    if label.startswith("BeginTalk"):
        return f"Initiates conversation with **{label[len('BeginTalk'):]}**"
    if label.startswith("Talk2"):
        return f"Continues talking to **{label[len('Talk2'):]}**"
    if label.startswith("ShowBadge"):
        return "Flashes FBI badge"
    if label.startswith("See"):
        return f"Notices / observes **{label[len('See'):]}**"
    if label.startswith("Got"):
        return f"Picks up / obtains **{label[len('Got'):]}**"
    if label.startswith("Searched"):
        return f"Searches **{label[len('Searched'):]}**"
    if label.startswith("Revived"):
        return f"Revives **{label[len('Revived'):]}**"
    if label.startswith("Filed"):
        return f"Files **{label[len('Filed'):]}**"
    if "Iso" in label:
        return f"Operates iso chamber : **{label}**"
    if "Computer" in label:
        return f"Accesses computer : **{label}**"
    if "LifeSupport" in label:
        return "Toggles life-support system"
    if "Door" in label:
        return f"Opens/closes door : **{label}**"
    return f"Completes action **{label}**"


def _classify_video_role(production_name: str) -> str:
    """Heuristically classify a video : navigation / loop / scripted shot."""
    pn = production_name.lower()
    if "nav" in pn or "wongnav" in pn:
        return "navigation"
    if "loop" in pn:
        return "loop"
    if any(c in pn for c in ("intro", "outro", "begin")):
        return "intro/outro"
    if any(c in pn for c in ("badge", "flash")):
        return "scripted shot"
    return "viewpoint variant"


def _parse_production_info(name: str) -> dict[str, Any]:
    """Extract sequence + viewpoint + states from a production name."""
    info: dict[str, Any] = {"raw": name}
    m = re.match(r"^(\d{2,3})[_\s-]+(.+)$", name)
    if m:
        info["seq"] = m.group(1)
    vm = re.search(r"(\d{1,2}[neswNESW]{1,2})", name)
    if vm:
        info["viewpoint"] = vm.group(1).lower()
    rl = name.lower()
    states: list[str] = []
    if "no_boat" in rl or "noboat" in rl:
        states.append("boat absent")
    elif "boat" in rl:
        states.append("boat present")
    if "day" in rl:
        states.append("daytime")
    if "night" in rl:
        states.append("nighttime")
    if states:
        info["state"] = " + ".join(states)
    return info


def build_scene_storyboard(
    scene: dict,
    triggers: list[dict],
    variables_sem: dict[str, dict],
    productions: list[dict],
    actions_catalogue: list[dict],
    conversations: list[dict],
) -> str:
    """Build a narrative Markdown storyboard for one scene."""
    name = scene["scene_path"]
    n_videos = scene["n_xmv"]
    n_amv = scene["n_amv"]
    n_mus = scene["n_mus"]
    n_xtx = scene["n_xtx"]
    duration = scene["video_dur_s"]
    checkpoints = scene.get("ai_checkpoints", [])
    scene_triggers = scene.get("triggers", [])

    # Lookup action freq by id
    action_by_id = {a["action_id"]: a for a in actions_catalogue}

    # Group videos by sequence + viewpoint
    by_seq: dict[str, list[dict]] = defaultdict(list)
    for p in productions:
        info = _parse_production_info(p["production_name"])
        by_seq[info.get("seq", "no_seq")].append({**p, **info})

    # Identify intro/setup video (first sequence)
    seq_keys = sorted(by_seq.keys())
    first_seq = by_seq.get(seq_keys[0]) if seq_keys else []

    lines = [
        f"# {name} — Narrative storyboard",
        "",
        "_This document describes the flow of scene **{name}** "
        "as an interactive film script. It is generated from "
        "the byte-direct HDB data + native ground truth._".format(name=name),
        "",
        "---",
        "",
        "## I. Setup",
        "",
        f"**{name}** is a Myst-style 360° scene with **{n_videos} "
        f"videos** (total duration **{duration // 60} minutes**), "
        f"plus {n_amv} audio tracks, {n_mus} music tracks and {n_xtx} text scripts.",
        "",
    ]

    if first_seq:
        lines += ["### Player entry", ""]
        nav_videos = [v for v in first_seq
                       if _classify_video_role(v["production_name"]) == "navigation"]
        if nav_videos:
            lines.append("When the player arrives in this scene, "
                         "they land on a navigable 360° view. The "
                         "entry videos available depending on the game state :")
            lines.append("")
            for v in nav_videos[:5]:
                state = v.get("state", "default state")
                lines.append(f"- `{v['storage_dir']}/{v['file_id']}.{v['asset_type']}` "
                             f"— **{v['production_name']}** "
                             f"(viewpoint {v.get('viewpoint', '?')}, _{state}_)")
            lines.append("")
        else:
            sample = first_seq[0]
            lines.append(f"First video : `{sample['storage_dir']}/"
                         f"{sample['file_id']}.{sample['asset_type']}` — "
                         f"**{sample['production_name']}**")
            lines.append("")

    # --- II. Available player actions ---
    lines += [
        "## II. Actions available to the player",
        "",
        "The player can interact with the scene through the following actions. ",
        "Each one fires a trigger that modifies the game state.",
        "",
    ]
    if scene_triggers:
        for i, t in enumerate(scene_triggers, 1):
            label = t["action_label"]
            var = t["trigger_id"]
            humanized = _humanize_action(label, name)
            lines.append(f"### {i}. {humanized}")
            lines.append("")
            lines.append(f"- **Trigger** : `{var}`")
            lines.append(f"- **Effect** : `{t['effect']}`")
            lines.append(f"- **Condition** : {t['condition_hint']}")
            sem = variables_sem.get(var, {})
            if sem.get("role_category"):
                lines.append(f"- **Category** : {sem['role_category']}")
            lines.append("")
    else:
        lines.append("_No named trigger identified for this scene "
                     "(passive scene or sub-scene)._")
        lines.append("")

    # --- III. Affected state variables ---
    if checkpoints:
        lines += [
            "## III. Game state modified by this scene",
            "",
            "These 7 flags can be set to `true` during the scene. ",
            "They gate behaviors in other scenes and the "
            "8 multiple endings.",
            "",
            "| Variable | Set by |",
            "|---|---|",
        ]
        for cp in checkpoints:
            var = f"bAI{name.replace(' ', '')}_{cp}"
            lines.append(f"| `{var}` | the `{cp}` action |")
        lines.append("")

    # --- IV. Video inventory ---
    if seq_keys:
        lines += [
            "## IV. Video inventory (by narrative sequence)",
            "",
            "Videos are organized into **sequences** (numbered 010, 020, "
            "...) that represent narrative phases. Within "
            "a sequence, several **viewpoints** correspond to the "
            "cardinal directions (1N=north, 2W=west, etc.).",
            "",
        ]
        for seq in seq_keys[:15]:  # cap to first 15 sequences
            videos = by_seq[seq]
            videos_xmv = [v for v in videos
                          if v.get("asset_type") in ("xmv", "dmv")]
            if not videos_xmv:
                continue
            label = f"Sequence {seq}" if seq != "no_seq" else "Unsequenced videos"
            n_v = len(videos_xmv)
            states_seen = set()
            for v in videos_xmv:
                if v.get("state"):
                    states_seen.add(v["state"])
            state_summary = ", ".join(sorted(states_seen)) if states_seen else "default state"
            lines.append(f"### {label} ({n_v} videos — _{state_summary}_)")
            lines.append("")
            # group by viewpoint
            by_vp: dict[str, list[dict]] = defaultdict(list)
            for v in videos_xmv:
                by_vp[v.get("viewpoint", "—")].append(v)
            for vp_key in sorted(by_vp.keys()):
                vids = by_vp[vp_key]
                lines.append(f"**View `{vp_key}`** :")
                lines.append("")
                for v in vids[:4]:  # cap to 4 per viewpoint
                    dur = (f" — {v['duration_s']:.1f}s" if v.get("duration_s")
                           else "")
                    lines.append(
                        f"- `{v['storage_dir']}/{v['file_id']}.{v['asset_type']}` "
                        f"— *{v['production_name']}*"
                        f"{dur}"
                    )
                lines.append("")

    # --- V. Active conversations (if applicable) ---
    related_convs = []
    for c in conversations:
        # Heuristic : conversations connected to triggers in this scene
        related_convs.append(c)
    if related_convs and any(t["action_label"].startswith("Talk")
                              for t in scene_triggers):
        lines += [
            "## V. Conversations",
            "",
            f"This scene probably includes dialogue. ",
            f"The game's global conversations (20 instances) are "
            f"detailed in `../json/decision_tree.json` and "
            f"`out/scenes/scene_conversations.md`. The "
            f"`BeginTalk*` / `Talk2*` actions above trigger "
            f"conversation nodes.",
            "",
        ]

    # --- VI. Progression conditions ---
    if checkpoints:
        lines += [
            "## VI. Narrative progression conditions",
            "",
            f"This scene contributes to the overall arc. Once the "
            f"key checkpoints are reached (e.g. all `bAI{name.replace(' ', '')}_*` "
            f"= true), the game unlocks later sequences and "
            f"influences which of the **8 multiple endings** is triggered.",
            "",
            "To track the global consequences, see "
            "`../json/decoded_records.json` (12,699 "
            "`VCGameState` records decoded byte-direct = each state snapshot).",
            "",
        ]

    # --- Cross-references ---
    lines += [
        "---",
        "",
        "## Cross-references",
        "",
        f"- **Structured data** : `scene_{_slug(name)}.json` (sibling)",
        "- **All triggers** : `../json/triggers_full.json`",
        "- **All C++ UDFs** : `../json/udfs.json` (89 functions)",
        "- **Complete variables** : `../json/variables_full.json` (670 vars)",
        "- **Decoded HDB records** : `../json/decoded_records.json` "
        "(13,605 records via PackedHDBSerializer)",
        "- **Trigger grammar** : `../json/trigger_system.json` "
        "(9 events / 8 actions / 8 operators)",
        "- **Wire format documentation** : see the format notes "
        "(established byte-direct from the on-disk structure)",
        "",
    ]

    return "\n".join(lines)


def write_storyboards(
    decision_tree: dict,
    out_dir: Path,
    scene_asset_map: dict | None = None,
) -> int:
    """Generate storyboard MD for each scene."""
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    scene_asset_map = scene_asset_map or {}
    triggers = load_triggers()
    actions = load_actions()
    convs = load_conversations()
    vars_full = load_variables_full()
    var_sem = {v["name"]: v for v in vars_full}

    count = 0
    for scene in decision_tree["scenes"]:
        productions = scene_asset_map.get(scene["scene_path"], [])
        md = build_scene_storyboard(
            scene, triggers, var_sem, productions, actions, convs,
        )
        out_path = out_dir / f"scene_{_slug(scene['scene_path'])}_storyboard.md"
        out_path.write_text(md, encoding="utf-8")
        count += 1
    return count
