"""Leaf page parser (0xC3) — extract records from a single leaf.

Leaf pages start with a 6-byte page header (tag/kind/reserved/flag) then
contain records back-to-back. Each record begins with a `CNeoIDList`-style
20-byte header (flag_const, count, offset_or_size, class_id, ID fourcc) or
an NPfl-prefixed VC* record (base init + class-specific fields).

This parser walks the leaf body and tries to identify each record's
class_id either by scanning for the 'ID  ' / 'NPfl' / 'vers' markers.
The exact intra-page record layout is still inferred (G3 pending) ; this
is a best-effort scanner that the test suite can validate against
ground-truth CSVs.
"""
from __future__ import annotations

import struct
from dataclasses import dataclass

from hdb_extract.container.pages import PAGE_SIZE
from hdb_extract.format.neoidlist import (
    CNEOIDLIST_RECORD_SIZE,
    parse_neoidlist_record,
)


@dataclass(frozen=True)
class LeafRecord:
    """A single record located inside a leaf page."""
    page_id: int
    record_off: int        # offset within the page (relative)
    abs_off: int           # absolute offset in the HDB file
    class_id: int | None   # if identified from header
    raw: bytes             # the record bytes (may be truncated to page boundary)


def iter_records_in_leaf(
    page: bytes,
    page_id: int,
    page_abs_off: int,
) -> list[LeafRecord]:
    """Scan a leaf page (0xC3) for records — best-effort marker search.

    The exact intra-page layout is gated by G3 (B-tree internal layout).
    Until G3 is decoded, we use a two-pass marker scan :

      1. Find every 'ID  ' FOURCC and back up 16 bytes for a CNeoIDList
         record (30 bytes total, class_id=0x0b).
      2. Find every 'NPfl' FOURCC and treat each as the start of a VC*
         in-line record (length = bytes until next NPfl or end-of-page).

    Records emitted by pass 1 are subtracted from pass 2 (we never emit
    overlapping records).
    """
    out: list[LeafRecord] = []
    consumed: set[int] = set()
    if not page or page[0] != 0xC3:
        return out

    # Pass 1 : CNeoIDList records (30B).
    idx = 0
    while True:
        idx = page.find(b"ID  ", idx)
        if idx < 0:
            break
        if idx >= 16 and idx + 14 <= len(page):
            rec_off = idx - 16
            try:
                rec = parse_neoidlist_record(page, rec_off)
                if rec.is_valid:
                    out.append(LeafRecord(
                        page_id=page_id,
                        record_off=rec_off,
                        abs_off=page_abs_off + rec_off,
                        class_id=rec.class_id,
                        raw=bytes(page[rec_off:rec_off + CNEOIDLIST_RECORD_SIZE]),
                    ))
                    for i in range(rec_off, rec_off + CNEOIDLIST_RECORD_SIZE):
                        consumed.add(i)
            except struct.error:
                pass
        idx += 4

    # Pass 2 : NPfl-prefixed VC* records (variable size).
    # Collect all NPfl positions first.
    npfl_positions: list[int] = []
    idx = 0
    while True:
        idx = page.find(b"NPfl", idx)
        if idx < 0:
            break
        if idx not in consumed:
            npfl_positions.append(idx)
        idx += 4

    # Bound each NPfl record by the next NPfl (or page end).
    for i, pos in enumerate(npfl_positions):
        end = npfl_positions[i + 1] if i + 1 < len(npfl_positions) else len(page)
        out.append(LeafRecord(
            page_id=page_id,
            record_off=pos,
            abs_off=page_abs_off + pos,
            class_id=None,  # unknown until polymorphe dispatch
            raw=bytes(page[pos:end]),
        ))

    out.sort(key=lambda r: r.record_off)
    return out
