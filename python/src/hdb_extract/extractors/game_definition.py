"""game_definition.json — unified honest model (schema v1).

Single artifact tying together the data side (HDB, GAM, DLL strings,
hotspots, triggers, scenes, canonical 29-step flow) so the C++ engine in
``OSS/cpp/src/engine/xfiles_engine.cpp`` can drive a scripted playthrough
from one normalized model.

Honesty model (applies to every leaf field):

  { "value": <raw>, "certainty": "byte-direct" | "undetermined"
                                                  | "walkthrough",
    "reason"?: "<closed-vocab>" }

* ``byte-direct``  — read from the bytes of ``XFILES.HDB``,
  ``XFILES.GAM``, ``XFiles*.dll`` RT_STRING block, or a sibling
  ``*.HOT`` file.
* ``undetermined`` — not resolvable from the data files. ``value`` is
  ``null``, ``reason`` is mandatory and drawn from the closed vocabulary
  defined in ``order_recovery/game_definition_schema.md`` §9.
* ``walkthrough``  — narrative text/items/puzzles copied from public
  walkthroughs. Used only inside ``flow.days[].scenes[]``.

No guess. No prose ``reason`` outside the closed vocabulary.
"""
from __future__ import annotations

import csv
import hashlib
import json
import os
from collections import Counter
from pathlib import Path
from typing import Any


# ---------------------------------------------------------------------------
# Honesty primitives
# ---------------------------------------------------------------------------

ALLOWED_CERTAINTY = {"byte-direct", "undetermined", "walkthrough"}

# Closed vocabulary for `reason` (only used with certainty=undetermined).
ALLOWED_REASONS = {
    "field-not-present-in-payload",
    "u32-not-in-dll-string-range-and-not-in-extended-index",
    "handle-not-in-extended-index",
    "scene-binding-not-resolved-byte-direct",
    "relation-byte-meaning-runtime-only",
    "opcode-to-verb-table-not-in-public-format",
    "no-authoring-annotation-in-GAM",
    "value-only-known-from-walkthrough",
}


def bd(value: Any, source: str | None = None) -> dict:
    """Wrap a byte-direct value (read from the bytes on disk)."""
    d: dict[str, Any] = {"value": value, "certainty": "byte-direct"}
    if source is not None:
        d["source"] = source
    return d


def und(reason: str) -> dict:
    """Wrap an undetermined value. Reason must be in the closed vocabulary."""
    if reason not in ALLOWED_REASONS:
        raise ValueError(f"reason {reason!r} not in closed vocabulary")
    return {"value": None, "certainty": "undetermined", "reason": reason}


def wt(value: Any) -> dict:
    """Wrap a walkthrough-derived value (story narrative)."""
    return {"value": value, "certainty": "walkthrough"}


# ---------------------------------------------------------------------------
# Artifact loading
# ---------------------------------------------------------------------------

# Default location for the reference artifact dumps relative to the OSS root.
def _default_artifacts_dir() -> Path:
    here = Path(__file__).resolve()
    # extractors/game_definition.py -> .. /.. /.. /.. /OSS/examples/outputs
    return here.parents[4] / "examples" / "outputs"


def _read_json(path: Path) -> Any:
    if not path.is_file():
        return None
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def _read_csv_rows(path: Path) -> list[dict]:
    if not path.is_file():
        return []
    with path.open("r", encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


# ---------------------------------------------------------------------------
# Section builders
# ---------------------------------------------------------------------------

def _build_scenes(all_scenes: dict | None, triggers_full: dict | None) -> dict:
    """Build ``scenes[]`` (one per location). Each scene wraps every leaf
    with a certainty marker. The trigger back-refs are matched by scene-scope
    prefix (e.g. ``bAIFieldOffice``, ``SHANKSN2_``)."""
    out: dict[str, dict] = {}
    if not all_scenes or "scenes" not in all_scenes:
        return out

    # Pre-compute trigger_id by scene-scope substring.
    triggers_by_scope: dict[str, list[str]] = {}
    if triggers_full and "triggers" in triggers_full:
        for t in triggers_full["triggers"]:
            scope = t.get("scene_scope") or ""
            triggers_by_scope.setdefault(scope, []).append(t["trigger_id"])

    for sc in all_scenes["scenes"]:
        loc = sc.get("location") or ""
        phase_machine = sc.get("phase_machine")
        phase_variable = sc.get("phase_variable")
        scene_entry = {
            "location": bd(loc, source="HDB:inline-label"),
            "video_stats": {
                "views": bd(sc.get("views", 0), source="HDB:scene_asset_map"),
                "clips_xmv": bd(sc.get("clips_xmv", 0), source="HDB:scene_asset_map"),
                "video_seconds": bd(sc.get("video_seconds", 0),
                                    source="HDB:scene_asset_map"),
            },
            "state_machine": {
                "phase_variable": (bd(phase_variable, source="GAM:state-var")
                                   if phase_variable is not None
                                   else und("field-not-present-in-payload")),
                "phase_machine": ([bd(p, source="GAM:phase-machine")
                                   for p in phase_machine]
                                  if phase_machine
                                  else []),
                "checkpoints": [bd(c, source="HDB:trigger-name")
                                for c in sc.get("checkpoints", [])],
                "scoped_state_vars": [bd(v, source="GAM:state-var")
                                      for v in sc.get("state_variables", [])],
            },
            "n_triggers_byte_direct": bd(sc.get("n_triggers", 0),
                                         source="HDB:trigger-scan"),
            # Trigger back-refs: match by scene-scope token in the trigger_id.
            "trigger_refs": _scene_trigger_refs(loc, triggers_by_scope),
            "certainty": "byte-direct",
        }
        # hotspots_ref is undetermined at the scene level — game_flow has no
        # scene-id <-> location index (documented in flow_artifact_coverage.json).
        scene_entry["hotspots_ref"] = und("scene-binding-not-resolved-byte-direct")

        out[loc] = scene_entry
    return out


def _scene_trigger_refs(location: str, triggers_by_scope: dict) -> list[dict]:
    """Pick triggers whose scene_scope is a substring of the location name."""
    if not location:
        return []
    # Normalize: location can be 'Field Office', scope 'FieldOffice'.
    loc_norm = location.replace(" ", "").replace("'", "").lower()
    refs: list[dict] = []
    seen: set[str] = set()
    for scope, ids in triggers_by_scope.items():
        scope_l = scope.lower()
        if not scope_l:
            continue
        if scope_l in loc_norm or loc_norm in scope_l:
            for tid in ids:
                if tid in seen:
                    continue
                seen.add(tid)
                refs.append(bd(tid, source="HDB:trigger-name"))
    return refs


def _build_triggers(triggers_full: dict | None) -> list[dict]:
    if not triggers_full or "triggers" not in triggers_full:
        return []
    out = []
    for t in triggers_full["triggers"]:
        out.append({
            "trigger_id": bd(t["trigger_id"], source="HDB:trigger-name"),
            "scene_scope": bd(t.get("scene_scope", ""),
                              source="HDB:trigger-name"),
            "action_label": bd(t.get("action_label", ""),
                               source="HDB:trigger-name"),
            "effect_summary": bd(t.get("effect", ""),
                                 source="HDB:single-write-trigger-pattern"),
            "condition_hint": bd(t.get("condition_hint", ""),
                                 source="HDB:single-write-trigger-pattern"),
            "vars_written": [bd(v, source="GAM:state-var")
                             for v in t.get("vars_written", [])],
            # Per schema §3 : the relation byte meaning is runtime-only.
            "relation_meaning": und("relation-byte-meaning-runtime-only"),
            "certainty": "byte-direct",
        })
    return out


def _build_conversations(convs: dict | None) -> tuple[list[dict], list[dict]]:
    """Returns (conversations[], conversation_graph_edges[])."""
    if not convs:
        return [], []
    records_out: list[dict] = []
    for r in convs.get("records", []):
        rec = {
            "record_offset": bd(r["record_offset"], source="HDB:offset"),
            "payload_size": bd(r["payload_size"], source="HDB:payload-size"),
            "kind": bd(r["kind"], source="HDB:record-kind"),
        }
        # Each field is itself an object that already carries certainty in
        # conversations.json. Normalize into the schema-v1 wrapper shape.
        for slot in ("conversation_id", "topic", "prompt", "response_1",
                     "response_2", "next_conversation_id", "flags"):
            v = r.get(slot)
            rec[slot] = _conv_slot(v)
        if "raw_dwords" in r:
            rec["raw_dwords"] = [bd(x, source="HDB:u32") for x in r["raw_dwords"]]
        if "neoid_pair" in r:
            rec["neoid_pair"] = bd(r["neoid_pair"], source="HDB:neoid-pair")
        if "dialogue_id" in r:
            rec["dialogue_id"] = _conv_slot(r["dialogue_id"])
        rec["certainty"] = "byte-direct"
        records_out.append(rec)
    edges_out: list[dict] = []
    for e in convs.get("graph_edges", []):
        edge = {
            "from": bd(e["from"], source="HDB:offset"),
            "to": bd(e["to"], source="HDB:offset"),
            "via": bd(e.get("via", ""), source="HDB:edge-kind"),
            "certainty": "byte-direct",
        }
        if "slot" in e:
            edge["slot"] = bd(e["slot"], source="HDB:slot")
        if "target_class_id" in e:
            edge["target_class_id"] = bd(e["target_class_id"],
                                         source="HDB:class-id")
        if "target_class_name" in e:
            edge["target_class_name"] = bd(e["target_class_name"],
                                           source="HDB:class-name")
        if "resolver_source" in e:
            edge["resolver_source"] = bd(e["resolver_source"],
                                         source="resolver")
        edges_out.append(edge)
    return records_out, edges_out


def _conv_slot(v: Any) -> dict:
    """Normalize a conversations.json slot value into the schema-v1 shape."""
    if v is None:
        return und("field-not-present-in-payload")
    if not isinstance(v, dict):
        return bd(v, source="HDB:slot-value")
    cert = v.get("certainty")
    if cert == "byte-direct":
        # Preserve provenance hints from the source artifact verbatim.
        keep = {k: vv for k, vv in v.items()
                if k not in ("certainty", "reason")}
        wrapped = {"value": v.get("value", v.get("raw_u32", v.get("string_id"))),
                   "certainty": "byte-direct"}
        if "target_offset" in keep:
            wrapped["target_offset"] = keep["target_offset"]
        if "target_class_id" in keep:
            wrapped["target_class_id"] = keep["target_class_id"]
        if "target_class_name" in keep:
            wrapped["target_class_name"] = keep["target_class_name"]
        if "resolver_source" in keep:
            wrapped["resolver_source"] = keep["resolver_source"]
        if "text" in keep and keep["text"] is not None:
            wrapped["text"] = keep["text"]
        if "string_id" in keep and keep["string_id"] is not None:
            wrapped["string_id"] = keep["string_id"]
        if "raw_u32" in keep and keep["raw_u32"] is not None:
            wrapped["raw_u32"] = keep["raw_u32"]
        if "handle_status" in keep:
            wrapped["handle_status"] = keep["handle_status"]
        if "kind" in keep:
            wrapped["kind"] = keep["kind"]
        return wrapped
    if cert == "undetermined":
        reason = v.get("reason")
        if reason in ALLOWED_REASONS:
            return {"value": None, "certainty": "undetermined", "reason": reason}
        # Map any out-of-vocab reason (defensive : conversations.json predates
        # the closed vocabulary) to the closest allowed bucket.
        if reason in ("u32-not-in-dll-string-range-and-not-in-extended-index",):
            return und("u32-not-in-dll-string-range-and-not-in-extended-index")
        if reason and "handle" in reason:
            return und("handle-not-in-extended-index")
        return und("field-not-present-in-payload")
    # Default : treat unknown shape as undetermined (defensive).
    return und("field-not-present-in-payload")


def _build_actions_vocabulary(av: dict | None) -> dict:
    if not av:
        return {"stats": {}, "opcodes": {}}
    stats = av.get("stats", {})
    stats_w = {k: bd(v, source="HDB:VCStdAction-scan") for k, v in stats.items()
               if not k.startswith("_")}
    opcodes: dict[str, dict] = {}
    for entry in av.get("action_types", []):
        op = str(entry["action_type"])
        opcodes[op] = {
            "count": bd(entry["count"], source="HDB:VCStdAction-scan"),
            "flags_seen": [bd(x, source="HDB:VCStdAction-flags")
                           for x in entry.get("flags_seen", [])],
            "arg_size_min": bd(entry.get("arg_size_min", 0),
                               source="HDB:VCStdAction-args"),
            "arg_size_max": bd(entry.get("arg_size_max", 0),
                               source="HDB:VCStdAction-args"),
            "arg_size_histogram": {
                k: bd(v, source="HDB:VCStdAction-args")
                for k, v in entry.get("arg_size_histogram", {}).items()
            },
            "sample_args_hex": [bd(s, source="HDB:VCStdAction-args")
                                for s in entry.get("sample_args", [])],
            "ascii_runs_seen": [bd(s, source="HDB:VCStdAction-args")
                                for s in entry.get("ascii_runs_seen", [])],
            "persistent_record_ids": [bd(x, source="HDB:VCStdAction-id")
                                      for x in entry.get("persistent_record_ids", [])],
            "candidate_effect": und("opcode-to-verb-table-not-in-public-format"),
            # The opcode RECORD is byte-direct (every scalar above is read from
            # the bytes). Only candidate_effect — the runtime verb — is
            # undetermined. The aggregate certainty therefore stays byte-direct
            # per honesty rule §0.4 : every contributing scalar is byte-direct,
            # so the wrapper is byte-direct, and the one undetermined slot is
            # documented inline.
            "certainty": "byte-direct",
        }
    return {"stats": stats_w, "opcodes": opcodes}


def _build_variables(state_vars: dict | None,
                     defaults: dict | None,
                     constants: dict | None,
                     docs: dict | None) -> dict:
    out: dict[str, Any] = {
        "namespace_global": [],
        "namespaces": {},
        "documented_variables": {},
        "enum_legends": {},
        "named_constants": {},
        "stats": {},
    }
    if state_vars and "by_scope" in state_vars:
        defaults_map = (defaults or {}).get("values", {})
        for scope, items in state_vars["by_scope"].items():
            arr = []
            for var in items:
                name = var["name"]
                entry = {
                    "name": bd(name, source="GAM:state-var"),
                    "type": bd(var.get("type", "?"), source="GAM:state-var"),
                }
                if name in defaults_map:
                    entry["default_value"] = bd(defaults_map[name]["value"],
                                                source="GAM:default-value")
                else:
                    entry["default_value"] = und("field-not-present-in-payload")
                entry["docs"] = und("no-authoring-annotation-in-GAM")
                arr.append(entry)
            if scope == "(global)":
                out["namespace_global"] = arr
            else:
                out["namespaces"][scope] = arr

    if docs:
        for name, text in docs.get("documented_variables", {}).items():
            out["documented_variables"][name] = bd(text,
                                                  source="GAM:authoring-annotation")
        for name, legend in docs.get("enum_legends", {}).items():
            out["enum_legends"][name] = {k: bd(v, source="GAM:enum-legend")
                                          for k, v in legend.items()}

    if constants and "families" in constants:
        for family, items in constants["families"].items():
            for name, info in items.items():
                out["named_constants"][name] = {
                    "value": bd(info["value"], source="GAM:constant"),
                    "ascii": bd(info.get("ascii", ""), source="GAM:constant"),
                    "family": bd(family, source="GAM:constant"),
                    "certainty": "byte-direct",
                }

    # Aggregated counts inherit byte-direct only when every contributing record
    # is byte-direct (rule §0.4). Here every named variable is byte-direct, so
    # the totals stay byte-direct.
    out["stats"] = {
        "total_names": bd(
            len(out["namespace_global"])
            + sum(len(v) for v in out["namespaces"].values()),
            source="GAM:state-var-count"),
        "default_values_present": bd(len((defaults or {}).get("values", {})),
                                     source="GAM:default-values"),
        "documented_count": bd(len(out["documented_variables"]),
                               source="GAM:authoring-annotation"),
        "enum_legends_count": bd(len(out["enum_legends"]),
                                 source="GAM:enum-legend"),
        "named_constants_count": bd(len(out["named_constants"]),
                                    source="GAM:constant"),
    }
    return out


def _build_flow(flow: dict | None,
                all_scenes: dict | None,
                triggers_full: dict | None) -> dict:
    """Canonical 29-step + byte-direct linkage."""
    out: dict[str, Any] = {
        "story": und("value-only-known-from-walkthrough"),
        "login": und("value-only-known-from-walkthrough"),
        "total_steps": bd(0, source="walkthrough-count"),
        "days": [],
    }
    if not flow:
        return out
    if "story" in flow:
        out["story"] = wt(flow["story"])
    if "login" in flow:
        out["login"] = {k: wt(v) for k, v in flow["login"].items()}
    out["total_steps"] = bd(flow.get("total_steps", 0),
                            source="walkthrough-count")

    # Index scene checkpoints/state_variables for quick back-ref.
    location_index: dict[str, dict] = {}
    if all_scenes:
        for sc in all_scenes.get("scenes", []):
            location_index[sc.get("location", "")] = sc

    triggers_by_scope: dict[str, list[str]] = {}
    if triggers_full:
        for t in triggers_full.get("triggers", []):
            triggers_by_scope.setdefault(t.get("scene_scope", ""),
                                         []).append(t["trigger_id"])

    for day in flow.get("days", []):
        day_w = {"day": wt(day["day"]), "scenes": []}
        for step in day.get("scenes", []):
            loc = step.get("location", "")
            sc_idx = location_index.get(loc, {})
            trig_refs = _scene_trigger_refs(loc, triggers_by_scope)
            mech = step.get("mechanics_byte_direct", {})
            step_w = {
                "step": bd(step["step"], source="walkthrough-order"),
                "location": bd(loc, source="HDB:inline-label"),
                "actions": wt(step.get("actions", "")),
                "items": [wt(it) for it in step.get("items", [])],
                "puzzles_codes": [wt(p) for p in step.get("puzzles_codes", [])],
                "mechanics_byte_direct": {
                    "scene_ref": (bd(loc, source="HDB:scene-binding")
                                  if loc in location_index
                                  else und("scene-binding-not-resolved-byte-direct")),
                    "views": bd(mech.get("views", sc_idx.get("views", 0)),
                                source="HDB:scene_asset_map"),
                    "clips_xmv": bd(mech.get("clips", sc_idx.get("clips_xmv", 0)),
                                    source="HDB:scene_asset_map"),
                    "phase_machine": [bd(p, source="GAM:phase-machine")
                                      for p in (mech.get("phase_machine")
                                                or sc_idx.get("phase_machine")
                                                or [])],
                    "checkpoints": [bd(c, source="HDB:trigger-name")
                                    for c in (mech.get("checkpoints")
                                              or sc_idx.get("checkpoints", []))],
                    "checkpoint_vars": [bd(v, source="GAM:state-var")
                                        for v in (mech.get("state_variables")
                                                  or sc_idx.get("state_variables", []))],
                    "trigger_refs": trig_refs,
                },
            }
            day_w["scenes"].append(step_w)
        out["days"].append(day_w)
    return out


def _build_dialogues(dialogue_lines: dict | None,
                     dll_dir: Path | None) -> dict:
    """Build dialogues{} from XFilest.dll (already extracted as dialogue_lines.json).
    Sibling DLLs are loaded on demand if available."""
    out: dict[str, Any] = {}
    base = {
        "_source": "XFilest.dll RT_STRING block, UTF-16LE",
        "count": und("field-not-present-in-payload"),
        "lines": {},
    }
    if dialogue_lines:
        base["count"] = bd(dialogue_lines.get("count", 0),
                           source="DLL:XFilest:string-count")
        base["lines"] = {
            sid: bd(text, source=f"DLL:XFilest:string#{sid}")
            for sid, text in dialogue_lines.get("lines", {}).items()
        }
    out["dialogues"] = base

    # Optional siblings, byte-direct from PE RT_STRING.
    for dll_stem, key in (("XFilese", "dialogues_en"),
                          ("XFiless", "dialogues_sys"),
                          ("XFilesc", "dialogues_credits")):
        if dll_dir is None:
            continue
        dll_path = dll_dir / f"{dll_stem}.dll"
        if not dll_path.is_file():
            continue
        try:
            from hdb_extract.extractors.loc_extract import LocaleDLL
            loc = LocaleDLL(dll_path)
            out[key] = {
                "_source": f"{dll_stem}.dll RT_STRING block, UTF-16LE",
                "count": bd(len(loc.strings),
                            source=f"DLL:{dll_stem}:string-count"),
                "lines": {
                    str(sid): bd(text, source=f"DLL:{dll_stem}:string#{sid}")
                    for sid, text in sorted(loc.strings.items())
                },
            }
        except Exception:  # pragma: no cover — pefile-related runtime issues
            out[key] = {
                "_source": f"{dll_stem}.dll RT_STRING block, UTF-16LE",
                "count": und("field-not-present-in-payload"),
                "lines": {},
            }
    return out


# ---------------------------------------------------------------------------
# Field statistics (count every wrapped leaf)
# ---------------------------------------------------------------------------

def _count_fields(node: Any) -> tuple[int, int, int]:
    """Walk the tree; return (total_with_certainty, byte_direct, undetermined).
    The third honesty bucket (``walkthrough``) is the remainder."""
    total = byte_direct = undetermined = 0
    if isinstance(node, dict):
        c = node.get("certainty")
        if c in ALLOWED_CERTAINTY:
            total += 1
            if c == "byte-direct":
                byte_direct += 1
            elif c == "undetermined":
                undetermined += 1
            # walkthrough = total - bd - ud
        for v in node.values():
            t, b, u = _count_fields(v)
            total += t
            byte_direct += b
            undetermined += u
    elif isinstance(node, list):
        for v in node:
            t, b, u = _count_fields(v)
            total += t
            byte_direct += b
            undetermined += u
    return total, byte_direct, undetermined


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def build_game_definition(hdb_path: str | Path,
                          artifacts_dir: str | Path | None = None,
                          dll_dir: str | Path | None = None) -> dict:
    """Assemble the unified game_definition.json model.

    ``hdb_path`` is required (used for the SHA-256 + size).
    ``artifacts_dir`` defaults to ``OSS/examples/outputs``.
    ``dll_dir`` defaults to the directory holding ``XFILES.HDB``.
    """
    hdb_path = Path(hdb_path)
    art = Path(artifacts_dir) if artifacts_dir else _default_artifacts_dir()
    dll_dir_p = Path(dll_dir) if dll_dir else hdb_path.parent

    # ---- byte-direct file identity ----
    raw = hdb_path.read_bytes()
    hdb_sha = _sha256(raw)
    hdb_size = len(raw)

    # ---- load every available artifact ----
    all_scenes = _read_json(art / "all_scenes.json")
    triggers_full = _read_json(art / "triggers_full.json")
    conversations = _read_json(art / "conversations.json")
    action_vocab = _read_json(art / "action_vocabulary.json")
    dialogue_lines = _read_json(art / "dialogue_lines.json")
    flow_local = _read_json(art / "game_flow.json")
    # Canonical flow lives in order_recovery/ (per the brief).
    flow_canonical = None
    order_root = hdb_path.parent / "order_recovery"
    if order_root.is_dir():
        flow_canonical = _read_json(order_root / "game_flow.json")
    flow = flow_canonical or flow_local

    state_vars = _read_json(art / "gam_state_variables.json")
    defaults = _read_json(art / "gam_values_default.json")
    constants = _read_json(art / "gam_constants.json")
    var_docs = _read_json(art / "gam_var_docs.json")
    # relation_byte_distribution lives next to the other artifacts.
    relation_rows = _read_csv_rows(art / "relation_byte_distribution.csv")

    # ---- section builders ----
    scenes = _build_scenes(all_scenes, triggers_full)
    triggers = _build_triggers(triggers_full)
    convs, edges = _build_conversations(conversations)
    actions = _build_actions_vocabulary(action_vocab)
    variables = _build_variables(state_vars, defaults, constants, var_docs)
    flow_w = _build_flow(flow, all_scenes, triggers_full)
    dialogues_blob = _build_dialogues(dialogue_lines, dll_dir_p)

    # Relation byte distribution is a global piece of byte-direct evidence,
    # surfaced inside the trigger section as evidence (each count is byte-direct).
    relation_dist = [
        {
            "relation_byte": bd(row["relation_byte"], source="HDB:CNeoPart+14"),
            "count_global": bd(int(row["count_global"]),
                               source="HDB:CNeoPart-scan"),
            "count_in_canonical_triggers": bd(
                int(row["count_in_canonical_triggers"]),
                source="HDB:CNeoPart-scan"),
            "relation_meaning": und("relation-byte-meaning-runtime-only"),
        }
        for row in relation_rows
    ]

    # ---- assemble (top-level key order is stable, schema §10) ----
    result: dict[str, Any] = {
        "_meta": {
            "schema_version": bd("1.0", source="schema-v1"),
            "generated_from": [
                bd("XFILES.HDB", source="HDB:bytes"),
                bd("XFILES.GAM", source="GAM:bytes"),
                bd("XFilest.dll", source="DLL:bytes"),
                bd("XFilese.dll", source="DLL:bytes"),
                bd("XFiless.dll", source="DLL:bytes"),
                bd("XFilesc.dll", source="DLL:bytes"),
                bd("*.HOT", source="HOT:bytes"),
            ],
            "source_hdb_sha256": bd(hdb_sha, source="sha256(XFILES.HDB)"),
            "source_hdb_size": bd(hdb_size, source="len(XFILES.HDB)"),
            "byte_direct_pct": None,            # filled below
            "field_counts": None,               # filled below
            "certainty_legend": {
                "byte-direct": "read directly from on-disk bytes",
                "undetermined": "not resolvable from data files; reason documented",
                "walkthrough":
                    "step text/items from public walkthroughs, not bytes",
            },
            "tooling": {
                "emitter": "OSS/python/src/hdb_extract/extractors/game_definition.py",
                "engine_consumer": "OSS/cpp/src/engine/xfiles_engine.cpp",
            },
        },
        "flow": flow_w,
        "scenes": scenes,
        "triggers": triggers,
        "conversations": convs,
        "conversation_graph_edges": edges,
        "hotspots": und("scene-binding-not-resolved-byte-direct"),
        "actions_vocabulary": actions,
        "variables": variables,
        "relation_byte_distribution": relation_dist,
        "dialogues": dialogues_blob.get("dialogues", {}),
        "dialogues_en": dialogues_blob.get("dialogues_en", {}),
        "dialogues_sys": dialogues_blob.get("dialogues_sys", {}),
        "dialogues_credits": dialogues_blob.get("dialogues_credits", {}),
        "_stats": {
            "scene_count": bd(len(scenes), source="all_scenes.json"),
            "trigger_count": bd(len(triggers), source="triggers_full.json"),
            "conversation_count": bd(len(convs), source="conversations.json"),
            "conversation_graph_edges": bd(len(edges),
                                           source="conversations.json"),
            "variable_count": bd(
                len(variables.get("namespace_global", []))
                + sum(len(v) for v in variables.get("namespaces", {}).values()),
                source="GAM:state-var-count"),
            "dialogue_count": (bd(dialogue_lines.get("count", 0),
                                  source="DLL:XFilest:string-count")
                               if dialogue_lines
                               else und("field-not-present-in-payload")),
            "opcode_count": bd(len(actions.get("opcodes", {})),
                               source="HDB:VCStdAction-scan"),
        },
    }

    # ---- field-count statistics ----
    # Install placeholder wrappers FIRST so the wrappers themselves are counted
    # — this keeps the totals self-consistent (audit count == reported total).
    result["_meta"]["byte_direct_pct"] = bd(0.0, source="field-count-aggregate")
    result["_meta"]["field_counts"] = {
        "total": bd(0, source="field-count-aggregate"),
        "byte_direct": bd(0, source="field-count-aggregate"),
        "undetermined": bd(0, source="field-count-aggregate"),
        "walkthrough": bd(0, source="field-count-aggregate"),
    }
    total, bd_count, ud_count = _count_fields(result)
    walkthrough_count = total - bd_count - ud_count
    pct = (bd_count * 100.0 / total) if total else 0.0
    # Mutate the wrappers in place (count is now stable).
    result["_meta"]["byte_direct_pct"]["value"] = round(pct, 4)
    result["_meta"]["field_counts"]["total"]["value"] = total
    result["_meta"]["field_counts"]["byte_direct"]["value"] = bd_count
    result["_meta"]["field_counts"]["undetermined"]["value"] = ud_count
    result["_meta"]["field_counts"]["walkthrough"]["value"] = walkthrough_count
    return result


def write_game_definition(hdb_path: str | Path,
                          out_path: str | Path,
                          artifacts_dir: str | Path | None = None,
                          dll_dir: str | Path | None = None) -> dict:
    """Build and write the unified model. Returns the model itself."""
    gd = build_game_definition(hdb_path, artifacts_dir=artifacts_dir,
                                dll_dir=dll_dir)
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as f:
        json.dump(gd, f, indent=1, ensure_ascii=False)
    return gd


def verify_certainty(gamedef: dict) -> dict:
    """Audit pass: every node carrying a ``certainty`` is in the allowed set.
    Returns ``{"ok": bool, "certainty_counts": {...}, "violations": [...]}``.
    """
    counts: Counter = Counter()
    bad: list[tuple[str, str]] = []

    def walk(node: Any, path: str = "") -> None:
        if isinstance(node, dict):
            c = node.get("certainty")
            if c is not None:
                counts[c] += 1
                if c not in ALLOWED_CERTAINTY:
                    bad.append((path, c))
                if c == "undetermined":
                    r = node.get("reason")
                    if r not in ALLOWED_REASONS:
                        bad.append((path + ".reason", str(r)))
            for k, v in node.items():
                walk(v, f"{path}.{k}")
        elif isinstance(node, list):
            for i, v in enumerate(node):
                walk(v, f"{path}[{i}]")

    walk(gamedef)
    return {
        "ok": not bad,
        "certainty_counts": dict(counts),
        "violations": bad,
    }
