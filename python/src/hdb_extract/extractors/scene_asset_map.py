"""scene_asset_map.json — byte-direct map from inline labels to XV/XN ids.

The HDB carries inline asset labels in a sub-format where each field of the
human-readable label is separated by a single ``0x7f`` byte (the printable
``ASCII DEL``), only the ``.mov`` and ``.xmv`` filename extensions use real
dots. Examples (with ``\\x7f`` rendered as ``|`` for clarity)::

    Video|Node 6|Field Office|sc081E.mov ... 12 XV|22539.xmv
    Navs|LoopsR|Field Office|028 - FONAV1E.mov ... 12 XN|38804.xmv
    Video|Game|VCLogo.mov ... 12 XV|58615.xmv

Between the ``.mov`` label and the asset path the file carries a small bundle
of length-prefix bytes (``\\x01<digit>\\x01<digit>`` for flags / numparts,
then ``\\x02<dec-len>`` for the path-length tag). That bundle is treated as
opaque; we just consume up to 8 non-letter bytes before the directory tag.

Output shape::

    {
      "_about": "...",
      "_source": "XFILES.HDB",
      "stats": { entries_total, by_kind, by_asset_dir, by_node, ... },
      "entries": [
        { kind, node, category?, location, scene_name, asset_dir,
          asset_id, label_offset, certainty: "byte-direct",
          source: "HDB:inline-label" }
      ]
    }

Every emitted field comes straight from the file bytes.
"""
from __future__ import annotations

import json
import re
from collections import Counter
from pathlib import Path


_SEP = b"\x7f"   # field separator inside inline labels


# Capture: (1) node number, (2) location, (3) scene name, (4) asset dir,
# (5) asset id.
_RE_SCENE = re.compile(
    rb"Video\x7fNode\s+(\d+)\x7f([A-Za-z][\x20-\x7e\x7f]{0,40}?)\x7f"
    rb"([\x20-\x7e\x7f]{1,80}?)\.mov[^A-Za-z]{1,12}"
    rb"([A-Z]{2,3})\x7f(\d+)\.xmv",
    re.DOTALL,
)

# Capture: (1) category (Navs sub-kind), (2) location, (3) scene name,
# (4) asset dir, (5) asset id.
_RE_NAV = re.compile(
    rb"Navs\x7f([A-Za-z][\x20-\x7e]{0,30}?)\x7f"
    rb"([A-Za-z][\x20-\x7e]{0,40}?)\x7f"
    rb"([\x20-\x7e\x7f]{1,80}?)\.mov[^A-Za-z]{1,12}"
    rb"([A-Z]{2,3})\x7f(\d+)\.xmv",
    re.DOTALL,
)

# Capture: (1) scene name, (2) asset dir, (3) asset id.
_RE_GAME = re.compile(
    rb"Video\x7fGame\x7f([\x20-\x7e\x7f]{1,60}?)\.mov[^A-Za-z]{1,12}"
    rb"([A-Z]{2,3})\x7f(\d+)\.xmv",
    re.DOTALL,
)


def _label_text(b: bytes) -> str:
    """Decode a label fragment, mapping the 0x7f separator to '/' for
    human readability while keeping all other printable ASCII verbatim."""
    out = []
    for ch in b:
        if ch == 0x7f:
            out.append("/")
        elif 0x20 <= ch < 0x7f:
            out.append(chr(ch))
        else:
            out.append(".")
    return "".join(out).strip()


def _iter_scene(raw: bytes):
    for m in _RE_SCENE.finditer(raw):
        yield {
            "kind": "scene",
            "node": int(m.group(1)),
            "location": _label_text(m.group(2)),
            "scene_name": _label_text(m.group(3)),
            "asset_dir": m.group(4).decode("ascii"),
            "asset_id": int(m.group(5)),
            "label_offset": m.start(),
            "certainty": "byte-direct",
            "source": "HDB:inline-label",
        }


def _iter_nav(raw: bytes):
    for m in _RE_NAV.finditer(raw):
        yield {
            "kind": "nav",
            "node": None,
            "category": _label_text(m.group(1)),
            "location": _label_text(m.group(2)),
            "scene_name": _label_text(m.group(3)),
            "asset_dir": m.group(4).decode("ascii"),
            "asset_id": int(m.group(5)),
            "label_offset": m.start(),
            "certainty": "byte-direct",
            "source": "HDB:inline-label",
        }


def _iter_game(raw: bytes):
    for m in _RE_GAME.finditer(raw):
        yield {
            "kind": "game",
            "node": None,
            "location": "Game",
            "scene_name": _label_text(m.group(1)),
            "asset_dir": m.group(2).decode("ascii"),
            "asset_id": int(m.group(3)),
            "label_offset": m.start(),
            "certainty": "byte-direct",
            "source": "HDB:inline-label",
        }


def build_scene_asset_map(hdb_path: str | Path) -> dict:
    raw = Path(hdb_path).read_bytes()
    entries: list[dict] = []
    entries.extend(_iter_scene(raw))
    entries.extend(_iter_nav(raw))
    entries.extend(_iter_game(raw))

    # Dedup by (kind, asset_id, location, scene_name) — same label can appear
    # multiple times; keep the lowest label_offset.
    dedup: dict[tuple, dict] = {}
    for e in entries:
        key = (e["kind"], e["asset_id"], e["location"],
                e.get("scene_name", ""))
        if key not in dedup or e["label_offset"] < dedup[key]["label_offset"]:
            dedup[key] = e
    entries = sorted(dedup.values(),
                      key=lambda e: (e["asset_id"], e["label_offset"]))

    by_node: Counter = Counter()
    by_loc: Counter = Counter()
    by_dir: Counter = Counter()
    by_kind: Counter = Counter()
    for e in entries:
        by_kind[e["kind"]] += 1
        by_dir[e["asset_dir"]] += 1
        by_loc[e["location"]] += 1
        if e["node"] is not None:
            by_node[str(e["node"])] += 1

    return {
        "_about": "Byte-direct map from HDB inline labels to their XV/XN "
                  "asset ids. Every entry is read straight from the bytes; "
                  "fields the label does not provide stay null.",
        "_source": str(hdb_path),
        "stats": {
            "entries_total": len(entries),
            "by_kind": dict(by_kind),
            "by_asset_dir": dict(by_dir),
            "by_node": dict(sorted(by_node.items(),
                                     key=lambda kv: int(kv[0]))),
            "distinct_locations": len(by_loc),
            "top_locations": dict(by_loc.most_common(10)),
        },
        "entries": entries,
    }


def write_scene_asset_map(hdb_path: str | Path, out_path: str | Path) -> dict:
    sm = build_scene_asset_map(hdb_path)
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as f:
        json.dump(sm, f, indent=1, ensure_ascii=False)
    return sm["stats"]
