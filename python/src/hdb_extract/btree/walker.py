"""High-level walker — enumerate all records in the HDB.

The HDB stores its records across **data** pages (first byte != known
structural tags 0x00/C0/C2/C3/D2). The leaves 0xC3 act as an index : they
hold CNeoIDList ID→offset entries that point into the data pages where
the actual VC* records live.

This walker exposes two modes :
  - `iter_leaf_pages`  : only the 202 leaf pages (CNeoIDList records)
  - `iter_record_pages`: all non-structural data pages (NPfl scan)

Combined, both modes feed `walk_records` which enumerates everything we
can currently identify by marker scanning. Full B-tree walking (root →
internal → leaf → data) awaits G3 completion.
"""
from __future__ import annotations

from typing import Iterator

from hdb_extract.btree.leaf import LeafRecord, iter_records_in_leaf
from hdb_extract.container.container import HdbContainer
from hdb_extract.container.pages import STRUCTURAL_TAGS


def iter_leaf_pages(container: HdbContainer) -> Iterator[tuple[int, bytes, int]]:
    """Yield (page_id, page_bytes, abs_offset) for each leaf page (0xC3)."""
    for page_id, page in container.iter_pages():
        if page and page[0] == 0xC3:
            yield page_id, page, container.page_offset(page_id)


def iter_record_pages(container: HdbContainer) -> Iterator[tuple[int, bytes, int]]:
    """Yield (page_id, page_bytes, abs_offset) for non-structural pages."""
    for page_id, page in container.iter_pages():
        if not page:
            continue
        if page[0] not in STRUCTURAL_TAGS:
            yield page_id, page, container.page_offset(page_id)


def walk_records(container: HdbContainer) -> Iterator[LeafRecord]:
    """Yield every record we can identify in any page."""
    # Pass A : leaf pages — CNeoIDList records.
    for page_id, page, abs_off in iter_leaf_pages(container):
        for rec in iter_records_in_leaf(page, page_id, abs_off):
            yield rec
    # Pass B : data pages — scan for NPfl-marked in-line VC* records.
    for page_id, page, abs_off in iter_record_pages(container):
        for rec in _iter_records_in_data_page(page, page_id, abs_off):
            yield rec


def _iter_records_in_data_page(
    page: bytes,
    page_id: int,
    page_abs_off: int,
) -> list[LeafRecord]:
    """Scan a data page for NPfl-prefixed VC* records (best-effort)."""
    out: list[LeafRecord] = []
    if not page:
        return out
    idx = 0
    positions: list[int] = []
    while True:
        idx = page.find(b"NPfl", idx)
        if idx < 0:
            break
        positions.append(idx)
        idx += 4
    for i, pos in enumerate(positions):
        end = positions[i + 1] if i + 1 < len(positions) else len(page)
        out.append(LeafRecord(
            page_id=page_id,
            record_off=pos,
            abs_off=page_abs_off + pos,
            class_id=None,
            raw=bytes(page[pos:end]),
        ))
    return out
