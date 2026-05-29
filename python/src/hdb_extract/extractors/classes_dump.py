"""Dump the documented ClassSpec set to classes.json."""
from __future__ import annotations

from typing import Any

from hdb_extract.classes.registry import CLASSES_BY_ID


def dump_classes(by_class_counts: dict[int, int] | None = None) -> dict[str, Any]:
    by_class_counts = by_class_counts or {}
    classes = []
    for cid in sorted(CLASSES_BY_ID):
        spec = CLASSES_BY_ID[cid]
        classes.append({
            "class_id": f"0x{cid:02x}",
            "class_id_int": cid,
            "name": spec.name,
            "size_bytes": spec.size_bytes,
            "parent": spec.parent,
            "n_ops": len(spec.ops),
            "ops_summary": _summarize_ops(spec.ops),
            "instances_seen": by_class_counts.get(cid, 0),
            "notes": spec.notes,
        })
    return {
        "total_classes": len(classes),
        "classes": classes,
    }


def _summarize_ops(ops: tuple) -> list[str]:
    """Render a short ASCII summary of each Op."""
    out = []
    for op in ops:
        name = type(op).__name__
        if hasattr(op, "tag") and hasattr(op, "field"):
            out.append(f"{name}(tag=0x{op.tag:02x},{op.field})")
        elif hasattr(op, "fourcc") and hasattr(op, "field"):
            out.append(f"{name}(fourcc=0x{op.fourcc:08x},{op.field})")
        elif hasattr(op, "field"):
            out.append(f"{name}({op.field})")
        elif hasattr(op, "min_version"):
            out.append(f"{name}(v>{op.min_version},{len(op.ops)} ops)")
        else:
            out.append(name)
    return out
