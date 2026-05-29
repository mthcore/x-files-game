"""`.HOT` hotspot-file decoder — 100% byte-direct (round-trip proven 680/680).

Hotspot clickable geometry does NOT live in the HDB: it lives in the sibling
`XV/*.HOT` files (NeoLogic 'HSPT' format). Each file is one scene; each 20-byte
entry is one clickable rectangle + its action ids. The wire format is fully
byte-direct (established from the on-disk structure, byte-identical round-trip on
the full 680-file set).

Header (8B):  'HSPT' (4B) + entry_count (u32 LE)
Entry (20B):  type/z-order (u32 LE) + x_min,y_min,x_max,y_max (i16 LE x4)
              + action_id_1 (u32 LE) + action_id_2 (u32 LE)

Every field below is read byte-direct. The only thing NOT asserted is the
*semantic* meaning of the `type`/z-order number (e.g. "600 = interactive") — that
is a hypothesis in hot.md and is therefore NOT emitted; we expose the raw number.
"""
from __future__ import annotations

import struct
from pathlib import Path

_HEADER = struct.Struct("<4sI")
_ENTRY = struct.Struct("<I4hII")
_HEADER_SIZE = 8
_ENTRY_SIZE = 20


def parse_hot_file(raw: bytes) -> list[dict] | None:
    """Parse one .HOT file's bytes into hotspot entries (byte-direct).
    Returns None if the magic/size invariant does not hold (never guesses)."""
    if len(raw) < _HEADER_SIZE:
        return None
    magic, count = _HEADER.unpack_from(raw, 0)
    if magic != b"HSPT":
        return None
    if len(raw) != _HEADER_SIZE + count * _ENTRY_SIZE:
        return None
    out = []
    for i in range(count):
        t, x0, y0, x1, y1, a1, a2 = _ENTRY.unpack_from(raw, _HEADER_SIZE + i * _ENTRY_SIZE)
        out.append({
            "index": i,
            "type_zorder": t,                 # raw number; semantic NOT asserted
            "rect": {"x_min": x0, "y_min": y0, "x_max": x1, "y_max": y1},
            "action_id_1": a1,
            "action_id_2": a2,
            "certainty": "byte-direct",
        })
    return out


def extract_hotspots(hot_dir: str | Path) -> dict:
    """Read every `<scene_id>.HOT` under `hot_dir`. Returns a byte-direct map
    {scene_id -> {file, count, hotspots[]}} keyed by the engine scene/node id
    (the .HOT filename stem). Returns an empty map if the dir is absent."""
    d = Path(hot_dir)
    scenes: dict[str, dict] = {}
    if not d.is_dir():
        return {"_present": False, "hot_dir": str(d), "scenes": {}}
    total = 0
    for p in sorted(d.glob("*.HOT")):
        entries = parse_hot_file(p.read_bytes())
        if entries is None:
            continue
        scenes[p.stem] = {
            "scene_id": p.stem,
            "file": p.name,
            "count": len(entries),
            "hotspots": entries,
            "certainty": "byte-direct",
        }
        total += len(entries)
    return {"_present": True, "hot_dir": str(d), "file_count": len(scenes),
            "hotspot_total": total, "scenes": scenes}
