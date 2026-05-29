"""Real byte-direct VCTrigger decoder — mirrors the C++ reference decoder (see `cpp/`).

This REPLACES the heuristic `triggers_full.json` (which reconstructed
triggers from `bAI*` variable names). Every trigger here is read
byte-direct from its HDB record, and every condition/action sub-record
is resolved via its embedded byte_offset — zero inference.

VCTrigger record layout (payload after 8-byte marker):
    +0x00  trigger_id  (4B BE)
    +0x04  field2      (4B BE)
    +0x08  field3      (4B BE)
    +0x18  pairs[] : each pair = [byte_offset 4B BE][neoid 4B BE]
            byte_offset points DIRECTLY to the sub-record's marker
            (with ±4/8/16 alignment adjustment), neoid is its NeoID.

Sub-record classes (target_class_id): 0x0f/0x10/0x11/0x14/0x15/0x1b/0x1c/0x1d
are UNREGISTERED CNeoPart sub-records (managed by CNeoPartMgr 0x0a), NOT the
fabricated "HBExpression/HBActionBody" classes. Their 16-byte payload
is decoded byte-direct by hb_subrecords.decode_part_ref; the +14 relation
byte's semantics are honestly left `undetermined`. Triggers also reference
real VC* targets (0x33 VCNav, 0x34 VCNav_List, 0x2c VCCharacter, 0x51
VCTrigger, 0x52 VCTriggerList, ...) which form the game graph (Pilier 4).
"""
from __future__ import annotations

import struct
from dataclasses import dataclass, field

from hdb_extract.btree.record_index import (
    HDBRecordIndex, class_label, kClassVCTrigger, PART_SUBRECORD_IDS,
)
from hdb_extract.extractors.hb_subrecords import decode_part_ref

MARKER_MAGIC = b"\x00\x00\x00\x01"
_DELTAS = (0, -4, -8, 4, 8, -16, 16)


@dataclass
class TriggerPair:
    byte_offset: int
    neoid: int
    target_class_id: int          # 0 = unresolved
    target_class_name: str
    target_payload_size: int
    certainty: str                # "byte-direct" | "unresolved"
    part_fields: dict | None = None   # decoded CNeoPart sub-record (byte-direct) or None


@dataclass
class DecodedTrigger:
    marker_offset: int
    page_id: int
    neo_id: int | None             # true NeoID via master-index inversion (byte-direct) or None
    header_raw: bytes              # payload +0..+0x17 (24B) — structure not fully decoded
    payload_size: int
    pairs: list[TriggerPair] = field(default_factory=list)
    neo_id_certainty: str = "byte-direct"  # or "undetermined" if not in master index


def _resolve_raw_marker(raw: bytes, boff: int) -> tuple[int, int] | None:
    """Return (adjusted_offset, class_id) if a marker sits at/near boff."""
    size = len(raw)

    def check(off: int) -> bool:
        return 0 <= off and off + 8 <= size and raw[off + 2:off + 6] == MARKER_MAGIC

    if boff < 32 or boff + 8 > size:
        return None
    for d in _DELTAS:
        adj = boff + d
        if adj < 32 or adj + 8 > size:
            continue
        if check(adj):
            cid = (raw[adj + 6] << 8) | raw[adj + 7]
            return adj, cid
    return None


def decode_trigger(index: HDBRecordIndex, marker_offset: int,
                   offset_to_neoid: dict[int, int] | None = None) -> DecodedTrigger | None:
    ref = index.find_by_offset(marker_offset)
    if ref is None or ref.class_id != kClassVCTrigger:
        return None
    raw = index.raw
    size = index.size

    # True NeoID via master-index inversion (byte-direct). Reading the
    # trigger_id from payload+0 is WRONG (yields aberrant values like
    # 452460544); the NeoID comes from the index, not the record header.
    neo_id = offset_to_neoid.get(marker_offset) if offset_to_neoid else None
    neo_cert = "byte-direct" if neo_id is not None else "undetermined"

    if ref.payload_size < 24:
        return DecodedTrigger(marker_offset, ref.page_id, neo_id, b"",
                              ref.payload_size, [], neo_cert)

    pbase = marker_offset + 8
    # Header (+0..+0x17) structure not fully decoded → expose as raw bytes.
    header_raw = bytes(raw[pbase:pbase + 0x18])

    out = DecodedTrigger(marker_offset, ref.page_id, neo_id, header_raw,
                         ref.payload_size, [], neo_cert)

    # Pairs region starts at payload +0x18.
    mark = 0x18
    while mark + 8 <= ref.payload_size:
        boff = struct.unpack_from(">I", raw, pbase + mark)[0]
        nid = struct.unpack_from(">I", raw, pbase + mark + 4)[0]
        mark += 8
        if boff < 100 or boff > 6_100_000:
            continue
        if nid == 0 or nid > 1_000_000:
            continue
        resolved = _resolve_raw_marker(raw, boff)
        if resolved is None:
            out.pairs.append(TriggerPair(boff, nid, 0, "?", 0, "unresolved"))
            continue
        adj_off, cid = resolved
        # payload size: indexed record, else scan to next aligned marker
        indexed = index.find_by_offset(adj_off)
        if indexed is not None:
            psize = indexed.payload_size
        else:
            end = adj_off + 8
            limit = min(adj_off + 256, size)
            while end + 8 <= limit:
                if ((end - adj_off) % 8 == 0 and end + 6 <= size
                        and raw[end + 2:end + 6] == MARKER_MAGIC):
                    cid2 = (raw[end + 6] << 8) | raw[end + 7]
                    if cid2 <= 0x5C:
                        break
                end += 8
            psize = (end - adj_off - 8) if end > adj_off + 8 else 8
        part_fields = None
        if cid in PART_SUBRECORD_IDS and psize >= 16:
            part_fields = decode_part_ref(cid, raw[adj_off + 8:adj_off + 8 + psize])
        out.pairs.append(TriggerPair(
            byte_offset=adj_off, neoid=nid, target_class_id=cid,
            target_class_name=class_label(cid),
            target_payload_size=psize, certainty="byte-direct",
            part_fields=part_fields,
        ))
    return out


def decode_all_triggers(index: HDBRecordIndex,
                        offset_to_neoid: dict[int, int] | None = None) -> list[DecodedTrigger]:
    out = []
    for r in index.find_by_class(kClassVCTrigger):
        t = decode_trigger(index, r.byte_offset, offset_to_neoid)
        if t is not None:
            out.append(t)
    return out
