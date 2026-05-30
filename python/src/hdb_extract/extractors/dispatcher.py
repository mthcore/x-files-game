"""Headless dispatcher: replay the canonical 29-step flow over the unified
``game_definition.json`` and mutate a variable-state map by parsing each
trigger's byte-direct ``effect_summary``.

This is the Python equivalent of the C++ dispatcher in
``cpp/src/engine/xfiles_engine.cpp``. The two are designed to produce
identical results — a parity contract guarded by the
``test_dispatcher_parity`` integration test.

Effect grammar handled (closed set, the only shape the extractor emits):

    set <var> = true | false | <int>

Anything else is recorded as ``effects_uninterpreted``.
"""
from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any


_EFFECT_RE = re.compile(
    r"^\s*set\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^\s].*?)\s*$"
)


def _unwrap(v: Any) -> Any:
    if isinstance(v, dict) and "value" in v:
        return v["value"]
    return v


def apply_effect(effect: str, vs: dict[str, str]) -> bool:
    """Apply one effect string to vs in place. Returns True if interpreted."""
    if not isinstance(effect, str):
        return False
    m = _EFFECT_RE.match(effect)
    if not m:
        return False
    name, rhs = m.group(1), m.group(2).rstrip()
    if rhs == "true" or rhs == "false":
        vs[name] = rhs
        return True
    # Integer literal?
    try:
        int(rhs)
    except ValueError:
        return False
    vs[name] = rhs
    return True


def dispatch_playthrough(game_def: dict) -> dict:
    """Run the dispatcher over the unified model. Returns a report dict
    with: steps_dispatched, triggers_dispatched, effects_applied,
    effects_uninterpreted, variables (final state), timeline (per-step
    mutations list).
    """
    flow_days = (game_def.get("flow") or {}).get("days") or []
    scenes = game_def.get("scenes") or {}
    triggers = game_def.get("triggers") or []

    # Index trigger entries by trigger_id (string).
    by_id: dict[str, dict] = {}
    for t in triggers:
        tid = _unwrap(t.get("trigger_id"))
        if isinstance(tid, str):
            by_id[tid] = t

    vs: dict[str, str] = {}
    steps_dispatched = 0
    triggers_dispatched = 0
    effects_applied = 0
    effects_uninterpreted = 0
    timeline: list[dict] = []

    for day in flow_days:
        for sc in day.get("scenes") or []:
            location = _unwrap(sc.get("location"))
            step_n = _unwrap(sc.get("step"))
            if not isinstance(location, str) or location not in scenes:
                continue
            steps_dispatched += 1
            mutations: list[dict] = []
            trigger_refs = scenes[location].get("trigger_refs") or []
            for tr in trigger_refs:
                tid = _unwrap(tr)
                if not isinstance(tid, str) or tid not in by_id:
                    continue
                t = by_id[tid]
                triggers_dispatched += 1
                effect = _unwrap(t.get("effect_summary")) or ""
                ok = apply_effect(effect, vs)
                if ok:
                    effects_applied += 1
                    for vw in t.get("vars_written") or []:
                        vname = _unwrap(vw)
                        if isinstance(vname, str) and vname in vs:
                            mutations.append({"var": vname,
                                                "value": vs[vname]})
                else:
                    effects_uninterpreted += 1
            timeline.append({"step": step_n, "location": location,
                              "mutations": mutations})

    return {
        "steps_dispatched": steps_dispatched,
        "triggers_dispatched": triggers_dispatched,
        "effects_applied": effects_applied,
        "effects_uninterpreted": effects_uninterpreted,
        "variables": vs,
        "timeline": timeline,
    }


def write_dispatch_report(game_def_path: str | Path,
                           out_path: str | Path) -> dict:
    game_def = json.loads(Path(game_def_path).read_text(encoding="utf-8"))
    rep = dispatch_playthrough(game_def)
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as f:
        json.dump(rep, f, indent=1, ensure_ascii=False)
    return {k: v for k, v in rep.items() if k != "timeline" and k != "variables"}
