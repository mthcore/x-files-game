"""Leaf-record full resolution : extract identifier strings from target pages.

Each leaf-page record (24 bytes) is :
  [0..7]  : marker [tag][sub_tag][00 00 00 01][flag16 BE]
  [8..11] : value32 BE = (target_page_id << 16) | offset_in_page
  [12..15]: field_12 (often = (related_class_id << 16) | 0)
  [16..19]: field_16 (cumulative cross-page offset)
  [20..23]: field_20 (additional metadata / version stamp)

When `flag16` is in the VC class_id range (0x27..0x5C), the record is an
INDEX ENTRY for a true VC* instance whose data lives at :
  target_page_id = value32 >> 16
  offset_in_page = value32 & 0xFFFF (usually 0)

This module:
  1. Walks all leaf pages (0xC3)
  2. For each VC*-typed entry, resolves the target page
  3. Extracts printable strings + structure from the target page
  4. Emits a rich JSON catalog of resolved records
"""
from __future__ import annotations

import json
import struct
from dataclasses import dataclass, field
from pathlib import Path

from hdb_extract.btree.master_resolver import parse_page_records
from hdb_extract.classes.registry import CLASSES_BY_ID
from hdb_extract.container.container import HdbContainer


PAGE_SIZE = 256


@dataclass
class ResolvedLeafRecord:
    """A VC* instance fully resolved from leaf page → target page."""

    page_id: int
    rec_idx: int
    class_id: int
    class_name: str
    marker_tag: int
    marker_sub_tag: int
    value32: int = 0
    field_12: int = 0
    field_16: int = 0
    field_20: int = 0
    target_page_id: int = 0
    target_page_tag: int = 0
    target_strings: list[str] = field(default_factory=list)
    target_first_64_hex: str = ""


def extract_strings(payload: bytes, min_len: int = 4) -> list[str]:
    """Extract printable ASCII strings of length >= min_len from bytes."""
    strings: list[str] = []
    cur: list[str] = []
    for b in payload:
        if 32 <= b < 127:
            cur.append(chr(b))
        else:
            if len(cur) >= min_len:
                strings.append("".join(cur))
            cur = []
    if len(cur) >= min_len:
        strings.append("".join(cur))
    return strings


def resolve_leaf_records(container: HdbContainer) -> list[ResolvedLeafRecord]:
    """Walk leaf pages and resolve each VC*-typed record to its target page."""
    out: list[ResolvedLeafRecord] = []
    page_count = len(container.raw) // PAGE_SIZE

    for pi in range(page_count):
        page = container.page(pi)
        if page[0] != 0xC3:
            continue

        recs = parse_page_records(page)
        for ri, r in enumerate(recs):
            if r.flag16 not in CLASSES_BY_ID:
                continue
            spec = CLASSES_BY_ID[r.flag16]
            off = r.offset_in_page
            full = page[off:off + 24]
            if len(full) < 24:
                continue

            value32 = struct.unpack(">I", full[8:12])[0]
            f12 = struct.unpack(">I", full[12:16])[0]
            f16 = struct.unpack(">I", full[16:20])[0]
            f20 = struct.unpack(">I", full[20:24])[0]

            target_page_id = value32 >> 16
            target_first_64 = ""
            target_strings: list[str] = []
            target_tag = 0
            if 0 < target_page_id < page_count:
                tp = container.page(target_page_id)
                target_tag = tp[0]
                target_first_64 = tp[:64].hex()
                target_strings = extract_strings(tp)

            out.append(ResolvedLeafRecord(
                page_id=pi,
                rec_idx=ri,
                class_id=r.flag16,
                class_name=spec.name,
                marker_tag=r.marker_tag,
                marker_sub_tag=r.marker_sub_tag,
                value32=value32,
                field_12=f12,
                field_16=f16,
                field_20=f20,
                target_page_id=target_page_id,
                target_page_tag=target_tag,
                target_strings=target_strings,
                target_first_64_hex=target_first_64,
            ))
    return out


def build_resolved_catalog(container: HdbContainer) -> dict:
    """Full resolved catalog as a JSON-serializable dict."""
    records = resolve_leaf_records(container)
    by_class: dict[str, int] = {}
    by_class_with_strings: dict[str, int] = {}
    for r in records:
        by_class[r.class_name] = by_class.get(r.class_name, 0) + 1
        if r.target_strings:
            by_class_with_strings[r.class_name] = (
                by_class_with_strings.get(r.class_name, 0) + 1)

    return {
        "_source": "F18 leaf_resolver — full leaf-to-target resolution",
        "_methodology": (
            "Walk 0xC3 leaf pages, find records with flag16 in class_id range, "
            "extract value32 high 16 bits as target_page_id, fetch that page's "
            "raw bytes + extract printable strings."
        ),
        "total_resolved": len(records),
        "by_class": dict(sorted(by_class.items(), key=lambda x: -x[1])),
        "by_class_with_strings": dict(sorted(by_class_with_strings.items(),
                                              key=lambda x: -x[1])),
        "records": [
            {
                "page_id": r.page_id,
                "rec_idx": r.rec_idx,
                "class_id": r.class_id,
                "class_name": r.class_name,
                "marker_tag": r.marker_tag,
                "marker_sub_tag": r.marker_sub_tag,
                "value32_hex": f"0x{r.value32:08x}",
                "field_12_hex": f"0x{r.field_12:08x}",
                "field_16_hex": f"0x{r.field_16:08x}",
                "field_20_hex": f"0x{r.field_20:08x}",
                "target_page_id": r.target_page_id,
                "target_page_tag_hex": f"0x{r.target_page_tag:02x}",
                "target_strings": r.target_strings,
                "target_first_64_hex": r.target_first_64_hex,
            }
            for r in records
        ],
    }
