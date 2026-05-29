"""playthrough_trace.json — per-step trace walking the canonical 29-step flow.

For each step in ``flow.days[].scenes[]`` the trace cross-references the
already-byte-direct artifacts: scene catalog, triggers in scope, conversations
sharing the scene_scope, and a sample of dialogue lines reachable through those
conversations. The result is one consumable artifact a runtime can replay
step-by-step without re-reading the HDB.

Every field carries the same honesty marker shape as the rest of the unified
model (``{value, certainty, source?}``). ``certainty`` is ``byte-direct`` when
the value comes from on-disk bytes, ``walkthrough`` when it comes from the
canonical flow text, and ``undetermined`` (with a documented reason) otherwise.
"""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any


def _unwrap(v: Any) -> Any:
    if isinstance(v, dict) and "value" in v:
        return v["value"]
    return v


def _location_token(location: str) -> str:
    """The trigger_id / variable scope token for a location.

    e.g. ``Field Office`` -> ``FieldOffice``; ``Hauling Yard`` -> ``Hauling``.
    A handful of catalog entries use a different stem (``Smolnikoff`` vs
    ``Smol``, ``Secret Base`` vs ``SecretB``); the matcher below treats the
    token as a substring on both sides so all of those land.
    """
    return location.replace(" ", "")


def _triggers_for_scope(triggers: list[dict], token: str) -> list[dict]:
    out = []
    for t in triggers:
        scope = _unwrap(t.get("scene_scope")) or ""
        if not scope:
            continue
        if scope.lower() in token.lower() or token.lower() in scope.lower():
            out.append({
                "trigger_id": _unwrap(t["trigger_id"]),
                "action_label": _unwrap(t.get("action_label")),
                "effect_summary": _unwrap(t.get("effect_summary")),
                "certainty": "byte-direct",
                "source": "HDB:trigger-name",
            })
    return out


def _conversations_for_scope(conversations: list[dict],
                              token: str,
                              dialogues: dict[str, dict],
                              max_records: int = 6) -> list[dict]:
    """Conversations whose resolved topic or response text mentions the scene.

    The conversation graph is loaded with raw NeoIDs; the only byte-direct
    handle into the dialogue text is the `string_id` slot. We surface the
    conversations whose response_1 or prompt resolves to a non-empty XFilest
    line, and within those the ones whose text mentions the scene token. If
    none mention the scene, the first few conversations are returned with
    ``certainty: byte-direct`` and ``scene_match: undetermined``.
    """
    matched: list[dict] = []
    fallback: list[dict] = []
    token_low = token.lower()
    for r in conversations:
        for slot in ("prompt", "response_1", "response_2"):
            s = r.get(slot)
            if not isinstance(s, dict):
                continue
            text = s.get("text")
            sid = s.get("string_id")
            if not text or sid is None:
                continue
            entry = {
                "record_offset": _unwrap(r.get("record_offset")),
                "slot": slot,
                "string_id": sid,
                "text": text,
                "certainty": "byte-direct",
                "source": "HDB:VCConversation + XFilest.dll",
            }
            if token_low and token_low in text.lower():
                entry["scene_match"] = "text-substring"
                matched.append(entry)
            else:
                fallback.append(entry)
        if len(matched) >= max_records:
            break
    if matched:
        return matched[:max_records]
    # honest fallback: surface a few resolved lines so the trace is not empty,
    # but flag the scene match as undetermined.
    out = []
    for f in fallback[:max_records]:
        f["scene_match"] = "undetermined"
        out.append(f)
    return out


def _sample_dialogues(dialogue_lines: dict[str, dict],
                       token: str,
                       max_lines: int = 4) -> list[dict]:
    """Pick up to ``max_lines`` XFilest lines whose text mentions the scene token."""
    out: list[dict] = []
    token_low = token.lower()
    if not token_low:
        return out
    for sid_str, wrapped in dialogue_lines.items():
        if not isinstance(wrapped, dict):
            continue
        text = wrapped.get("value")
        if not isinstance(text, str):
            continue
        if token_low in text.lower():
            out.append({
                "string_id": int(sid_str),
                "text": text,
                "certainty": "byte-direct",
                "source": "DLL:XFilest:RT_STRING",
            })
            if len(out) >= max_lines:
                break
    return out


def build_playthrough_trace(game_def: dict) -> dict:
    flow = game_def.get("flow", {}) or {}
    days = flow.get("days") or []
    triggers = game_def.get("triggers") or []
    conversations = game_def.get("conversations") or []
    scenes = game_def.get("scenes") or {}
    dialogues = (game_def.get("dialogues") or {}).get("lines") or {}

    steps_out: list[dict] = []
    for day in days:
        day_name = _unwrap(day.get("day")) or ""
        for sc in day.get("scenes") or []:
            location = _unwrap(sc.get("location")) or ""
            step_num = _unwrap(sc.get("step")) or 0
            token = _location_token(location)
            in_catalog = location in scenes

            scene_entry = scenes.get(location, {}) if in_catalog else {}
            sm = scene_entry.get("state_machine", {}) if in_catalog else {}
            phase_var = _unwrap(sm.get("phase_variable")) if sm else None
            phases = [_unwrap(p) for p in sm.get("phase_machine") or []] if sm else []
            checkpoints = [_unwrap(c) for c in sm.get("checkpoints") or []] if sm else []

            actions_text = _unwrap(sc.get("actions")) or ""
            items_text = _unwrap(sc.get("items")) or ""

            step_entry = {
                "step": {"value": step_num, "certainty": "byte-direct",
                         "source": "walkthrough-order"},
                "day": {"value": day_name, "certainty": "walkthrough"},
                "location": {"value": location, "certainty": "byte-direct"
                             if in_catalog else "walkthrough",
                             "source": "HDB:inline-label" if in_catalog
                             else "walkthrough-step"},
                "status": ("byte-direct-catalog" if in_catalog
                            else "walkthrough-only"),
                "state_machine": {
                    "phase_variable": ({"value": phase_var,
                                         "certainty": "byte-direct",
                                         "source": "GAM:state-var"}
                                        if phase_var
                                        else {"value": None,
                                              "certainty": "undetermined",
                                              "reason": "no-phase-variable-for-this-scene"}),
                    "phases": phases,
                    "checkpoints": checkpoints,
                },
                "actions_walkthrough": {"value": actions_text,
                                        "certainty": "walkthrough"},
                "items_walkthrough": {"value": items_text,
                                       "certainty": "walkthrough"},
                "triggers_in_scope": _triggers_for_scope(triggers, token),
                "conversations_in_scope": _conversations_for_scope(
                    conversations, token, dialogues),
                "dialogue_lines_sample": _sample_dialogues(dialogues, token),
            }
            steps_out.append(step_entry)

    return {
        "_about": "Per-step playthrough trace cross-referencing the byte-direct "
                  "catalog with the canonical 29-step flow.",
        "_source": "examples/outputs/game_definition.json",
        "stats": {
            "steps_total": len(steps_out),
            "byte_direct_catalog_steps":
                sum(1 for s in steps_out if s["status"] == "byte-direct-catalog"),
            "walkthrough_only_steps":
                sum(1 for s in steps_out if s["status"] == "walkthrough-only"),
            "triggers_total": sum(len(s["triggers_in_scope"]) for s in steps_out),
            "conversations_total":
                sum(len(s["conversations_in_scope"]) for s in steps_out),
            "dialogue_samples_total":
                sum(len(s["dialogue_lines_sample"]) for s in steps_out),
        },
        "steps": steps_out,
    }


def write_playthrough_trace(game_def_path: str | Path,
                             out_path: str | Path) -> dict:
    game_def = json.loads(Path(game_def_path).read_text(encoding="utf-8"))
    trace = build_playthrough_trace(game_def)
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as f:
        json.dump(trace, f, indent=1, ensure_ascii=False)
    return trace["stats"]
