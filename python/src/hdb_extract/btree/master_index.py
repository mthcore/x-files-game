"""Master index — byte-direct NeoID -> file_offset resolution.

THE KEY DISCOVERY (2026-05-20): the 14,286 tag-0x00 pages are NOT
zero-padding. They split into:

  * Index tables (~1,747 pages): rows of `[file_offset BE u32][id BE u32]`
    starting at offset 0x30, 8 bytes each, terminated by a (0,0) pair.
    Each file_offset points to a record marker elsewhere in the HDB.
    THIS is the flattened B-tree master index (NeoID -> file_offset).

  * Pixel data (~12,539 pages): `[00 R G B]` XRGB pixel triplets
    (decompressed image data). Not relevant to ID resolution.

Established from the on-disk page structure.

This module builds the master index from the validated index-table
pages ONLY, keeping only entries whose file_offset points to a real
record marker — zero inference. Entries that don't validate are
dropped (and counted) rather than guessed.
"""
from __future__ import annotations

import struct
from dataclasses import dataclass, field

from hdb_extract.container.container import HdbContainer

PAGE_SIZE = 256
ENTRY_START = 0x30          # index entries begin here (header occupies 0x00..0x2f)
ENTRY_SIZE = 8
MARKER_MAGIC = b"\x00\x00\x00\x01"

# A page qualifies as an index table only if it has >= MIN_ENTRIES rows
# and >= VALID_RATIO of them point to a real marker. The strict ratio
# eliminates pixel-data pages that coincidentally parse as (offset, id).
MIN_ENTRIES = 3
VALID_RATIO = 0.80


def _marker_at(raw: bytes, off: int) -> bool:
    """True if a record marker [0xCX][sub][00 00 00 01] sits at `off`."""
    if off < 0 or off + 6 > len(raw):
        return False
    return 0xC0 <= raw[off] <= 0xCF and raw[off + 2:off + 6] == MARKER_MAGIC


@dataclass
class IndexStats:
    index_table_pages: int = 0
    pixel_or_other_pages: int = 0
    total_entries: int = 0
    validated_entries: int = 0
    dropped_unvalidated: int = 0
    conflicting_ids: int = 0
    unique_ids: int = 0


@dataclass
class MasterIndex:
    """Byte-direct NeoID -> file_offset map built from index-table pages."""

    id_to_offset: dict[int, int] = field(default_factory=dict)
    stats: IndexStats = field(default_factory=IndexStats)

    @classmethod
    def build(cls, container: HdbContainer) -> "MasterIndex":
        raw = container.raw
        mi = cls()
        st = mi.stats
        for pi in range(container.page_count):
            page = container.page(pi)
            if page[0] != 0x00:
                continue
            # Parse candidate entries (relative to page), validate against
            # absolute file offsets.
            entries: list[tuple[int, int]] = []
            for off in range(ENTRY_START, PAGE_SIZE - ENTRY_SIZE + 1, ENTRY_SIZE):
                fo = struct.unpack_from(">I", page, off)[0]
                rid = struct.unpack_from(">I", page, off + 4)[0]
                if fo == 0 and rid == 0:
                    break  # table terminator
                entries.append((fo, rid))

            if len(entries) < MIN_ENTRIES:
                st.pixel_or_other_pages += 1
                continue

            valid = sum(1 for fo, _ in entries if _marker_at(raw, fo))
            if valid < len(entries) * VALID_RATIO:
                st.pixel_or_other_pages += 1
                continue

            # Confirmed index table.
            st.index_table_pages += 1
            for fo, rid in entries:
                st.total_entries += 1
                if not _marker_at(raw, fo):
                    st.dropped_unvalidated += 1
                    continue
                st.validated_entries += 1
                if rid in mi.id_to_offset and mi.id_to_offset[rid] != fo:
                    st.conflicting_ids += 1
                    # First-wins (matches B-tree first-leaf semantics).
                    continue
                mi.id_to_offset[rid] = fo
        st.unique_ids = len(mi.id_to_offset)
        return mi

    def resolve(self, neo_id: int) -> int | None:
        """Return the file_offset for `neo_id`, or None (never guesses)."""
        return self.id_to_offset.get(neo_id)

    def record_marker(self, container: HdbContainer, neo_id: int) -> dict | None:
        """Resolve `neo_id` and return its decoded 8-byte marker, or None."""
        fo = self.resolve(neo_id)
        if fo is None:
            return None
        raw = container.raw
        if fo + 8 > len(raw):
            return None
        return {
            "file_offset": fo,
            "tag": raw[fo],
            "sub_tag": raw[fo + 1],
            "version": struct.unpack_from(">I", raw, fo + 2)[0],
            "flag16": struct.unpack_from(">H", raw, fo + 6)[0],
        }
