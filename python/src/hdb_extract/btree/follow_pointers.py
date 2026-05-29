"""Walker that follows CNeoIDList `offset_or_size` pointers.

Strategy : the 13,982 CNeoIDList records in HDB each have an
`offset_or_size` field that points to where the actual data record sits.
We follow these pointers, read the record bytes at each target, and
attempt to decode them using PackedHDBSerializer.

The target byte at each offset usually starts with the page tag of the
containing page (0xC0/0xC2/0xC3/0xD2 if it points to a structural page,
or arbitrary first-byte for data pages).

For records that we can decode (the 9 oracle classes from W23_4_*),
return a parsed dict. For records we cannot yet decode (the 42 other
classes), return the raw bytes and the class_id (if identifiable).
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Iterator

from hdb_extract.container.container import HdbContainer
from hdb_extract.format.neoidlist import parse_neoidlist_record


@dataclass
class FollowedRecord:
    """A record reached by following a CNeoIDList pointer."""
    source_neoidlist_off: int   # where the CNeoIDList record sits
    target_off: int             # where the pointed record sits
    target_first_byte: int      # first byte at the target offset
    target_page_id: int         # page containing the target offset
    target_page_tag: int        # tag of that page
    raw_bytes: bytes            # up to 256 bytes from target_off


def iter_followed_records(
    container: HdbContainer,
    *,
    max_bytes: int = 256,
) -> Iterator[FollowedRecord]:
    """Yield a FollowedRecord for each CNeoIDList pointer in the HDB."""
    raw = container.raw
    total = len(raw)

    for page_id, page in container.iter_pages():
        if not page:
            continue
        page_abs_off = container.page_offset(page_id)
        # Find each 'ID  ' marker in this page (CNeoIDList records).
        idx = 0
        while True:
            idx = page.find(b"ID  ", idx)
            if idx < 0:
                break
            if idx >= 16 and idx + 14 <= len(page):
                rec_off_in_page = idx - 16
                rec_abs_off = page_abs_off + rec_off_in_page
                try:
                    rec = parse_neoidlist_record(page, rec_off_in_page)
                except Exception:
                    idx += 4
                    continue
                if rec.is_valid:
                    target = rec.offset_or_size
                    if 0 < target < total:
                        tgt_page_id = (target - 32) // 256 if target >= 32 else -1
                        tgt_first = raw[target]
                        tgt_page_tag = (raw[32 + tgt_page_id * 256]
                                         if 0 <= tgt_page_id < container.page_count
                                         else 0)
                        body_end = min(target + max_bytes, total)
                        yield FollowedRecord(
                            source_neoidlist_off=rec_abs_off,
                            target_off=target,
                            target_first_byte=tgt_first,
                            target_page_id=tgt_page_id,
                            target_page_tag=tgt_page_tag,
                            raw_bytes=raw[target:body_end],
                        )
            idx += 4


def categorize_followed(records: list[FollowedRecord]) -> dict:
    """Group followed records by target page tag for analysis."""
    by_tag: dict[int, list[FollowedRecord]] = {}
    for r in records:
        by_tag.setdefault(r.target_page_tag, []).append(r)
    return {
        "total_followed": len(records),
        "by_target_page_tag": {
            f"0x{tag:02x}": len(group)
            for tag, group in sorted(by_tag.items(), key=lambda x: -len(x[1]))
        },
        "samples": {
            f"0x{tag:02x}": [
                {
                    "neoidlist_off": r.source_neoidlist_off,
                    "target_off": r.target_off,
                    "target_first_byte": f"0x{r.target_first_byte:02x}",
                    "raw_head_hex": r.raw_bytes[:32].hex(),
                }
                for r in group[:2]
            ]
            for tag, group in sorted(by_tag.items(), key=lambda x: -len(x[1]))
        },
    }
