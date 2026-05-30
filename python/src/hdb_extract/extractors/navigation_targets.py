"""navigation_targets.json — byte-direct labels of every VCNav record.

VCNav records (class ``0x33``) describe navigation links — door / exit /
loop / pan transitions the player can take between scenes. The on-disk
records embed an inline label that names the target (e.g. the destination
node + location, or a side-door tag like ``(porch SE)``). This module
walks every VCNav record, pulls out the longest printable run from its
payload, and reports it byte-direct.

The full record grammar (e/f/g NeoID handles + optional metadata) is
intentionally not decoded here — that grammar varies by sub-kind and is
captured elsewhere. The label alone is enough for downstream tools to
build a coarse graph: each VCNav is a navigation point, the label is
its human-readable name.

Output shape::

    {
      "_about": "...",
      "_source": "XFILES.HDB",
      "stats": { count, with_label, without_label, by_size_class },
      "entries": [
        { record_offset, payload_size, label, certainty, source }
      ]
    }
"""
from __future__ import annotations

import json
from pathlib import Path

from hdb_extract.container.container import HdbContainer
from hdb_extract.btree.record_index import HDBRecordIndex


_VCNAV_CLASS_ID = 0x33


def _longest_printable_run(buf: bytes, min_len: int = 4) -> str:
    """Return the longest contiguous ASCII printable run inside `buf`.

    Tabs (\\x09), spaces, and 0x7f are treated as printable so the X-Files
    inline-label separator (``\\x7f`` = ``DEL``) is preserved for the
    downstream caller (replaced with ``/`` for display below).
    """
    best = b""
    cur = bytearray()
    for ch in buf:
        if 0x20 <= ch <= 0x7e or ch == 0x7f or ch == 0x09:
            cur.append(ch)
        else:
            if len(cur) > len(best):
                best = bytes(cur)
            cur = bytearray()
    if len(cur) > len(best):
        best = bytes(cur)
    if len(best) < min_len:
        return ""
    out = []
    for ch in best:
        if ch == 0x7f:
            out.append("/")
        elif 0x20 <= ch < 0x7f:
            out.append(chr(ch))
        else:
            out.append(" ")
    return "".join(out).strip()


def build_navigation_targets(hdb_path: str | Path) -> dict:
    container = HdbContainer.from_path(hdb_path)
    raw = container.raw
    idx = HDBRecordIndex(container)

    entries: list[dict] = []
    size_class_counts: dict[int, int] = {}
    with_label = 0

    for r in idx.records:
        if r.class_id != _VCNAV_CLASS_ID:
            continue
        end = r.byte_offset + 8 + r.payload_size
        payload = bytes(raw[r.byte_offset + 8:end])
        label = _longest_printable_run(payload)
        sz_bucket = (r.payload_size // 16) * 16  # bucketize by 16-byte
        size_class_counts[sz_bucket] = size_class_counts.get(sz_bucket, 0) + 1
        if label:
            with_label += 1
        entries.append({
            "record_offset": r.byte_offset,
            "payload_size": r.payload_size,
            "label": label,
            "certainty": "byte-direct",
            "source": "HDB:VCNav-inline-label",
        })

    entries.sort(key=lambda e: e["record_offset"])
    return {
        "_about": "Byte-direct labels of every VCNav (class 0x33) record. "
                  "Each entry names a navigation point; the label is the "
                  "longest printable run inside the record's payload.",
        "_source": str(hdb_path),
        "stats": {
            "count": len(entries),
            "with_label": with_label,
            "without_label": len(entries) - with_label,
            "by_size_class": {str(k): v for k, v
                              in sorted(size_class_counts.items())},
        },
        "entries": entries,
    }


def write_navigation_targets(hdb_path: str | Path,
                              out_path: str | Path) -> dict:
    nt = build_navigation_targets(hdb_path)
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as f:
        json.dump(nt, f, indent=1, ensure_ascii=False)
    return nt["stats"]
