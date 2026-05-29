"""VCConversation (class 0x31) byte-direct extractor.

This module produces `conversations_full` — a streaming, format-clean view of
every VCConversation record found in XFILES.HDB, plus the list/index
containers that group them. Everything is byte-direct from the on-disk
record payload; no value is invented and no field is filled by guessing.

------------------------------------------------------------------
Grammar (W9.3c VCConversation read order)
------------------------------------------------------------------
The VCConversation class reads its tagged fields in this order:

    base ; e=conversation_id ; f=topic ; h=response_1 ; j=next   (version 1)
    + g=prompt ; i=response_2                                    (version >= 3)
    + k=flags                                                    (version >= 4)

prompt / response_1 / response_2 are u32 ids into the XFilest.dll dialogue
string table (valid range 1..1105). The fields are stored in the record
payload in the order they are read (e, f, h, j, g, i, k).

------------------------------------------------------------------
Two record shapes
------------------------------------------------------------------
1. **Conversation record** (typical, payload 8..40 bytes).
   The payload holds the read-order dwords (BE u32) — 2 dwords for the
   smallest records up to 6 for the v>=3 ones, with optional trailing
   padding for inline blobs. Each dword is exposed as the matching grammar
   slot. Slots that fall in the DLL string range are resolved to text and
   marked `certainty=byte-direct`. Slots that do not resolve to DLL text
   get a second chance via :class:`MasterHDBResolver` extended index
   (see "Extended index resolution" below). Slots that resolve via
   neither path keep their raw value and are marked
   `certainty=undetermined` with a documented `reason`.

2. **List container** (payload 144 bytes).
   Layout = 16-byte header + N pairs of (token BE u32, dialogue_id BE u32).
   The token is a list-walk handle (not an HDB byte offset). The
   dialogue_id is a DLL string id that resolves byte-direct to a prompt
   line. Each pair is surfaced as its own entry under `records` with
   `kind="list_container_entry"` and `prompt={string_id, text,
   certainty="byte-direct"}`. The list container itself appears with
   `kind="list_container"` and the synthesised graph edges link each
   pair back to it.

------------------------------------------------------------------
Extended index resolution (NeoPersist handles)
------------------------------------------------------------------
Many u32 fields outside the DLL range are NeoID handles that point at
other CNeoPersist records. The original engine resolved them through the
master index. We cross-reference them via
:meth:`MasterHDBResolver.extend_with_neoidlist_entries` (global scope) and
:meth:`MasterHDBResolver.resolve_offset` to obtain the absolute file
offset of the target marker; the marker bytes +6..+7 yield its class id.
When the resolution succeeds the slot gains the additional fields
``target_offset / target_class_id / target_class_name / resolver_source``
and stays ``certainty=byte-direct``. The resolution is gated on a
byte-direct marker check (the offset must start with 0xC0..0xCF followed
by ``00 00 00 01``); a stale offset never injects fabricated data.

------------------------------------------------------------------
Incidental DLL matches on handle slots
------------------------------------------------------------------
The grammar slots e / f / j hold raw u32 handles (conversation handle,
topic handle, next handle). When such a value happens to fall in the
DLL string range and resolves to text, the handle slot carries an
additional `dll_text` / `dll_string_id` field — purely a cross-reference,
the slot's primary meaning is *not* overridden. The smaller VCConversation
records (16 bytes = 4 dwords, v=1) only carry e/f/h/j; in those records a
human-readable dialogue line sometimes surfaces through the j slot via
this cross-reference.

------------------------------------------------------------------
Honesty markers
------------------------------------------------------------------
Every emitted field carries a `certainty` value of either:
- `byte-direct` — the value was read straight from the on-disk record and,
  for the prompt/response slots, resolved either against the DLL string
  table or via the master extended index to a real CNeoPersist marker.
- `undetermined` — the field exists in the grammar but the payload does
  not provide a value (record too short for the slot) or the value
  resolved through neither the DLL table nor the extended index. A
  `reason` always accompanies these cases.

------------------------------------------------------------------
Graph edges
------------------------------------------------------------------
Three kinds of edges are emitted when both ends are byte-direct:
- `via=list_container_entry` — from a list-container record to each
  contained (token, dialogue_id) pair.
- `via=next_link` — from a conversation record to another class-0x31
  record whose byte_offset matches the j (next) slot.
- `via=handle_target` — from a record to a resolved handle's
  target_offset. Specifically emitted (with `kind=cross_conversation`)
  when the target's class id is 0x31 (another VCConversation record);
  generic `handle_target` edges to other class ids are also surfaced
  to make the byte-direct graph complete.
"""
from __future__ import annotations

import json
import struct
from pathlib import Path
from typing import Optional

from hdb_extract.container.container import HdbContainer
from hdb_extract.btree.record_index import HDBRecordIndex, class_label
from hdb_extract.btree.master_resolver import (
    MasterHDBResolver,
    classify_handle,
)


# Additional CNeoPersist class ids that the pair-table recon
# (order_recovery/pair_table_classes.json) flagged as carrying
# ``[marker_offset, id]`` pair tables in their record payloads. The
# global scope of the resolver already subsumes every pair these classes
# contain (first-wins collision), but feeding them explicitly to
# ``extend_with_class_index`` makes the dependency explicit and gives
# callers a per-class hit report. The threshold is "max_pair_score >=
# 0.2", which keeps every class the recon scored as plausible while
# excluding noise classes.
EXTENDED_PAIR_TABLE_CLASS_IDS = (
    0x10, 0x12, 0x47, 0x59, 0x50, 0x19, 0x18, 0x43, 0x1E,
    0x08, 0x3A, 0x13, 0x36, 0x40, 0x5A, 0x51, 0x49, 0x22,
)


# Order in which VCConversation reads its tagged fields. Mirrors the grammar
# slots e/f/h/j/g/i/k — the on-disk dword order is the read order.
GRAMMAR_FIELDS = ("conversation_id", "topic", "response_1", "next_conversation_id",
                  "prompt", "response_2", "flags")
GRAMMAR_TAGS = ("e", "f", "h", "j", "g", "i", "k")

# DLL string id range (XFilest.dll dialogue table — verified 1025 entries
# spanning 1..1105).
DLL_ID_MIN = 1
DLL_ID_MAX = 1105

# List-container payload shape.
LIST_CONTAINER_PAYLOAD = 144
LIST_CONTAINER_HEADER = 16
LIST_PAIR_SIZE = 8  # (token u32, dialogue_id u32)

# CNeoPersist class id of VCConversation (used to detect cross-conversation
# handle edges).
VC_CONVERSATION_CLASS_ID = 0x31

# Documented reason strings (kept here so tests / consumers can match).
REASON_NOT_PRESENT = "field-not-present-in-payload"
REASON_NO_RAW = "field-not-present-in-payload"
REASON_OUT_OF_RANGE = "u32-not-in-dll-string-range-and-not-in-extended-index"
REASON_NO_HANDLE = "handle-not-in-extended-index"


def _load_dll_strings(loc_json: Path | str) -> dict[int, str]:
    """Load XFilest.dll string id -> text mapping."""
    d = json.load(open(loc_json, encoding="utf-8"))
    return {int(k): v for k, v in d.get("strings", {}).items()}


def _resolve_string(value: int, dll: dict[int, str]) -> tuple[Optional[str], bool]:
    """Return (text, resolved) for a raw u32 against the DLL table."""
    if DLL_ID_MIN <= value <= DLL_ID_MAX and value in dll:
        return dll[value], True
    return None, False


def _classify_offset(raw: bytes, off: int) -> Optional[dict]:
    """Return ``{class_id, class_name}`` if ``off`` is a valid CNeoPersist
    marker, else ``None``. Byte-direct, no inference.

    The check mirrors the one performed by :class:`HDBRecordIndex` so we
    never decorate a slot with target metadata that cannot be confirmed
    from the raw bytes.
    """
    if off < 0 or off + 8 > len(raw):
        return None
    if not (0xC0 <= raw[off] <= 0xCF):
        return None
    if raw[off + 2:off + 6] != b"\x00\x00\x00\x01":
        return None
    cid = (raw[off + 6] << 8) | raw[off + 7]
    return {"class_id": cid, "class_name": class_label(cid)}


def _resolve_handle(value: int, resolver: Optional[MasterHDBResolver],
                    raw: bytes) -> Optional[dict]:
    """Try to resolve ``value`` as a NeoID through the master extended
    index. Returns ``None`` when the resolver is absent, the value is not
    in the index, or the offset does not land on a real marker.
    """
    if resolver is None:
        return None
    off = resolver.resolve_offset(value)
    if off is None:
        return None
    info = _classify_offset(raw, off)
    if info is None:
        return None
    source = ("extended" if value in resolver.extended_offsets
              else "page-aligned")
    return {
        "offset": off,
        "class_id": info["class_id"],
        "class_name": info["class_name"],
        "source": source,
        "kind": "neoid",
    }


def _resolve_direct_offset(value: int, raw: bytes) -> Optional[dict]:
    """Try to interpret ``value`` as a direct file offset into the HDB.

    A direct offset is honest only when the raw bytes at that offset are
    literally a CNeoPersist marker (``0xC0..0xCF`` followed by
    ``00 00 00 01``). The result has the same shape as
    :func:`_resolve_handle` so callers can treat both kinds uniformly.
    """
    kind, payload = classify_handle(value, len(raw), raw=raw)
    if kind != "direct-offset" or payload is None:
        return None
    info = _classify_offset(raw, payload)
    if info is None:
        return None
    return {
        "offset": payload,
        "class_id": info["class_id"],
        "class_name": info["class_name"],
        "source": "direct-offset",
        "kind": "direct-offset",
    }


def _slot(value: Optional[int], dll: dict[int, str],
          resolver: Optional[MasterHDBResolver] = None,
          raw: Optional[bytes] = None,
          *, reason_if_absent: str = REASON_NOT_PRESENT) -> dict:
    """Build a slot dict for prompt / response_1 / response_2.

    Resolution order:
      1. value is None              -> undetermined (reason_if_absent)
      2. value in DLL string range  -> byte-direct text
      3. value is a known NeoID     -> byte-direct handle (target_*)
      4. otherwise                  -> undetermined (out-of-range reason)
    """
    if value is None:
        return {
            "raw_u32": None,
            "string_id": None,
            "text": None,
            "certainty": "undetermined",
            "reason": reason_if_absent,
        }
    text, resolved = _resolve_string(value, dll)
    if resolved:
        return {
            "raw_u32": value,
            "string_id": value,
            "text": text,
            "certainty": "byte-direct",
        }
    handle = _resolve_handle(value, resolver, raw) if raw is not None else None
    if handle is None and raw is not None:
        handle = _resolve_direct_offset(value, raw)
    if handle is not None:
        return {
            "raw_u32": value,
            "string_id": value,
            "text": None,
            "certainty": "byte-direct",
            "kind": handle["kind"],
            "target_offset": handle["offset"],
            "target_class_id": handle["class_id"],
            "target_class_name": handle["class_name"],
            "resolver_source": handle["source"],
        }
    return {
        "raw_u32": value,
        "string_id": value,
        "text": None,
        "certainty": "undetermined",
        "reason": REASON_OUT_OF_RANGE,
    }


def _be_dwords(payload: bytes) -> list[int]:
    n = len(payload) // 4
    return [struct.unpack_from(">I", payload, i * 4)[0] for i in range(n)]


def _decode_conversation_record(offset: int, payload: bytes,
                                dll: dict[int, str],
                                resolver: Optional[MasterHDBResolver],
                                raw: bytes) -> dict:
    """Decode a regular VCConversation record into the public schema."""
    dwords = _be_dwords(payload)
    # Map read-order dwords to grammar slots; absent slots stay None.
    slot_values: dict[str, Optional[int]] = {name: None for name in GRAMMAR_FIELDS}
    for i, name in enumerate(GRAMMAR_FIELDS):
        if i < len(dwords):
            slot_values[name] = dwords[i]

    # conversation_id (e), topic (f) and next_conversation_id (j) are raw
    # u32 handles. We expose them as-is with byte-direct certainty when they
    # were read from the payload. They also receive a second chance via the
    # extended index (decorated with target_* keys), and we surface
    # incidental DLL-id matches under `dll_text` — honest cross-references,
    # the slot's primary meaning is *not* overridden.
    def _raw_slot(name: str) -> dict:
        v = slot_values[name]
        if v is None:
            return {
                "raw_u32": None,
                "value": None,
                "certainty": "undetermined",
                "reason": REASON_NO_RAW,
            }
        out = {"raw_u32": v, "value": v, "certainty": "byte-direct"}
        text, resolved = _resolve_string(v, dll)
        if resolved:
            out["dll_text"] = text
            out["dll_string_id"] = v
        handle = _resolve_handle(v, resolver, raw)
        if handle is None:
            handle = _resolve_direct_offset(v, raw)
        if handle is not None:
            out["target_offset"] = handle["offset"]
            out["target_class_id"] = handle["class_id"]
            out["target_class_name"] = handle["class_name"]
            out["resolver_source"] = handle["source"]
            out["kind"] = handle["kind"]
        else:
            # Honest reason when the value isn't a registered NeoID handle
            # and is not a byte-direct file offset to a marker.
            out["handle_status"] = REASON_NO_HANDLE
        return out

    record = {
        "record_offset": offset,
        "payload_size": len(payload),
        "kind": "conversation_record",
        "conversation_id": _raw_slot("conversation_id"),
        "topic": _raw_slot("topic"),
        "prompt": _slot(slot_values["prompt"], dll, resolver, raw),
        "response_1": _slot(slot_values["response_1"], dll, resolver, raw),
        "response_2": _slot(slot_values["response_2"], dll, resolver, raw),
        "next_conversation_id": _raw_slot("next_conversation_id"),
        "flags": (
            {"raw_u32": slot_values["flags"],
             "value": slot_values["flags"],
             "certainty": "byte-direct"}
            if slot_values["flags"] is not None
            else {"raw_u32": None, "value": None,
                  "certainty": "undetermined",
                  "reason": REASON_NO_RAW}
        ),
        "raw_dwords": dwords,
    }
    # A record is byte-direct overall when at least one grammar slot resolved
    # (text, handle target, or raw handle u32).
    has_text = any(
        record[k]["certainty"] == "byte-direct" and record[k].get("text")
        for k in ("prompt", "response_1", "response_2")
    )
    has_handle = any(
        record[k]["certainty"] == "byte-direct"
        for k in ("conversation_id", "topic", "next_conversation_id")
    )
    has_handle_target = any(
        record[k]["certainty"] == "byte-direct"
        and record[k].get("target_offset") is not None
        for k in ("prompt", "response_1", "response_2",
                  "conversation_id", "topic", "next_conversation_id")
    )
    record["certainty"] = (
        "byte-direct" if (has_text or has_handle or has_handle_target)
        else "undetermined"
    )
    if not (has_text or has_handle or has_handle_target):
        record["reason"] = "payload-too-short-for-any-grammar-slot"
    return record


def _decode_list_container(offset: int, payload: bytes,
                           dll: dict[int, str],
                           resolver: Optional[MasterHDBResolver],
                           raw: bytes) -> tuple[dict, list[dict], list[dict]]:
    """Decode a 144-byte list container.

    Returns (container_record, entry_records, edges).
    """
    header = payload[:LIST_CONTAINER_HEADER]
    body = payload[LIST_CONTAINER_HEADER:]
    pairs: list[tuple[int, int]] = []
    for i in range(0, len(body) - LIST_PAIR_SIZE + 1, LIST_PAIR_SIZE):
        tok = struct.unpack_from(">I", body, i)[0]
        sid = struct.unpack_from(">I", body, i + 4)[0]
        pairs.append((tok, sid))

    container = {
        "record_offset": offset,
        "payload_size": len(payload),
        "kind": "list_container",
        "header_hex": header.hex(),
        "entry_count": len(pairs),
        "certainty": "byte-direct",
    }

    entry_records: list[dict] = []
    edges: list[dict] = []
    for k, (tok, sid) in enumerate(pairs):
        prompt_slot = _slot(sid, dll, resolver, raw)
        # Conversation id token slot — decorate with handle target when
        # the token is itself a known NeoID.
        token_slot: dict = {
            "raw_u32": tok,
            "value": tok,
            "certainty": "byte-direct",
        }
        tok_handle = _resolve_handle(tok, resolver, raw)
        if tok_handle is None:
            tok_handle = _resolve_direct_offset(tok, raw)
        if tok_handle is not None:
            token_slot["target_offset"] = tok_handle["offset"]
            token_slot["target_class_id"] = tok_handle["class_id"]
            token_slot["target_class_name"] = tok_handle["class_name"]
            token_slot["resolver_source"] = tok_handle["source"]
            token_slot["kind"] = tok_handle["kind"]
        else:
            token_slot["handle_status"] = REASON_NO_HANDLE

        entry_offset = offset + LIST_CONTAINER_HEADER + k * LIST_PAIR_SIZE
        entry = {
            "record_offset": entry_offset,
            "payload_size": LIST_PAIR_SIZE,
            "kind": "list_container_entry",
            "conversation_id": token_slot,
            "topic": {"raw_u32": None, "value": None,
                      "certainty": "undetermined",
                      "reason": "list-entry-has-no-topic-slot"},
            "prompt": prompt_slot,
            "response_1": {"raw_u32": None, "string_id": None, "text": None,
                           "certainty": "undetermined",
                           "reason": "list-entry-has-no-response-slot"},
            "response_2": {"raw_u32": None, "string_id": None, "text": None,
                           "certainty": "undetermined",
                           "reason": "list-entry-has-no-response-slot"},
            "next_conversation_id": {"raw_u32": None, "value": None,
                                     "certainty": "undetermined",
                                     "reason": "list-entry-has-no-next-slot"},
            "flags": {"raw_u32": None, "value": None,
                      "certainty": "undetermined",
                      "reason": "list-entry-has-no-flags-slot"},
            "parent_list_offset": offset,
            "entry_index": k,
            "certainty": ("byte-direct"
                          if prompt_slot["certainty"] == "byte-direct"
                          else "undetermined"),
        }
        if entry["certainty"] == "undetermined":
            entry["reason"] = "dialogue-id-did-not-resolve-against-dll"
        entry_records.append(entry)
        edges.append({
            "from": offset,
            "to": entry_offset,
            "via": "list_container_entry",
            "entry_index": k,
            "certainty": "byte-direct",
        })

    return container, entry_records, edges


def _emit_handle_edges(record: dict, graph_edges: list[dict]) -> None:
    """Emit `handle_target` edges for every slot in ``record`` carrying a
    resolved ``target_offset``. Edges targeting another VCConversation
    record (class id 0x31) are tagged with ``kind=cross_conversation``.
    """
    from_off = record["record_offset"]
    for slot_name in ("prompt", "response_1", "response_2",
                      "conversation_id", "topic", "next_conversation_id"):
        slot = record.get(slot_name)
        if not isinstance(slot, dict):
            continue
        if slot.get("certainty") != "byte-direct":
            continue
        target_off = slot.get("target_offset")
        if target_off is None:
            continue
        edge: dict = {
            "from": from_off,
            "to": target_off,
            "via": "handle_target",
            "slot": slot_name,
            "target_class_id": slot.get("target_class_id"),
            "target_class_name": slot.get("target_class_name"),
            "resolver_source": slot.get("resolver_source"),
            "certainty": "byte-direct",
        }
        if slot.get("target_class_id") == VC_CONVERSATION_CLASS_ID:
            edge["kind"] = "cross_conversation"
        graph_edges.append(edge)


def extract_conversations(container: HdbContainer,
                          dll_strings: dict[int, str],
                          *,
                          resolver: Optional[MasterHDBResolver] = None) -> dict:
    """Walk every VCConversation record and build the public structure.

    The return value matches the on-disk JSON schema and is safe for
    streaming consumers : `records` enumerates regular records, list
    containers, and list-container entries (with a `parent_list_offset`
    back pointer); `graph_edges` exposes only the edges that are
    byte-direct.

    Parameters
    ----------
    container:
        The opened HDB container.
    dll_strings:
        Mapping ``id -> text`` for the XFilest.dll dialogue table.
    resolver:
        Optional pre-built :class:`MasterHDBResolver`. When omitted the
        extractor builds one lazily and extends it with
        ``extend_with_neoidlist_entries(scope='global')`` so handle
        cross-references are available out of the box. Callers running
        multiple extractors should share a single resolver to avoid
        rebuilding the master index.
    """
    raw = container.raw
    idx = HDBRecordIndex(container)

    resolver_extension_info: Optional[dict] = None
    resolver_class_index_info: Optional[dict] = None
    if resolver is None:
        resolver = MasterHDBResolver(container)
        resolver_extension_info = resolver.extend_with_neoidlist_entries(
            scope="global")
        # Class-scoped extension on top of the global scan. Because the
        # global scope already subsumes every pair these classes contain
        # (first-wins collision), the additional ids_added here is
        # typically 0 — that is the honest measurement. We still call it
        # so callers see the per-class hit report and so the wiring
        # exercises the new API in production builds.
        resolver_class_index_info = resolver.extend_with_class_index(
            EXTENDED_PAIR_TABLE_CLASS_IDS)

    class31_records = [r for r in idx.records if r.class_id == 0x31]
    class31_offsets = {r.byte_offset for r in class31_records}

    records: list[dict] = []
    graph_edges: list[dict] = []
    list_container_count = 0
    list_entry_count = 0
    conversation_record_count = 0
    with_resolved_text = 0
    with_resolved_handle = 0
    handles_resolved_extended = 0
    handles_resolved_page_aligned = 0
    handles_resolved_direct_offset = 0
    handles_unresolved = 0
    undetermined_total = 0
    graph_nodes_set: set[int] = set()

    def _tally_slot(slot: dict) -> None:
        nonlocal handles_resolved_extended, handles_resolved_page_aligned
        nonlocal handles_resolved_direct_offset
        nonlocal handles_unresolved, undetermined_total
        src = slot.get("resolver_source")
        if src == "extended":
            handles_resolved_extended += 1
        elif src == "page-aligned":
            handles_resolved_page_aligned += 1
        elif src == "direct-offset":
            handles_resolved_direct_offset += 1
        elif slot.get("certainty") == "undetermined":
            undetermined_total += 1
            # A miss with a raw u32 means the resolver checked but did
            # not find the value in the extended index.
            if slot.get("raw_u32") is not None:
                handles_unresolved += 1

    for r in class31_records:
        payload = raw[r.byte_offset + 8:r.byte_offset + 8 + r.payload_size]
        graph_nodes_set.add(r.byte_offset)

        if r.payload_size == LIST_CONTAINER_PAYLOAD:
            cont, entries, edges = _decode_list_container(
                r.byte_offset, payload, dll_strings, resolver, raw)
            records.append(cont)
            list_container_count += 1
            for e in entries:
                records.append(e)
                graph_nodes_set.add(e["record_offset"])
                list_entry_count += 1
                if e["prompt"]["certainty"] == "byte-direct" and e["prompt"].get("text"):
                    with_resolved_text += 1
                _tally_slot(e["prompt"])
                _tally_slot(e["conversation_id"])
                if any(
                    e.get(k, {}).get("target_offset") is not None
                    for k in ("conversation_id", "prompt")
                ):
                    with_resolved_handle += 1
                _emit_handle_edges(e, graph_edges)
            graph_edges.extend(edges)
        else:
            rec = _decode_conversation_record(
                r.byte_offset, payload, dll_strings, resolver, raw)
            records.append(rec)
            conversation_record_count += 1
            text_present = any(
                rec[k]["certainty"] == "byte-direct" and rec[k].get("text")
                for k in ("prompt", "response_1", "response_2")
            )
            handle_text_present = any(
                rec[k].get("dll_text")
                for k in ("conversation_id", "topic", "next_conversation_id")
            )
            handle_target_present = any(
                rec[k].get("target_offset") is not None
                for k in ("prompt", "response_1", "response_2",
                          "conversation_id", "topic", "next_conversation_id")
            )
            if text_present or handle_text_present:
                with_resolved_text += 1
            if handle_target_present:
                with_resolved_handle += 1
            for slot_name in ("prompt", "response_1", "response_2",
                              "conversation_id", "topic",
                              "next_conversation_id"):
                _tally_slot(rec[slot_name])
            # next-link edge when j (next_conversation_id) hits another
            # class-0x31 record byte offset
            nxt = rec["next_conversation_id"].get("value")
            if (nxt is not None and nxt in class31_offsets
                    and nxt != r.byte_offset):
                graph_edges.append({
                    "from": r.byte_offset,
                    "to": nxt,
                    "via": "next_link",
                    "certainty": "byte-direct",
                })
            _emit_handle_edges(rec, graph_edges)

    stats = {
        "records_total": len(records),
        "graph_nodes": len(graph_nodes_set),
        "with_resolved_text": with_resolved_text,
        "with_resolved_handle": with_resolved_handle,
        "graph_edges": len(graph_edges),
        "conversation_records": conversation_record_count,
        "list_containers": list_container_count,
        "list_container_entries": list_entry_count,
        "dll_strings_available": len(dll_strings),
        # Handle-resolution stats (extension-layer instrumentation).
        "dll_resolved": with_resolved_text,
        "handles_resolved": (handles_resolved_extended
                             + handles_resolved_page_aligned
                             + handles_resolved_direct_offset),
        "handles_resolved_extended": handles_resolved_extended,
        "handles_resolved_page_aligned": handles_resolved_page_aligned,
        "handles_resolved_direct_offset": handles_resolved_direct_offset,
        "handles_unresolved": handles_unresolved,
        "undetermined_total": undetermined_total,
    }
    if resolver_extension_info is not None:
        stats["resolver_extension_added"] = resolver_extension_info["added"]
        stats["resolver_extension_total"] = resolver_extension_info["after"]
    if resolver_class_index_info is not None:
        stats["resolver_class_index_added"] = resolver_class_index_info["ids_added"]
        stats["resolver_class_index_classes_scanned"] = (
            resolver_class_index_info["classes_scanned"])
        stats["resolver_class_index_total"] = (
            resolver_class_index_info["total_ids_after"])

    return {
        "_source": ("XFILES.HDB class 0x31 (VCConversation) + XFilest.dll "
                    "dialogue strings 1..1105 + MasterHDBResolver extended "
                    "index"),
        "_about": ("Byte-direct walk of every VCConversation record. The "
                   "grammar slots (e=conversation_id, f=topic, h=response_1, "
                   "j=next, g=prompt, i=response_2, k=flags) are mapped from "
                   "the read-order dwords of each on-disk payload. The 144B "
                   "list container is split into its (token, dialogue_id) "
                   "pairs and surfaced as list_container_entry records. "
                   "Prompt / response / dialogue_id values are first "
                   "resolved against the XFilest.dll string table; values "
                   "outside that range are cross-referenced through the "
                   "MasterHDBResolver extended index (CNeoIDList / "
                   "CNeoIDIndex pair tables) and decorated with "
                   "target_offset / target_class_id when the lookup hits a "
                   "real CNeoPersist marker. Fields that resolve neither "
                   "way keep their raw value and carry "
                   "certainty=undetermined with a documented reason."),
        "stats": stats,
        "records": records,
        "graph_edges": graph_edges,
    }


def write_conversations_json(hdb_path: str, loc_json: str, out_path: str) -> dict:
    """Convenience wrapper : load HDB + DLL strings, write the result JSON."""
    container = HdbContainer.from_path(hdb_path)
    strings = _load_dll_strings(loc_json)
    result = extract_conversations(container, strings)
    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    json.dump(result, open(out_path, "w", encoding="utf-8"),
              indent=1, ensure_ascii=False)
    return result["stats"]
