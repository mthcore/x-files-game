"""B-tree page inventory for ``XFILES.HDB``.

Each 256-byte page begins with a one-byte tag (``0xC0``/``0xC2``/``0xC3``/
``0xD2``/``0x00``/...) followed by a one-byte ``kind``. This module walks the
file page by page and tabulates the ``(tag, kind)`` distribution plus a few
byte-direct shape statistics — record marker count, zero-tail size, and the
two-byte ``flag16`` at +4..+5 (big-endian) per page.

The semantic meaning of ``kind`` for B-tree pages is **not** in the public
NeoPersist 3.0 headers and is honestly left ``undetermined``. The inventory
just surfaces what the bytes show — counts, ranges, and a sample of records.
"""
from __future__ import annotations

import json
import struct
from collections import Counter
from pathlib import Path

PAGE_SIZE = 256
MARKER_INVARIANT = b"\x00\x00\x00\x01"   # bytes [+2..+6) of a CNeoPersist marker


def _walk_pages(raw: bytes) -> list[dict]:
    out = []
    n = len(raw) // PAGE_SIZE
    for p in range(n):
        off = p * PAGE_SIZE
        page = raw[off:off + PAGE_SIZE]
        tag = page[0]
        kind = page[1]
        flag16 = struct.unpack_from(">H", page, 6)[0]
        # Count CNeoPersist markers (record starts) inside the page body.
        marker_count = 0
        for i in range(8, PAGE_SIZE - 8, 8):
            if page[i + 2:i + 6] == MARKER_INVARIANT:
                marker_count += 1
        # Zero-tail size.
        z = PAGE_SIZE
        while z > 0 and page[z - 1] == 0:
            z -= 1
        zero_tail = PAGE_SIZE - z
        out.append({
            "page_id": p,
            "tag": tag,
            "kind": kind,
            "flag16": flag16,
            "marker_count": marker_count,
            "zero_tail": zero_tail,
        })
    return out


def build_btree_inventory(hdb_path: str | Path) -> dict:
    raw = Path(hdb_path).read_bytes()
    pages = _walk_pages(raw)
    by_tag_kind: dict[tuple[int, int], dict] = {}
    for p in pages:
        key = (p["tag"], p["kind"])
        bucket = by_tag_kind.setdefault(key, {
            "tag": p["tag"], "kind": p["kind"],
            "page_count": 0,
            "marker_total": 0,
            "marker_min": 0xFFFF,
            "marker_max": 0,
            "zero_tail_min": PAGE_SIZE,
            "zero_tail_max": 0,
            "sample_page_ids": [],
        })
        bucket["page_count"] += 1
        bucket["marker_total"] += p["marker_count"]
        bucket["marker_min"] = min(bucket["marker_min"], p["marker_count"])
        bucket["marker_max"] = max(bucket["marker_max"], p["marker_count"])
        bucket["zero_tail_min"] = min(bucket["zero_tail_min"], p["zero_tail"])
        bucket["zero_tail_max"] = max(bucket["zero_tail_max"], p["zero_tail"])
        if len(bucket["sample_page_ids"]) < 4:
            bucket["sample_page_ids"].append(p["page_id"])

    # Promote each bucket to a JSON-friendly entry with certainty markers.
    distribution: list[dict] = []
    for (tag, kind), b in sorted(by_tag_kind.items()):
        if b["marker_min"] > b["marker_max"]:
            b["marker_min"] = 0
        distribution.append({
            "tag": tag, "kind": kind,
            "tag_hex": f"0x{tag:02x}", "kind_hex": f"0x{kind:02x}",
            "page_count": b["page_count"],
            "marker_total": b["marker_total"],
            "marker_per_page": {"min": b["marker_min"], "max": b["marker_max"]},
            "zero_tail": {"min": b["zero_tail_min"], "max": b["zero_tail_max"]},
            "sample_page_ids": b["sample_page_ids"],
            "semantic_meaning": {
                "value": None,
                "certainty": "undetermined",
                "reason": "kind-semantics-not-in-public-format",
            },
            "certainty": "byte-direct",
        })

    # Per-tag rollup.
    by_tag: dict[int, int] = Counter()
    for p in pages:
        by_tag[p["tag"]] += 1

    return {
        "_about": "Page-by-page (tag, kind) inventory for the HDB. Counts and "
                  "shape statistics are byte-direct; the semantic meaning of "
                  "the kind byte is left undetermined per the public format.",
        "_source": str(hdb_path),
        "stats": {
            "page_size": PAGE_SIZE,
            "pages_total": len(pages),
            "tag_kind_combinations": len(distribution),
            "distinct_tags": len(by_tag),
        },
        "by_tag_summary": [
            {"tag": t, "tag_hex": f"0x{t:02x}", "page_count": c,
             "certainty": "byte-direct"}
            for t, c in sorted(by_tag.items())
        ],
        "distribution": distribution,
    }


def write_btree_inventory(hdb_path: str | Path,
                           out_path: str | Path) -> dict:
    inv = build_btree_inventory(hdb_path)
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as f:
        json.dump(inv, f, indent=1, ensure_ascii=False)
    return inv["stats"]
