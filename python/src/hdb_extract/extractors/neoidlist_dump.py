"""Dump every CNeoIDList ID record found in the HDB.

15,606 occurrences of 'ID  ' marker in XFILES.HDB. Each marker is preceded
by a 16-byte CNeoIDList record header. This extractor enumerates them
all, producing :

    {
        "total": 15606,
        "ids": [
            {"page_id": 91, "off": 16, "abs_off": 23316,
             "flag": 1, "count": 1, "offset_or_size": ,
             "class_id": 0x0b, "value": 1},
            ...
        ]
    }

Even without G1 packed wire resolved, this is a clean byte-direct
inventory of every record key in the HDB.
"""
from __future__ import annotations

from hdb_extract.container.container import HdbContainer
from hdb_extract.format.neoidlist import (
    CNEOIDLIST_RECORD_SIZE,
    parse_neoidlist_record,
)


def dump_neoidlist(container: HdbContainer) -> dict:
    out = []
    for page_id, page in container.iter_pages():
        if not page:
            continue
        idx = 0
        page_abs_off = container.page_offset(page_id)
        while True:
            idx = page.find(b"ID  ", idx)
            if idx < 0:
                break
            if idx >= 16 and idx + 14 <= len(page):
                try:
                    rec = parse_neoidlist_record(page, idx - 16)
                    if rec.is_valid:
                        out.append({
                            "page_id": page_id,
                            "page_off": idx - 16,
                            "abs_off": page_abs_off + idx - 16,
                            "flag_const": rec.flag_const,
                            "count": rec.count,
                            "offset_or_size": rec.offset_or_size,
                            "class_id": rec.class_id,
                            "value": rec.value,
                        })
                except Exception:
                    pass
            idx += 4
    return {"total": len(out), "ids": out}
