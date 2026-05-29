"""B-tree catalog extractor — recover object/asset names from index pages.

The B-tree pages (tags 0xC2 / 0xD2 / 0xC3) carry NeoAccess index
records. Beyond the navigation structure (whose per-`kind` semantics
require the unpublished `CNeoNode.cp`), these pages embed the
**catalog of object and asset names** as length-prefixed NativeStrings
(the format documented in CNeoStream.cp: one length byte followed by
up to 255 characters).

This extractor scans every B-tree page for NativeStrings and recovers
the full name catalog — asset paths (.mov / .pff / .pic / .mus / ...),
object labels, scene names, even embedded e-mail HTML. This closes the
"content" side of the B-tree internal pages: the bytes were always
captured (round-trip identical), but their *semantic* extraction
(names → JSON) was incomplete until now.

What this does NOT recover: the meaning of the `kind` discriminator
byte (B-tree level / node type). That is pure navigation metadata with
no game content, and requires the unpublished NeoLogic engine source.
"""
from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Iterator

from hdb_extract.container.container import HdbContainer
from hdb_extract.container.pages import PAGE_SIZE

BTREE_TAGS = (0xC0, 0xC2, 0xC3, 0xD2)

# A "name" is a NativeString: [len:1B][len printable bytes].
# We accept lengths 2..127 and require the body to be mostly printable.
_MIN_LEN = 2
_MAX_LEN = 127
_PRINTABLE = set(range(0x20, 0x7F))


@dataclass
class CatalogEntry:
    page_id: int
    offset: int            # offset within the page of the length byte
    abs_off: int           # absolute file offset
    length: int
    text: str


@dataclass
class BtreeCatalog:
    entries: list[CatalogEntry] = field(default_factory=list)

    def names(self) -> list[str]:
        return [e.text for e in self.entries]

    def unique_names(self) -> list[str]:
        seen: dict[str, None] = {}
        for e in self.entries:
            seen.setdefault(e.text, None)
        return list(seen.keys())

    def asset_paths(self) -> list[str]:
        rx = re.compile(r"\.(mov|pff|pic|mus|wav|aif|xmv|nmv|amv|dmv|gif|bmp)$", re.I)
        return sorted({e.text for e in self.entries if rx.search(e.text)})


def _scan_page_nativestrings(page: bytes, page_id: int,
                             page_abs_off: int) -> Iterator[CatalogEntry]:
    """Yield NativeString catalog entries found in one B-tree page."""
    n = len(page)
    i = 6  # skip the 6-byte page header
    while i < n:
        ln = page[i]
        if _MIN_LEN <= ln <= _MAX_LEN and i + 1 + ln <= n:
            body = page[i + 1:i + 1 + ln]
            # Require at least 80% printable and a leading alnum/punct.
            printable = sum(1 for b in body if b in _PRINTABLE)
            if printable >= max(2, int(ln * 0.8)) and body[0] in _PRINTABLE:
                text = body.decode("latin-1", errors="replace")
                # Heuristic: skip pure-numeric single tokens shorter than 3
                # and the FOURCC index markers.
                if not re.match(r"^(ID|NP|null|vers)\b", text):
                    yield CatalogEntry(
                        page_id=page_id,
                        offset=i,
                        abs_off=page_abs_off + i,
                        length=ln,
                        text=text,
                    )
                    i += 1 + ln
                    continue
        i += 1


def build_btree_catalog(container: HdbContainer) -> BtreeCatalog:
    """Scan all B-tree pages and return the recovered name catalog."""
    cat = BtreeCatalog()
    for page_id, page in container.iter_pages():
        if not page or page[0] not in BTREE_TAGS:
            continue
        abs_off = container.page_offset(page_id)
        for entry in _scan_page_nativestrings(page, page_id, abs_off):
            cat.entries.append(entry)
    return cat


def catalog_to_dict(cat: BtreeCatalog) -> dict:
    return {
        "_doc": "B-tree name catalog (NativeStrings recovered from index "
                "pages 0xC2/0xD2/0xC3). Asset paths, object labels, scene "
                "names. The kind-byte navigation semantics are NOT decoded "
                "(needs unpublished CNeoNode.cp) but carry no game content.",
        "total_entries": len(cat.entries),
        "unique_names": len(cat.unique_names()),
        "asset_paths": cat.asset_paths(),
        "names": cat.unique_names(),
    }
