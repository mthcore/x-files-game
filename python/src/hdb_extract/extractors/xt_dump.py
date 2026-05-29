"""Extract all 474 XT (text script) files from the game's XT/ directory.

Each XT file is a plain UTF-8 text script — an email, a journal entry,
a help screen, a media article, an interrogation note, a character bio,
a Bible excerpt, or credits.

We classify each file by content + cross-reference with the asset
inventory, and produce :

  - `out/json/xt_scripts.json` — catalogue with classification + content preview
  - `out/xt/by_category/<category>/<id>.txt` — organized copies for browsing
"""
from __future__ import annotations

import csv
import os
import re
import shutil
from pathlib import Path
from typing import Any

DEFAULT_XT_DIR = Path(os.environ.get("XT_DIR", "XT"))
DEFAULT_INVENTORY = Path(
    os.environ.get("XT_INVENTORY", "asset_inventory.csv")
)


# Email naming pattern : R<sender><topic>[N<day>].txt or .xtx
_EMAIL_RE = re.compile(r"^R([A-Z][a-z]+)([A-Za-z0-9]+?)(?:N(\d))?$")


def _classify_content(text: str, file_id: int) -> dict[str, Any]:
    """Guess the category from content + ID range heuristics."""
    text_lower = text.lower()

    # ID range hints from narrative_content.md
    if 49526 <= file_id <= 50368:
        return {"category": "journal", "subcategory": "Willmore diary"}
    if 51070 <= file_id <= 51088:
        return {"category": "help", "subcategory": "UI help screen"}
    if 55583 <= file_id <= 55615:
        return {"category": "media", "subcategory": "News article"}
    if 61137 <= file_id <= 64560:
        return {"category": "email", "subcategory": "Player mailbox"}
    if 63271 <= file_id <= 63385:
        return {"category": "other", "subcategory": "Crime scene interrog"}
    if 80887 <= file_id <= 80912:
        return {"category": "other", "subcategory": "Bible excerpt"}
    if 81279 <= file_id <= 81321:
        return {"category": "other", "subcategory": "Character profile"}
    if 86072 <= file_id <= 86112:
        return {"category": "other", "subcategory": "Suspect bio"}
    if 91101 <= file_id <= 91111:
        return {"category": "other", "subcategory": "Credits"}

    # Content-based fallback heuristics.
    if "march" in text_lower[:60] or "april" in text_lower[:60]:
        if "1996" in text_lower[:120]:
            return {"category": "journal", "subcategory": "diary"}
    if "post," in text_lower[:60] or "news" in text_lower[:60]:
        return {"category": "media", "subcategory": "article"}
    if text.startswith("R") and "_" not in text[:20]:
        return {"category": "email", "subcategory": "fallback"}
    return {"category": "other", "subcategory": "unclassified"}


def _read_xt(path: Path) -> str:
    """Read an XT file. They're UTF-8 with optional BOM, ~150-1700 chars."""
    try:
        with path.open("rb") as fp:
            data = fp.read()
        if data.startswith(b"\xff\xfe"):
            return data[2:].decode("utf-16-le", errors="replace")
        if data.startswith(b"\xfe\xff"):
            return data[2:].decode("utf-16-be", errors="replace")
        if data.startswith(b"\xef\xbb\xbf"):
            return data[3:].decode("utf-8", errors="replace")
        return data.decode("utf-8", errors="replace")
    except Exception as e:
        return f"<read error: {e}>"


def _scripts_inventory(path: Path | None = None) -> dict[int, dict]:
    """Optional cross-reference from the asset inventory"""
    path = path or DEFAULT_INVENTORY
    if not path.exists():
        return {}
    out: dict[int, dict] = {}
    with path.open(newline="", encoding="utf-8") as fp:
        for row in csv.DictReader(fp):
            try:
                fid = int(row.get("file_id") or row.get("id") or 0)
            except (ValueError, TypeError):
                continue
            if fid:
                out[fid] = row
    return out


def dump_xt_scripts(
    xt_dir: Path | None = None,
    preview_chars: int = 200,
    inventory_path: Path | None = None,
) -> dict[str, Any]:
    """Read all XT files, classify them, return a JSON-serializable dict."""
    xt_dir = xt_dir or DEFAULT_XT_DIR
    if not xt_dir.exists():
        return {
            "total": 0, "error": f"XT dir not found : {xt_dir}",
            "scripts": [],
        }

    inventory = _scripts_inventory(inventory_path)
    scripts: list[dict[str, Any]] = []
    by_category: dict[str, int] = {}
    files = sorted(xt_dir.glob("*.xtx"))

    for f in files:
        try:
            fid = int(f.stem)
        except ValueError:
            continue
        text = _read_xt(f)
        cls = _classify_content(text, fid)
        inv = inventory.get(fid, {})
        scripts.append({
            "file_id": fid,
            "filename": f.name,
            "size_bytes": f.stat().st_size,
            "category": cls["category"],
            "subcategory": cls["subcategory"],
            "preview": text[:preview_chars].strip(),
            "inventory": inv or None,
        })
        by_category[cls["category"]] = by_category.get(cls["category"], 0) + 1

    return {
        "_source": str(xt_dir),
        "total": len(scripts),
        "by_category": dict(sorted(by_category.items())),
        "scripts": scripts,
    }


def organize_xt_by_category(
    xt_dir: Path | None = None,
    out_dir: Path = Path("out/xt/by_category"),
    dump: dict[str, Any] | None = None,
) -> dict[str, int]:
    """Copy each XT into out/xt/by_category/<category>/<id>.txt."""
    xt_dir = xt_dir or DEFAULT_XT_DIR
    out_dir = Path(out_dir)
    if dump is None:
        dump = dump_xt_scripts(xt_dir, preview_chars=0)
    counts: dict[str, int] = {}
    for entry in dump.get("scripts", []):
        cat = entry["category"]
        subcat_dir = out_dir / cat
        subcat_dir.mkdir(parents=True, exist_ok=True)
        src = xt_dir / entry["filename"]
        dst = subcat_dir / f"{entry['file_id']}.txt"
        if src.exists():
            try:
                shutil.copyfile(src, dst)
                counts[cat] = counts.get(cat, 0) + 1
            except OSError:
                pass
    return counts
