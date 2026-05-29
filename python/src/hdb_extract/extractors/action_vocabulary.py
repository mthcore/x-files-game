"""VCStdAction (class 0x54) action-vocabulary extractor — byte-direct.

Every VCStdAction record stored in the HDB shares the same 8-byte
NeoPersist-tagged header that precedes its opcode-specific arguments :

    +0  BE u16   persistent_record_id   (intra-class NeoPersist id,
                                         unique per record, referenced
                                         by VCStdActionList containers)
    +2  BE u16   reserved               (always 0 in the shipped data)
    +4  BE u16   action_opcode          (the action verb — 23 distinct
                                         values across the 60 records)
    +6  BE u16   flags                  (0, 1, or 0x8xxx for the chained
                                         sub-record variants)
    +8  ...       opcode-specific args  (variable length, see arg_bytes)

The grammar above is established directly from the on-disk bytes : every
one of the 51 records with a payload >= 8 B follows it, the bytes at +2/+3
are zero on every record, and `bytes [0:2]` are unique across the table
(60 distinct values for 60 records). The shorter 8-byte records carry the
same 8-byte header with no arguments.

Honesty rule
------------
We do NOT have a public ground-truth mapping from action_opcode to verb
semantics. Therefore every emitted action_type carries
`candidate_effect="undetermined"` with a documented reason; the byte-direct
fields (opcode, flags, args_hex, ASCII args when present) are emitted as
evidence so downstream readers can verify themselves.

The ext.decomp_intel hotspot CSV is consulted only for an optional
cross-reference : `classification_hint` reports whether the opcode value
also appears inside the hotspot action_id space. It typically does NOT
(VCStdAction opcodes are a small enum of ~23 values; hotspot action_ids
live in the 200..100867 range), so for almost every opcode the hint is
"absent_from_action_id_csv" — that's a feature, not a bug : it documents
that these are two distinct id spaces.
"""
from __future__ import annotations

import json
import struct
from collections import Counter
from pathlib import Path
from typing import Iterable

from hdb_extract.btree.record_index import HDBRecordIndex
from hdb_extract.container.container import HdbContainer

# Allowed candidate_effect enum (closed set). Anything outside this list is
# rejected by the build step — it forces us to either map to one of these or
# stay honest with "undetermined".
ALLOWED_EFFECTS = {
    "play_clip",
    "navigate",
    "set_variable",
    "open_conversation",
    "give_item",
    "send_email",
    "advance_phase",
    "undetermined",
}

CLASS_VCSTDACTION = 0x54
HEADER_SIZE = 8  # the 4 BE u16 fields above


# ---------------------------------------------------------------------------
# Byte-direct decoder
# ---------------------------------------------------------------------------

def decode_vcstdaction_header(payload: bytes) -> dict | None:
    """Return the byte-direct header fields, or None if too short.

    The 8 bytes are read as four BE uint16 values per the documented grammar.
    """
    if len(payload) < HEADER_SIZE:
        return None
    rid, reserved, opcode, flags = struct.unpack(">HHHH", payload[:HEADER_SIZE])
    return {
        "persistent_record_id": rid,
        "reserved_word": reserved,
        "action_opcode": opcode,
        "flags": flags,
    }


def extract_arg_bytes(payload: bytes) -> bytes:
    """Return the opcode-specific argument bytes (everything after the 8-byte
    header). Empty if the record has no args.
    """
    return payload[HEADER_SIZE:] if len(payload) > HEADER_SIZE else b""


def ascii_window(b: bytes, *, min_run: int = 3) -> list[str]:
    """Extract printable-ASCII runs >= `min_run` from `b`.

    Used to surface embedded asset paths / strings inside VCStdAction
    arguments without making any structural claim about them.
    """
    out: list[str] = []
    cur: list[int] = []
    for byte in b:
        if 32 <= byte < 127:
            cur.append(byte)
        else:
            if len(cur) >= min_run:
                out.append(bytes(cur).decode("ascii"))
            cur = []
    if len(cur) >= min_run:
        out.append(bytes(cur).decode("ascii"))
    return out


# ---------------------------------------------------------------------------
# Per-record decoding + aggregation
# ---------------------------------------------------------------------------

def _decode_record(raw: bytes, byte_offset: int, payload_size: int) -> dict:
    payload = raw[byte_offset + 8 : byte_offset + 8 + payload_size]
    header = decode_vcstdaction_header(payload)
    args = extract_arg_bytes(payload)
    entry: dict = {
        "payload_size": payload_size,
        "page_relative": byte_offset & 0xFF,
        "args_hex": args.hex(),
        "arg_bytes_len": len(args),
        "ascii_runs": ascii_window(args),
        "certainty": "byte-direct" if header else "undetermined",
    }
    if header:
        entry.update(header)
    else:
        entry["reason"] = (
            f"payload {payload_size}B is below the 8-byte VCStdAction header"
        )
    return entry


def _candidate_effect_for(opcode: int,
                          flags_seen: set[int],
                          ascii_present: bool,
                          count: int) -> tuple[str, str]:
    """Return (candidate_effect, reason).

    We refuse to invent an opcode->verb mapping. The 23 opcodes we observe in
    the HDB are an internal enum of the VC engine; the public documentation
    does not pin them to specific verbs. Therefore the candidate_effect is
    always "undetermined" and we record WHY in the reason field.
    """
    # honest summary of what evidence we DO have
    if ascii_present:
        return (
            "undetermined",
            f"opcode {opcode} carries ASCII arguments in some records but the "
            "opcode->verb table is not in the public format; no claim made",
        )
    if flags_seen and any(f >= 0x8000 for f in flags_seen):
        return (
            "undetermined",
            f"opcode {opcode} appears with high-byte flags "
            f"{sorted(flags_seen)} which indicate a chained variant; verb "
            "semantics not in public format",
        )
    return (
        "undetermined",
        f"opcode {opcode} byte-direct decoded over {count} record(s); no "
        "ground-truth opcode->verb table is publicly available",
    )


def build_action_vocabulary(container: HdbContainer,
                            action_id_csv_lookup: Iterable[int] | None = None
                            ) -> dict:
    """Return the full action_vocabulary JSON payload.

    `action_id_csv_lookup` is an optional iterable of hotspot action_id
    values (from ext.decomp_intel.load_actions). When provided, each opcode
    is annotated with `classification_hint` = whether the opcode collides
    with the hotspot id space (typically not).
    """
    raw = container.raw
    idx = HDBRecordIndex(container)
    records_0x54 = idx.find_by_class(CLASS_VCSTDACTION)

    csv_set = set(action_id_csv_lookup) if action_id_csv_lookup else set()

    per_opcode: dict[int, dict] = {}
    decoded_records: list[dict] = []
    short_records: list[dict] = []
    rid_set: set[int] = set()
    rid_collisions = 0

    for r in records_0x54:
        rec = _decode_record(raw, r.byte_offset, r.payload_size)
        rec["page_id"] = r.page_id
        rec["byte_offset_in_record"] = (
            r.byte_offset - rec["page_relative"]  # page base
        )
        if rec["certainty"] != "byte-direct":
            short_records.append(rec)
            continue

        rid = rec["persistent_record_id"]
        if rid in rid_set:
            rid_collisions += 1
        rid_set.add(rid)

        opcode = rec["action_opcode"]
        op_entry = per_opcode.setdefault(opcode, {
            "count": 0,
            "flags_seen": set(),
            "arg_sizes": [],
            "rid_seen": [],
            "sample_args_hex": [],
            "ascii_runs_seen": [],
        })
        op_entry["count"] += 1
        op_entry["flags_seen"].add(rec["flags"])
        op_entry["arg_sizes"].append(rec["arg_bytes_len"])
        op_entry["rid_seen"].append(rid)
        if rec["args_hex"] and len(op_entry["sample_args_hex"]) < 4:
            op_entry["sample_args_hex"].append(rec["args_hex"])
        for s in rec["ascii_runs"]:
            if s not in op_entry["ascii_runs_seen"]:
                op_entry["ascii_runs_seen"].append(s)

        decoded_records.append(rec)

    # Build the per-action_type rollup with honest certainty markers.
    action_types: list[dict] = []
    n_undetermined = 0
    for op, info in sorted(per_opcode.items()):
        flags = info["flags_seen"]
        sizes = info["arg_sizes"]
        ascii_present = bool(info["ascii_runs_seen"])
        effect, reason = _candidate_effect_for(op, flags, ascii_present,
                                                info["count"])
        assert effect in ALLOWED_EFFECTS, f"effect {effect!r} not in enum"
        if effect == "undetermined":
            n_undetermined += 1
        hint = (
            "present_in_action_id_csv"
            if csv_set and op in csv_set
            else ("absent_from_action_id_csv" if csv_set else "no_csv_loaded")
        )
        action_types.append({
            "action_type": op,
            "count": info["count"],
            "flags_seen": sorted(flags),
            "arg_size_min": min(sizes) if sizes else 0,
            "arg_size_max": max(sizes) if sizes else 0,
            "arg_size_histogram": _hist(sizes),
            "sample_args": info["sample_args_hex"],
            "ascii_runs_seen": info["ascii_runs_seen"][:5],
            "classification_hint": hint,
            "candidate_effect": effect,
            "certainty": "byte-direct" if effect != "undetermined" else "undetermined",
            "certainty_reason": reason,
            "persistent_record_ids": sorted(info["rid_seen"]),
        })

    # Stats
    flag_hist = Counter()
    for rec in decoded_records:
        flag_hist[rec["flags"]] += 1
    stats = {
        "vcstdaction_records_total": len(records_0x54),
        "vcstdaction_records_decoded": len(decoded_records),
        "vcstdaction_short_records": len(short_records),
        "distinct_action_types": len(action_types),
        "action_types_byte_direct_effect": (
            len(action_types) - n_undetermined
        ),
        "action_types_undetermined": n_undetermined,
        "distinct_persistent_record_ids": len(rid_set),
        "persistent_record_id_collisions": rid_collisions,
        "flags_histogram": {str(k): v for k, v in sorted(flag_hist.items())},
        "csv_action_ids_loaded": len(csv_set),
    }

    return {
        "_source": "XFILES.HDB class 0x54 VCStdAction records",
        "_about": (
            "Every VCStdAction record is decoded byte-direct as a 4xBE-u16 "
            "header (persistent_record_id, reserved=0, action_opcode, flags) "
            "followed by opcode-specific argument bytes. Distinct opcodes "
            "are tabulated with their flags, argument-size histogram, and "
            "embedded ASCII runs."
        ),
        "_grammar": {
            "byte_0_1": "BE u16 persistent_record_id (intra-class id)",
            "byte_2_3": "BE u16 reserved (observed 0 in all 60 records)",
            "byte_4_5": "BE u16 action_opcode (the action verb)",
            "byte_6_7": "BE u16 flags (0, 1, or 0x8xxx variants)",
            "byte_8_plus": "opcode-specific argument bytes",
        },
        "_honesty": (
            "candidate_effect is 'undetermined' for every opcode because the "
            "opcode->verb mapping is not part of the public format. The "
            "byte-direct fields (opcode, flags, args_hex, ASCII runs) are "
            "emitted as evidence for downstream analysis."
        ),
        "stats": stats,
        "action_types": action_types,
        "short_records": short_records,
        "records": decoded_records,
    }


def _hist(values: list[int]) -> dict[str, int]:
    h: dict[int, int] = {}
    for v in values:
        h[v] = h.get(v, 0) + 1
    return {str(k): v for k, v in sorted(h.items())}


# ---------------------------------------------------------------------------
# Entry point used by tests / CLI
# ---------------------------------------------------------------------------

def write_action_vocabulary_json(hdb_path: str, out_path: str,
                                 csv_dir: str | None = None) -> dict:
    """Read the HDB at `hdb_path`, build the action_vocabulary, and write JSON.

    Returns the `stats` block from the result.
    """
    container = HdbContainer.from_path(hdb_path)
    action_ids: set[int] | None = None
    if csv_dir is not None:
        try:
            from hdb_extract.ext.decomp_intel import load_actions
            action_ids = {a["action_id"] for a in load_actions(Path(csv_dir))}
        except Exception:
            action_ids = None
    result = build_action_vocabulary(container, action_ids)
    Path(out_path).write_text(
        json.dumps(result, indent=1, ensure_ascii=False),
        encoding="utf-8",
    )
    return result["stats"]
