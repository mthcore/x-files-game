"""Merge localization strings into the decision tree.

The HDB stores dialog references as integer ``text_id`` values; the
human-readable text lives in the four ``XFiles*.dll`` files. This
extractor joins them.

CLI: ``python -m hdb_extract merge-locale --tree decision_tree.json \
    --dll XFilest.dll [--dll XFilesc.dll ...] --out merged.json``
"""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from hdb_extract.extractors.loc_extract import LocaleDLL


def _walk_and_resolve(node: Any, lookup: dict[int, str], stats: dict[str, int]) -> Any:
    """Recursively walk a JSON-like structure and inject ``*_text`` siblings.

    Any key matching one of ``text_id``, ``subject_id``, ``label_id``,
    ``name_id`` triggers a lookup; the resolved text is added as a new
    key with ``_id`` replaced by ``_text``.
    """
    if isinstance(node, dict):
        for key in list(node.keys()):
            if key.endswith("_id") and isinstance(node[key], int):
                tid = node[key]
                if tid in lookup:
                    node[key.removesuffix("_id") + "_text"] = lookup[tid]
                    stats["resolved"] += 1
                else:
                    stats["unresolved"] += 1
        for k, v in node.items():
            _walk_and_resolve(v, lookup, stats)
    elif isinstance(node, list):
        for v in node:
            _walk_and_resolve(v, lookup, stats)
    return node


def merge(tree_path: str | Path,
          dll_paths: list[str | Path]) -> dict[str, Any]:
    """Load tree + DLLs, return a new merged JSON dict.

    The original ``tree`` is mutated in-place for efficiency; the
    returned dict has the same structure plus ``*_text`` siblings.
    """
    with open(tree_path, encoding="utf-8") as f:
        tree = json.load(f)

    # Build a flat int->str lookup across all DLLs. Later DLLs win on
    # ID collisions (we don't expect any since the four DLLs use
    # disjoint ID ranges, but be explicit).
    lookup: dict[int, str] = {}
    for dll in dll_paths:
        loc = LocaleDLL(dll)
        for k, v in loc.strings.items():
            lookup[k] = v

    stats = {"resolved": 0, "unresolved": 0, "strings_loaded": len(lookup)}
    _walk_and_resolve(tree, lookup, stats)
    tree["_merge_stats"] = stats
    return tree


def merge_and_write(tree_path: str | Path,
                    dll_paths: list[str | Path],
                    out_path: str | Path) -> dict[str, Any]:
    merged = merge(tree_path, dll_paths)
    out_p = Path(out_path)
    out_p.parent.mkdir(parents=True, exist_ok=True)
    out_p.write_text(json.dumps(merged, indent=2, ensure_ascii=False),
                     encoding="utf-8")
    return merged["_merge_stats"]
