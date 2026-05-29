"""Per-page index : for each page, classify and characterize it.

This gives a complete map of the 23,726 pages with :
  - page_id, abs_off, tag, kind
  - n_strings (≥4 printable ASCII)
  - has_id_marker (contains 'ID  ' FOURCC)
  - first_string (first ≥4-char ASCII string if any)
  - npfl_count, nptc_count (always 0 in real HDB, but recorded for invariant)

The output is queryable in SQLite for "find all pages with text matching X".
"""
from __future__ import annotations

import re

from hdb_extract.container.container import HdbContainer
from hdb_extract.container.pages import PAGE_TAG_NAMES

_ASCII_RUN = re.compile(rb"[\x20-\x7e]{4,}")


def dump_page_index(container: HdbContainer) -> dict:
    rows = []
    for page_id, page in container.iter_pages():
        if not page:
            continue
        tag = page[0]
        strs = list(_ASCII_RUN.finditer(page))
        rows.append({
            "page_id": page_id,
            "abs_off": container.page_offset(page_id),
            "tag": tag,
            "tag_hex": f"0x{tag:02x}",
            "name": PAGE_TAG_NAMES.get(tag, "data"),
            "n_strings": len(strs),
            "first_string": (strs[0].group(0).decode("ascii")
                             if strs else None),
            "has_id_marker": b"ID  " in page,
            "kind": page[1] if len(page) > 1 else None,
        })
    return {"total_pages": len(rows), "pages": rows}
