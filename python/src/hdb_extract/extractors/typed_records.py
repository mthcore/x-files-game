"""Typed-record decoder : identifies class via marker_flag16 == class_id.

In the 8-byte HDB record marker
`[tag][sub_tag][00 00 00 01 magic][flag16 BE]`, the flag16 field
holds the class_id for ~2,371 records (those that ARE VC* class instances).

Records where flag16 is in 0x27..0x5C (the legitimate class_id range)
are decoded with the matching ClassSpec from the registry, applied to
the payload bytes (after 8-byte marker skip).

Records where flag16 is 0 or out-of-range are container/subordinate
records (B-tree linkage, sub-records, system metadata) — not VC* class
instances themselves. They're catalogued but not class-decoded.
"""
from __future__ import annotations

from typing import Any

from hdb_extract.classes.registry import CLASSES_BY_ID, apply_read, spec_for
from hdb_extract.container.container import HdbContainer
from hdb_extract.serializer.packed import PackedHDBSerializer


PAGE_SIZE = 256
MARKER_MAGIC = bytes([0x00, 0x00, 0x00, 0x01])


def _serialize(v):
    """JSON-safe : bytes -> hex, recurse into dicts/lists."""
    if isinstance(v, bytes):
        return v.hex()
    if isinstance(v, dict):
        return {k: _serialize(x) for k, x in v.items()}
    if isinstance(v, list):
        return [_serialize(x) for x in v]
    return v


def find_markers_in_page(page: bytes) -> list[int]:
    """Return offsets within page where 8-byte markers start."""
    offs = []
    for off in range(0, PAGE_SIZE - 8 + 1, 8):
        if 0xC0 <= page[off] <= 0xCF and page[off + 2:off + 6] == MARKER_MAGIC:
            offs.append(off)
    return offs


def decode_typed_record(
    raw: bytes,
    payload_offset_abs: int,
    payload_size: int,
    class_id: int,
) -> dict[str, Any] | None:
    """Decode payload bytes as `class_id`. Returns record dict or None."""
    spec = spec_for(class_id)
    if spec is None:
        return None
    buf = raw[payload_offset_abs:payload_offset_abs + payload_size]
    ser = PackedHDBSerializer(buf, absolute_offset=payload_offset_abs)
    try:
        result = apply_read(ser, spec)
    except Exception:
        return None
    return {
        "class_id": class_id,
        "class_name": spec.name,
        "payload_offset": payload_offset_abs,
        "payload_size": payload_size,
        "bytes_consumed": ser.cursor,
        "fields": {k: _serialize(v) for k, v in result.items()
                   if not k.startswith("__")},
    }


def extract_typed_records(container: HdbContainer) -> dict[str, Any]:
    """Walk every HDB page, identify VC* records via flag16 = class_id.

    Returns a structured catalog with :
      - `typed_records` : records decoded as a known VC* class
      - `container_records` : records with flag16=0 or out-of-class-range
      - `class_id_distribution` : count of typed records per class
    """
    raw = container.raw

    typed: list[dict] = []
    container_recs: list[dict] = []
    by_class: dict[str, int] = {}

    page_count = len(raw) // PAGE_SIZE
    for page_id in range(page_count):
        page_off = page_id * PAGE_SIZE
        page = raw[page_off:page_off + PAGE_SIZE]
        if len(page) < PAGE_SIZE:
            continue
        if not (0xC0 <= page[0] <= 0xCF):
            continue

        marker_offs = find_markers_in_page(page)
        if not marker_offs:
            continue

        for i, m_off in enumerate(marker_offs):
            next_off = (marker_offs[i + 1] if i + 1 < len(marker_offs)
                        else PAGE_SIZE)
            payload_off = m_off + 8
            payload_size = next_off - payload_off
            tag = page[m_off]
            sub_tag = page[m_off + 1]
            flag16 = (page[m_off + 6] << 8) | page[m_off + 7]

            marker_meta = {
                "page_id": page_id,
                "marker_offset_abs": page_off + m_off,
                "payload_offset_abs": page_off + payload_off,
                "marker_tag": tag,
                "marker_sub_tag": sub_tag,
                "marker_flag16": flag16,
                "payload_size": payload_size,
            }

            if flag16 in CLASSES_BY_ID:
                # IS a typed VC* class record
                decoded = decode_typed_record(
                    raw,
                    page_off + payload_off,
                    payload_size,
                    flag16,
                )
                if decoded:
                    typed.append({**marker_meta, **decoded})
                    by_class[decoded["class_name"]] = (
                        by_class.get(decoded["class_name"], 0) + 1)
                else:
                    container_recs.append({**marker_meta,
                                            "_decode_failed": True})
            else:
                container_recs.append(marker_meta)

    return {
        "_source": "Typed-record decoder — "
                   "flag16 in marker = class_id for VC* records",
        "_methodology": "1) Walk 0xC0..0xCF pages, 2) parse 8-byte markers, "
                        "3) if marker_flag16 in CLASSES_BY_ID, decode with "
                        "that class's grammar.",
        "total_typed": len(typed),
        "total_container": len(container_recs),
        "by_class": dict(sorted(by_class.items(), key=lambda x: -x[1])),
        "typed_records": typed,
        "container_records": container_recs,
    }
