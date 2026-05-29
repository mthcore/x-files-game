"""SQLite writer — produces a queryable cross-table from extracted JSON."""
from __future__ import annotations

import sqlite3
from pathlib import Path
from typing import Any


def write_sqlite(
    db_path: Path,
    *,
    classes: dict[str, Any],
    neoidlist: dict[str, Any],
    production_paths: dict[str, Any],
    records: list[dict[str, Any]],
    summary: dict[str, Any],
) -> None:
    """Write everything we have to an SQLite DB.

    Tables :
      summary           single-row file-level info
      classes           one row per documented VC* class
      neoidlist         13,982 CNeoIDList entries
      records           records identified by surface markers
      production_paths  2,455 scene → asset_id mappings
    """
    db_path = Path(db_path)
    db_path.parent.mkdir(parents=True, exist_ok=True)
    if db_path.exists():
        db_path.unlink()
    conn = sqlite3.connect(db_path)
    try:
        cur = conn.cursor()

        # summary
        cur.execute("""
            CREATE TABLE summary (
                key TEXT PRIMARY KEY, value TEXT
            )
        """)
        flat = _flatten_dict(summary)
        cur.executemany("INSERT INTO summary VALUES (?, ?)", flat.items())

        # classes
        cur.execute("""
            CREATE TABLE classes (
                class_id INTEGER PRIMARY KEY,
                class_id_hex TEXT,
                name TEXT,
                size_bytes INTEGER,
                parent TEXT,
                n_ops INTEGER,
                instances_seen INTEGER,
                notes TEXT
            )
        """)
        for c in classes["classes"]:
            cur.execute("""
                INSERT INTO classes (class_id, class_id_hex, name, size_bytes,
                                     parent, n_ops, instances_seen, notes)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            """, (c["class_id_int"], c["class_id"], c["name"], c["size_bytes"],
                  c["parent"], c["n_ops"], c["instances_seen"], c["notes"]))

        # neoidlist
        cur.execute("""
            CREATE TABLE neoidlist (
                rowid INTEGER PRIMARY KEY AUTOINCREMENT,
                page_id INTEGER, page_off INTEGER, abs_off INTEGER,
                flag_const INTEGER, count INTEGER,
                offset_or_size INTEGER, class_id INTEGER, value INTEGER
            )
        """)
        cur.executemany("""
            INSERT INTO neoidlist (page_id, page_off, abs_off, flag_const,
                                   count, offset_or_size, class_id, value)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        """, [(r["page_id"], r["page_off"], r["abs_off"], r["flag_const"],
               r["count"], r["offset_or_size"], r["class_id"], r["value"])
              for r in neoidlist["ids"]])

        # records (surface)
        cur.execute("""
            CREATE TABLE records (
                rowid INTEGER PRIMARY KEY AUTOINCREMENT,
                page_id INTEGER, abs_off INTEGER, len INTEGER,
                class_id INTEGER, class_name TEXT
            )
        """)
        cur.executemany("""
            INSERT INTO records (page_id, abs_off, len, class_id, class_name)
            VALUES (?, ?, ?, ?, ?)
        """, [(r["page_id"], r["abs_off"], r["len"],
               r.get("class_id"), r.get("class_name")) for r in records])

        # production_paths
        cur.execute("""
            CREATE TABLE production_paths (
                rowid INTEGER PRIMARY KEY AUTOINCREMENT,
                abs_off INTEGER, scene TEXT, production_name TEXT,
                flags TEXT, storage_dir TEXT, file_id INTEGER,
                asset_ext TEXT
            )
        """)
        cur.executemany("""
            INSERT INTO production_paths (abs_off, scene, production_name,
                                          flags, storage_dir, file_id, asset_ext)
            VALUES (?, ?, ?, ?, ?, ?, ?)
        """, [(p["abs_off"], p["scene"], p["production_name"],
               ",".join(p["flags"]) if isinstance(p["flags"], list) else p["flags"],
               p["storage_dir"], p["file_id"], p["asset_ext"])
              for p in production_paths["paths"]])

        # Useful indexes for cross-table queries.
        cur.execute("CREATE INDEX idx_neoidlist_class ON neoidlist(class_id)")
        cur.execute("CREATE INDEX idx_records_class ON records(class_id)")
        cur.execute("CREATE INDEX idx_paths_scene ON production_paths(scene)")
        cur.execute("CREATE INDEX idx_paths_storage ON production_paths(storage_dir)")
        cur.execute("CREATE INDEX idx_paths_file ON production_paths(file_id)")

        conn.commit()
    finally:
        conn.close()


def _flatten_dict(d: dict, prefix: str = "") -> dict[str, str]:
    """Flatten nested dict into key.path → str value pairs."""
    out: dict[str, str] = {}
    for k, v in d.items():
        key = f"{prefix}{k}" if not prefix else f"{prefix}.{k}"
        if isinstance(v, dict):
            out.update(_flatten_dict(v, key))
        else:
            out[key] = str(v)
    return out
