"""HDB page classification.

Each page is 256 bytes. The first byte (tag) identifies its role :

    0x00  empty           — zero-padded reserved page
    0xC0  btree_freed     — page recycled after a delete, content opaque
    0xC2  btree_internal  — interior B-tree node (G3, layout pending runtime)
    0xC3  btree_leaf      — leaf B-tree node, contains records
    0xD2  btree_internal_alt  — alternate interior node (also G3)
    other data            — raw blob page (string content, sub-records)

The B-tree pages share a 6-byte header :

    +0  tag                                       (0xC0/C2/C3/D2)
    +1  kind                                      (0x30 / 0x31 / 0x32 typically)
    +2  reserved (3 bytes, observed always 0x00)
    +5  flag    (observed always 0x01)
"""
from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum

PAGE_SIZE = 256


class PageType(IntEnum):
    EMPTY = 0x00
    BTREE_FREED = 0xC0
    BTREE_INTERNAL = 0xC2
    BTREE_LEAF = 0xC3
    BTREE_INTERNAL_ALT = 0xD2
    DATA = -1  # synthetic — first byte does not match any structural tag


PAGE_TAG_NAMES = {
    0x00: "empty",
    0xC0: "btree_freed",
    0xC2: "btree_internal",
    0xC3: "btree_leaf",
    0xD2: "btree_internal_alt",
}

STRUCTURAL_TAGS = {0x00, 0xC0, 0xC2, 0xC3, 0xD2}


def classify_page(page: bytes) -> PageType:
    """Return the PageType for `page` (256 bytes)."""
    if not page:
        raise ValueError("empty page bytes")
    tag = page[0]
    if tag in (0x00, 0xC0, 0xC2, 0xC3, 0xD2):
        return PageType(tag)
    return PageType.DATA


@dataclass(frozen=True)
class PageHeader:
    """B-tree page header (6 bytes)."""
    tag: int
    kind: int
    reserved: tuple[int, int, int]
    flag: int

    @classmethod
    def parse(cls, page: bytes) -> "PageHeader":
        if len(page) < 6:
            raise ValueError("page too short for header")
        return cls(
            tag=page[0],
            kind=page[1],
            reserved=(page[2], page[3], page[4]),
            flag=page[5],
        )
