"""Load the full scene→asset map (2,432 productions) from the format-analysis ground truth.

This is what makes per-scene readable scripts actually informative :
each scene has 50-330 video productions whose filenames encode the
gameplay (viewpoint, day/night, with/without object, etc.).
"""
from __future__ import annotations

import csv
import os
from collections import defaultdict
from pathlib import Path

DEFAULT_DECOMP_INTEL = Path(os.environ.get(
    "DECOMP_INTEL_DIR",
    "datasets/flow",
))


def load_scene_asset_map(base: Path | None = None) -> dict[str, list[dict]]:
    """Returns {scene_path: [asset_dict, ...]} grouped by scene."""
    base = base or DEFAULT_DECOMP_INTEL
    p = base / "scene_asset_map.csv"
    if not p.exists():
        return {}
    by_scene: dict[str, list[dict]] = defaultdict(list)
    with p.open(newline="", encoding="utf-8") as fp:
        for row in csv.DictReader(fp):
            scene = row["scene_path"]
            try:
                size = int(row["size_bytes"]) if row["size_bytes"] else None
            except ValueError:
                size = None
            try:
                duration = float(row["duration_s"]) if row["duration_s"] else None
            except ValueError:
                duration = None
            by_scene[scene].append({
                "production_name": row["production_name"],
                "storage_dir": row["storage_dir"],
                "file_id": int(row["file_id"]) if row["file_id"] else None,
                "asset_type": row["asset_type"],
                "size_bytes": size,
                "duration_s": duration,
                "video_codec": row.get("video_codec") or "",
                "video_dim": row.get("video_dim") or "",
                "audio_codec": row.get("audio_codec") or "",
                "first_line": row.get("first_line") or "",
            })
    return dict(by_scene)
