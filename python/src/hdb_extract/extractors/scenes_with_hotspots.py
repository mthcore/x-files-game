"""scenes_with_hotspots.json — cross-link scene_asset_map x hotspots_inventory.

For each scene that the HDB inline labels point at (``scene_asset_map.json``),
look up the matching ``XV/<scene_id>.HOT`` entry in ``hotspots_inventory.json``
and combine the two views: the (Node, Location, scene_name) context AND the
clickable rects. The result is a single artifact a downstream tool can use to
ask, byte-direct:

  * which scene_ids are interactive vs cinematic-only;
  * for an interactive scene, which rects + action_ids fire on click;
  * which .HOT files belong to a scene the inline labels never name (orphan
    rects — surfaced honestly, not silently dropped).

Output shape::

    {
      "_about": "...",
      "_source": [scene_asset_map.json, hotspots_inventory.json],
      "stats": {
        scenes_total, interactive, cinematic_only, orphan_hot_files,
        by_kind: {scene, nav, game},
        by_location: {Field Office: ..., ...},
      },
      "interactive": [           // entries with both label + rects
        { scene_id, kind, node, location, scene_name, asset_dir,
          rect_count, rects: [...], certainty: "byte-direct" }
      ],
      "cinematic_only": [        // label present, no .HOT (e.g. dialog video)
        { scene_id, kind, node, location, scene_name, asset_dir,
          certainty: "byte-direct" }
      ],
      "orphan_hot_files": [      // .HOT present, no inline label
        { scene_id, rect_count, certainty: "byte-direct" }
      ]
    }
"""
from __future__ import annotations

import json
from collections import Counter
from pathlib import Path


def _load_json(path: str | Path) -> dict:
    return json.loads(Path(path).read_text(encoding="utf-8"))


def build_scenes_with_hotspots(scene_asset_map_path: str | Path,
                                hotspots_inventory_path: str | Path) -> dict:
    sm = _load_json(scene_asset_map_path)
    inv = _load_json(hotspots_inventory_path)

    hot_by_id: dict[int, dict] = {sc["scene_id"]: sc for sc in inv["scenes"]}

    # Group inline-label entries by asset_id (one scene_id can carry several
    # entries — different scene_name labels for the same XV file).
    sm_by_id: dict[int, list[dict]] = {}
    for e in sm["entries"]:
        sm_by_id.setdefault(e["asset_id"], []).append(e)

    interactive: list[dict] = []
    cinematic_only: list[dict] = []
    by_kind: Counter = Counter()
    by_loc: Counter = Counter()

    for sid, sm_entries in sorted(sm_by_id.items()):
        # Use the first entry as the primary label set; carry the rest in
        # an `aka` list if there are multiple.
        primary = sm_entries[0]
        aka = []
        for extra in sm_entries[1:]:
            aka.append({
                "kind": extra["kind"],
                "node": extra["node"],
                "location": extra["location"],
                "scene_name": extra["scene_name"],
                "asset_dir": extra["asset_dir"],
                "label_offset": extra["label_offset"],
            })
        by_kind[primary["kind"]] += 1
        by_loc[primary["location"]] += 1
        if sid in hot_by_id:
            sc = hot_by_id[sid]
            entry = {
                "scene_id": sid,
                "kind": primary["kind"],
                "node": primary["node"],
                "location": primary["location"],
                "scene_name": primary["scene_name"],
                "asset_dir": primary["asset_dir"],
                "rect_count": sc["rect_count"],
                "rects": sc["rects"],
                "certainty": "byte-direct",
                "source": "scene_asset_map + hotspots_inventory",
            }
            if aka:
                entry["aka"] = aka
            interactive.append(entry)
        else:
            entry = {
                "scene_id": sid,
                "kind": primary["kind"],
                "node": primary["node"],
                "location": primary["location"],
                "scene_name": primary["scene_name"],
                "asset_dir": primary["asset_dir"],
                "certainty": "byte-direct",
                "source": "scene_asset_map (no .HOT)",
            }
            if aka:
                entry["aka"] = aka
            cinematic_only.append(entry)

    orphan_ids = sorted(set(hot_by_id.keys()) - set(sm_by_id.keys()))
    orphan_hot_files = [
        {
            "scene_id": sid,
            "rect_count": hot_by_id[sid]["rect_count"],
            "certainty": "byte-direct",
            "source": "hotspots_inventory (no inline label)",
        }
        for sid in orphan_ids
    ]

    return {
        "_about": "Cross-link of scene_asset_map and hotspots_inventory. "
                  "Every scene is classified interactive (has .HOT) vs "
                  "cinematic-only (label but no .HOT). Orphan .HOT files "
                  "are surfaced honestly rather than silently dropped.",
        "_source": [str(scene_asset_map_path), str(hotspots_inventory_path)],
        "stats": {
            "scenes_total": len(interactive) + len(cinematic_only),
            "interactive": len(interactive),
            "cinematic_only": len(cinematic_only),
            "orphan_hot_files": len(orphan_hot_files),
            "by_kind": dict(by_kind),
            "by_location": dict(by_loc.most_common(15)),
        },
        "interactive": interactive,
        "cinematic_only": cinematic_only,
        "orphan_hot_files": orphan_hot_files,
    }


def write_scenes_with_hotspots(scene_asset_map_path: str | Path,
                                hotspots_inventory_path: str | Path,
                                out_path: str | Path) -> dict:
    sw = build_scenes_with_hotspots(
        scene_asset_map_path, hotspots_inventory_path)
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as f:
        json.dump(sw, f, indent=1, ensure_ascii=False)
    return sw["stats"]
