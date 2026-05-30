"""HSPT hotspot decoder for ``XV/<scene_id>.HOT`` files.

Format (byte-direct, validated against the C++ engine in ``cpp/src/engine/``):

    header  : 'HSPT' (4 B) + count (u32 LE, 4 B)
    entry   : type_zorder (u32 LE, 4 B)
              x_min, y_min, x_max, y_max (i16 LE, 2 B × 4)
              action_id_1 (u32 LE, 4 B)
              action_id_2 (u32 LE, 4 B)
              total = 20 B per entry

File size invariant: ``8 + n * 20``. Anything else is rejected.

Output JSON shape (one file per scene, plus an aggregate inventory)::

    {
      "scene_id": 19672,
      "rect_count": 4,
      "rects": [
        {"index": 0, "type_zorder": 0, "x_min": 600, "y_min": 0,
         "x_max": 640, "y_max": 44, "action_id_1": 1033, "action_id_2": 0,
         "certainty": "byte-direct"},
        ...
      ],
      "_source": "XV/19672.HOT"
    }

Every numeric value is read directly from the file bytes; no inference. The
``action_id`` semantics are exposed raw — the mapping from ``action_id`` to a
runtime effect is not part of the public format and is honestly left to the
downstream consumer.
"""
from __future__ import annotations

import json
import struct
from pathlib import Path


_HEADER = b"HSPT"
_ENTRY_SIZE = 20
_HEADER_SIZE = 8


def parse_hot_bytes(buf: bytes, scene_id: int | None = None,
                     source: str | None = None) -> dict | None:
    if len(buf) < _HEADER_SIZE or buf[:4] != _HEADER:
        return None
    (n,) = struct.unpack_from("<I", buf, 4)
    if len(buf) != _HEADER_SIZE + n * _ENTRY_SIZE:
        return None
    rects = []
    for i in range(n):
        off = _HEADER_SIZE + i * _ENTRY_SIZE
        (type_zorder,) = struct.unpack_from("<I", buf, off)
        x_min, y_min, x_max, y_max = struct.unpack_from("<hhhh", buf, off + 4)
        action_id_1, action_id_2 = struct.unpack_from("<II", buf, off + 12)
        rects.append({
            "index": i,
            "type_zorder": type_zorder,
            "x_min": x_min, "y_min": y_min,
            "x_max": x_max, "y_max": y_max,
            "action_id_1": action_id_1,
            "action_id_2": action_id_2,
            "certainty": "byte-direct",
        })
    out: dict = {
        "scene_id": scene_id,
        "rect_count": n,
        "rects": rects,
        "certainty": "byte-direct",
    }
    if source:
        out["_source"] = source
    return out


def parse_hot_file(path: str | Path) -> dict | None:
    path = Path(path)
    try:
        scene_id = int(path.stem)
    except ValueError:
        scene_id = None
    buf = path.read_bytes()
    return parse_hot_bytes(buf, scene_id=scene_id, source=path.name)


def build_hotspots_inventory(xv_dir: str | Path) -> dict:
    """Scan ``xv_dir`` for ``*.HOT`` files, parse each, and return an
    aggregate inventory plus per-scene records.
    """
    xv = Path(xv_dir)
    scenes: list[dict] = []
    rect_total = 0
    skipped = 0
    action_id_counts: dict[int, int] = {}
    for p in sorted(xv.glob("*.HOT")):
        rec = parse_hot_file(p)
        if rec is None:
            skipped += 1
            continue
        scenes.append(rec)
        rect_total += rec["rect_count"]
        for r in rec["rects"]:
            for slot in ("action_id_1", "action_id_2"):
                aid = r[slot]
                if aid:
                    action_id_counts[aid] = action_id_counts.get(aid, 0) + 1

    # Ranked frequency table (descending by count, ascending action_id for ties).
    ranked = sorted(action_id_counts.items(),
                     key=lambda kv: (-kv[1], kv[0]))
    by_frequency = [
        {"action_id": aid, "occurrences": count, "certainty": "byte-direct"}
        for aid, count in ranked
    ]
    return {
        "_about": "Inventory of every HSPT hotspot file in the supplied XV "
                  "directory. Each rect's geometry and action_ids are read "
                  "byte-direct; action_id semantics are not in the public "
                  "format and are surfaced raw.",
        "stats": {
            "scenes_total": len(scenes),
            "rects_total": rect_total,
            "action_ids_distinct": len(action_id_counts),
            "skipped_files": skipped,
            "action_id_range": {
                "min": min(action_id_counts) if action_id_counts else 0,
                "max": max(action_id_counts) if action_id_counts else 0,
            },
            "top_action_ids_top_count": (ranked[0][1] if ranked else 0),
        },
        "action_ids_by_frequency": by_frequency,
        "scenes": scenes,
    }


def write_hotspots_inventory(xv_dir: str | Path,
                              out_path: str | Path) -> dict:
    inv = build_hotspots_inventory(xv_dir)
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as f:
        json.dump(inv, f, indent=1, ensure_ascii=False)
    return inv["stats"]
