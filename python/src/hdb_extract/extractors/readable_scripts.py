"""Generate readable NL-style gameplay scripts from the decision tree.

For each scene, produces a Markdown script that captures :

  - All video productions organized by sequence number + viewpoint
  - Conditional variants (boat/no_boat, day/night, etc.)
  - Game state checkpoints the scene can flip (`bAI<Scene>_<Action>`)
  - Triggers that fire on player actions
  - Re-implementation notes

The pattern is Myst-style 360° navigation : each scene is a node with
multiple viewpoints (1N, 2W, 3E, 4S, 5W, 11W, 12N, ...) and short
videos that play depending on game state.
"""
from __future__ import annotations

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
    """Parse a production_name into seq/viewpoint/states/take."""
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


def _group_by_seq(productions: list[dict]) -> dict[str, list[dict]]:
    groups: dict[str, list[dict]] = {}
    for p in productions:
        info = _parse_production(p["production_name"])
        key = info.get("seq", "no_seq")
        groups.setdefault(key, []).append({**p, **info})
    return dict(sorted(groups.items()))


def _humanize_state(states: list[str]) -> str:
    if not states:
        return "default state"
    mapping = {
        "boat": "boat present",
        "no_boat": "boat absent",
        "day": "daytime",
        "night": "nighttime",
    }
    return ", ".join(mapping.get(s, s) for s in states)


def write_readable_scripts(
    tree: dict,
    out_dir: Path,
    *,
    scene_asset_map: dict[str, list[dict]] | None = None,
) -> None:
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    scene_asset_map = scene_asset_map or {}

    action_names: dict[int, str] = {}
    for a in tree["actions"]:
        action_names[a["action_id"]] = a.get("classification", "?")

    # 0. Index
    lines = [
        "# X-Files HDB — readable gameplay scripts",
        "",
        "Each scene is a Myst-style 360° navigation node with hotspots,",
        "conditional video variants, and game-state flags that drive the story.",
        "",
        "Generated from `the research datasets` (ground truth) joined with",
        "`scene_asset_map.csv` (2,432 productions with names + durations).",
        "",
        "## Stats",
        "",
        f"- **Scenes** : {tree['summary']['scenes']}",
        f"- **Hotspots** : {tree['summary']['hotspots']}",
        f"- **Actions** : {tree['summary']['actions']}",
        f"- **Triggers** : {tree['summary']['triggers']}",
        f"- **Game state variables** : {tree['summary']['variables']}",
        f"- **Conversations** : {tree['summary']['conversations']}",
        f"- **Emails** : {tree['summary']['emails']}",
        f"- **Endings** : {tree['summary']['endings']}",
        "",
        "## Scenes",
        "",
    ]
    for s in tree["scenes"]:
        slug = _slug(s["scene_path"])
        assets = scene_asset_map.get(s["scene_path"], [])
        lines.append(
            f"- [scene_{slug}.md](scene_{slug}.md) — **{s['scene_path']}** "
            f"({s['n_xmv']} videos, {len(assets)} mapped productions, "
            f"{s['n_ai_checkpoints']} checkpoints, {s['n_triggers']} triggers)"
        )
    lines += [
        "",
        "## Cross-cutting docs",
        "",
        "- [triggers.md](triggers.md) — all 61 triggers and their effects",
        "- [variables.md](variables.md) — 670 game state variables",
        "- [conversations.md](conversations.md) — conversation graph (20 instances)",
        "- [emails.md](emails.md) — 323-email arc",
        "- [endings.md](endings.md) — 8 multiple endings",
    ]
    (out_dir / "00_index.md").write_text("\n".join(lines), encoding="utf-8")

    for s in tree["scenes"]:
        slug = _slug(s["scene_path"])
        productions = scene_asset_map.get(s["scene_path"], [])
        _write_scene_script(out_dir / f"scene_{slug}.md", s, productions,
                            action_names)

    _write_triggers(out_dir / "triggers.md", tree["triggers"])
    _write_variables(out_dir / "variables.md", tree["variables"])
    _write_conversations(out_dir / "conversations.md", tree["conversations"])
    _write_emails(out_dir / "emails.md", tree["emails"])
    _write_endings(out_dir / "endings.md", tree["endings"])


def _write_scene_script(
    path: Path,
    scene: dict,
    productions: list[dict],
    action_names: dict[int, str],
) -> None:
    name = scene["scene_path"]
    lines = [
        f"# Scene : **{name}**",
        "",
        f"_Myst-style 360° navigation node with {len(productions)} mapped productions._",
        "",
        "## Overview",
        "",
        f"- Videos (xmv) : **{scene['n_xmv']}**",
        f"- Audio voice (amv) : {scene['n_amv']}",
        f"- Music (mus) : {scene['n_mus']}",
        f"- Text scripts (xtx) : {scene['n_xtx']}",
        f"- Total bytes : {scene['total_bytes']:,}",
        f"- Video duration : **{scene['video_dur_s']:,} sec** "
        f"({scene['video_dur_s'] // 60} min)",
        f"- Hotspot files : {scene['n_hot_files']}",
        "",
    ]

    if scene["ai_checkpoints"]:
        lines += [
            "## Game state checkpoints (`bAI<Scene>_<Action>` flags)",
            "",
            "These boolean flags can flip during this scene. They gate",
            "downstream conditional behavior in other scenes and endings.",
            "",
        ]
        for cp in scene["ai_checkpoints"]:
            lines.append(f"- `bAI{name.replace(' ', '')}_{cp}` "
                         f"= `false` initially")
        lines.append("")

    if scene["triggers"]:
        lines += [
            "## Triggers (game state mutations in this scene)",
            "",
            "```pseudo",
        ]
        for t in scene["triggers"]:
            lines.append(f"on  player_action == \"{t['action_label']}\"")
            lines.append(f"in  scene \"{t['scene_scope']}\"")
            lines.append(f"    -> {t['effect']}")
            lines.append(f"    # {t['condition_hint']}")
            lines.append("")
        lines.append("```")
        lines.append("")

    if productions:
        groups = _group_by_seq(productions)
        video_groups: dict[str, list[dict]] = {}
        non_video: list[dict] = []
        for seq, items in groups.items():
            videos = [it for it in items if it["asset_type"] in ("xmv", "dmv")]
            non_video.extend(it for it in items
                             if it["asset_type"] not in ("xmv", "dmv"))
            if videos:
                video_groups[seq] = videos

        lines += [
            "## Gameplay video flow",
            "",
            "Videos are organized by **sequence number** (the leading "
            "010_, 020_, ... in production names). Within each sequence,",
            "multiple **viewpoint** variants represent the Myst-style 360°",
            "navigation (1N=north, 2W=west, 3E=east, 4S=south, etc.).",
            "",
            "Conditional variants (boat / no_boat, day / night) play",
            "depending on game state flags.",
            "",
        ]
        for seq, items in video_groups.items():
            label = (f"Sequence {seq}" if seq != "no_seq"
                     else "Unsequenced videos")
            lines.append(f"### {label}")
            lines.append("")
            by_view: dict[str, list[dict]] = {}
            for it in items:
                vp = it.get("viewpoint", "—")
                by_view.setdefault(vp, []).append(it)
            for vp, vids in sorted(by_view.items()):
                lines.append(f"**Viewpoint `{vp}`** :")
                lines.append("")
                for v in vids:
                    states = v.get("states", [])
                    state_str = _humanize_state(states)
                    take = f" take{v['take']}" if v.get("take") else ""
                    dur = (f" {v['duration_s']:.1f}s" if v.get("duration_s")
                           else "")
                    sz = (f" {v['size_bytes']:,}B" if v.get("size_bytes")
                          else "")
                    lines.append(
                        f"- `{v['storage_dir']}/{v['file_id']}"
                        f".{v['asset_type']}` "
                        f"— **{v['production_name']}** "
                        f"— _{state_str}_{take}{dur}{sz}"
                    )
                lines.append("")

        if non_video:
            lines.append("### Non-video assets")
            lines.append("")
            for nv in non_video:
                lines.append(
                    f"- `{nv['storage_dir']}/{nv['file_id']}"
                    f".{nv['asset_type']}` — {nv['production_name']}"
                )
            lines.append("")

    lines += [
        "## Re-implementation notes",
        "",
        "To replay this scene in a modern engine :",
        "",
        "1. **Load the assets** for this scene from `XV/`, `XN/`, `XG/`, "
        "`XT/`, `XS/`.",
        "2. **Start at the default viewpoint** (typically the first numbered "
        "sequence, e.g. `010_`) with the default state.",
        "3. **Listen for hotspot clicks** ; each hotspot has 2 action_ids "
        "(see `decision_tree.json` for the full per-hotspot mapping).",
        "4. **On action click**, evaluate the matching trigger above — "
        "it sets a `bAI<Scene>_<Action>` flag.",
        "5. **Re-play the appropriate video variant** based on which flags "
        "are now set (boat / no_boat etc. select the right take).",
        "6. **Transition** to the next scene when the user clicks a "
        "navigation hotspot (typically via `XN/` videos).",
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")


def _write_triggers(path: Path, triggers: list[dict]) -> None:
    lines = [
        "# All game triggers",
        "",
        f"Total : **{len(triggers)}** triggers from "
        "`the research datasets`.",
        "",
        "Each trigger fires when the player completes a specific action ",
        "in a specific scene, setting a `bAI*` flag to `true`.",
        "",
        "| Trigger | Scene | Action | Effect | Condition |",
        "|---|---|---|---|---|",
    ]
    for t in triggers:
        lines.append(f"| `{t['trigger_id']}` | {t['scene_scope']} | "
                     f"{t['action_label']} | {t['effect']} | "
                     f"{t['condition_hint']} |")
    path.write_text("\n".join(lines), encoding="utf-8")


def _write_variables(path: Path, variables: list[dict]) -> None:
    lines = [
        "# Game state variables",
        "",
        f"Total : **{len(variables)}** named variables in the runtime "
        "`VCGameState` singleton.",
        "",
        "Hungarian-prefix convention :",
        "",
        "- `b` : boolean flag (e.g. `bAIWong_ShowBadge`)",
        "- `i` : integer counter or index",
        "- `n` : count / number",
        "- `S` : string identifier",
        "- `I` : large integer (sequence index)",
        "- `l` : long / 32-bit integer",
        "",
        "| Variable | Type | Occurrences |",
        "|---|---|---|",
    ]
    for v in variables:
        lines.append(f"| `{v['name']}` | {v['type_prefix']} | "
                     f"{v['occurrences']} |")
    path.write_text("\n".join(lines), encoding="utf-8")


def _write_conversations(path: Path, conversations: list[dict]) -> None:
    lines = [
        "# Conversation graph",
        "",
        f"Total : **{len(conversations)}** conversation instances from "
        "`the research datasets`.",
        "",
        "Each node has a prompt, up to two responses, and a link to the ",
        "next conversation node. IDs are large integers (FOURCC-encoded) ",
        "indexing into the text resource table.",
        "",
        "| Conv ID | Speaker | Prompt | Resp 1 | Resp 2 | Next | HDB offset |",
        "|---|---|---|---|---|---|---|",
    ]
    for c in conversations:
        lines.append(
            f"| {c['conversation_id']} | {c['speaker_topic_id']} | "
            f"{c['prompt_string_id']} | "
            f"{c.get('response_id_1', '—')} | "
            f"{c.get('response_id_2', '—')} | "
            f"{c['next_conversation_id']} | {c['source_offset']} |"
        )
    path.write_text("\n".join(lines), encoding="utf-8")


def _write_emails(path: Path, emails: list[dict]) -> None:
    lines = [
        "# Email arc",
        "",
        f"Total : **{len(emails)}** emails received by the player.",
        "",
    ]
    for e in emails:
        cols = " — ".join(f"`{k}`={v}" for k, v in e.items() if v)
        lines.append(f"- {cols}")
    path.write_text("\n".join(lines), encoding="utf-8")


def _write_endings(path: Path, endings: list[dict]) -> None:
    lines = [
        "# Multiple endings (8 paths)",
        "",
        "The game has **8 distinct endings**. Which one plays depends on ",
        "the combination of `bAI*` and state flags at the final decision.",
        "",
    ]
    for e in endings:
        cols = " — ".join(f"`{k}`={v}" for k, v in e.items() if v)
        lines.append(f"- {cols}")
    path.write_text("\n".join(lines), encoding="utf-8")
