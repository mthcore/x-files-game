"""Generate human-readable per-scene Markdown scripts.

For each scene, produces a Markdown document that combines :
  - Overview (videos, duration, hotspots, checkpoints, triggers)
  - Gameplay scripts in pseudo-code form
  - Video flow organized by sequence + viewpoint
  - Game state variables this scene touches
  - Reimplementation notes

Output is `out/scenes/scene_<slug>.md` (alongside scene_<slug>.json).
"""
from __future__ import annotations

import re
from pathlib import Path
from typing import Any


def _slug(s: str) -> str:
    return re.sub(r"[^a-zA-Z0-9]+", "_", s).strip("_").lower()


def _humanize_state(states: list[str]) -> str:
    if not states:
        return "default"
    mapping = {
        "boat": "boat present",
        "no_boat": "boat absent",
        "day": "daytime",
        "night": "nighttime",
    }
    return ", ".join(mapping.get(s, s) for s in states)


def write_scene_md(scene: dict, out_path: Path) -> None:
    """Write a scene's readable Markdown alongside its JSON."""
    name = scene["name"]
    summary = scene["summary"]
    checkpoints = scene.get("checkpoints", [])
    triggers = scene.get("triggers", [])
    flow = scene.get("flow", [])
    non_video = scene.get("non_video_assets", [])

    lines = [
        f"# Scene : **{name}**",
        "",
        "## Overview",
        "",
        f"- **Videos** : {summary['n_videos_xmv']} (xmv) — "
        f"{summary['video_duration_min']} min total",
        f"- **Mapped productions** : {summary['n_productions_mapped']}",
        f"- **Hotspot files** : {summary['hotspot_files']}",
        f"- **Audio/music/text** : {summary['n_audio_amv']} amv / "
        f"{summary['n_music_mus']} mus / {summary['n_text_xtx']} xtx",
        f"- **Total bytes** : {summary['total_bytes']:,}",
        "",
    ]

    # --- Game state checkpoints ---
    if checkpoints:
        lines += [
            "## Game state checkpoints",
            "",
            "These boolean flags can flip when the player completes the "
            "listed action in this scene. They gate downstream behavior in "
            "other scenes and the 8 multiple endings.",
            "",
            "| Variable | Type | Scope | Set when player does |",
            "|---|---|---|---|",
        ]
        for cp in checkpoints:
            lines.append(
                f"| `{cp['variable']}` | "
                f"{cp.get('type', 'bool')} | "
                f"{cp.get('scope', 'Title')} | "
                f"`{cp.get('set_by_action', '?')}` action |"
            )
        lines.append("")

    # --- Triggers (game scripts) ---
    if triggers:
        lines += [
            "## Gameplay scripts (triggers)",
            "",
            "Each trigger is a small script that fires when the player "
            "completes a specific action in this scene. The byte-direct "
            "effect is to set the named variable to `true` ; additional "
            "actions (video playback, UDF calls, state mutations) are "
            "part of the VCTrigger record payload (see "
            "`decoded_records.json` filtered by_class).",
            "",
            "```pseudo",
        ]
        for t in triggers:
            lines.append(f"trigger {t['trigger_id']}")
            lines.append(f"  on:     player completes action \"{t['action_label']}\"")
            lines.append(f"  scope:  scene \"{t['scene_scope']}\"")
            lines.append(f"  effect: {t['effect']}")
            if t.get("condition_hint"):
                lines.append(f"  hint:   {t['condition_hint']}")
            lines.append("")
        lines.append("```")
        lines.append("")

    # --- Video flow by sequence ---
    if flow:
        lines += [
            "## Video flow (Myst-style 360° navigation)",
            "",
            "Videos are organized by **sequence number** (010, 020, ...) "
            "and **viewpoint** (1N=north, 2W=west, ...). Within each "
            "viewpoint, multiple variants play conditional on game state "
            "(boat present/absent, daytime/nighttime, takes).",
            "",
        ]
        for step in flow:
            seq = step["sequence"]
            label = step.get("label", f"Sequence {seq}")
            n_vids = step.get("n_videos", 0)
            lines.append(f"### {label} ({n_vids} videos)")
            lines.append("")
            viewpoints = step.get("viewpoints", {})
            for vp_key, videos in viewpoints.items():
                lines.append(f"**Viewpoint `{vp_key}`** :")
                lines.append("")
                for v in videos:
                    states = _humanize_state(v.get("states", []))
                    take = f" take{v['take']}" if v.get("take") else ""
                    dur = (f" {v['duration_s']:.1f}s"
                           if v.get("duration_s") else "")
                    sz = (f" {v['size_bytes']:,}B"
                          if v.get("size_bytes") else "")
                    lines.append(
                        f"- `{v['file']}` — **{v['production_name']}** — "
                        f"_{states}_{take}{dur}{sz}"
                    )
                lines.append("")

    if non_video:
        lines += ["### Non-video assets", ""]
        for nv in non_video:
            lines.append(
                f"- `{nv['file']}` — {nv['production_name']}"
            )
        lines.append("")

    # --- Reimplementation notes ---
    lines += [
        "## Re-implementation notes",
        "",
        "1. Load assets for this scene from `XV/`, `XN/`, `XG/`, `XT/`, "
        "`XS/`.",
        "2. Start at `flow[0]` (lowest order_index) with default state.",
        "3. Play the appropriate viewpoint video for current game state.",
        "4. Listen for hotspot clicks ; fire the matching trigger from "
        "the scripts list above.",
        "5. Each trigger sets a `bAI<Scene>_<Action>` flag — propagate to "
        "downstream scenes via the `VCGameState` records "
        "(see `decoded_records.json`).",
        "6. Transition to another scene via navigation hotspots "
        "(typically `XN/` videos).",
        "",
        "## Cross-references",
        "",
        f"- JSON data : `scene_{_slug(name)}.json` (sibling file)",
        f"- Decoded records : `../json/decoded_records.json`",
        f"- Architecture grammar : `../json/trigger_system.json`",
        f"- UDF handlers : `../json/udfs.json`",
        f"- Variables semantics : `../json/variables_full.json`",
        "",
    ]
    out_path.write_text("\n".join(lines), encoding="utf-8")


def write_scenes_md(scenes_dir: Path) -> int:
    """Walk scene_*.json files and produce sibling scene_*.md files."""
    import json
    scenes_dir = Path(scenes_dir)
    count = 0
    for json_path in sorted(scenes_dir.glob("scene_*.json")):
        try:
            scene = json.loads(json_path.read_text(encoding="utf-8"))
        except Exception:
            continue
        md_path = json_path.with_suffix(".md")
        write_scene_md(scene, md_path)
        count += 1
    return count
