"""Complete byte-direct dump of the entire HDB to JSON.

This is the literal "100% extract" — every byte is represented in the
output JSON in some form :

  - Header : parsed into named fields
  - Each page : tag classification + hex content + ASCII runs + markers
  - Trailer : raw bytes (zero-padded in HDB)

The output is large (~100MB JSON) but fully reversible : you can
reconstruct the HDB byte-for-byte from it (proven by `verify_dump`).

Records that the FOURCC remap (G1) has not yet decoded are present as
raw bytes — they're still extracted, just not semantically decoded.
"""
from __future__ import annotations

import re
from typing import Any

from hdb_extract.container.container import HdbContainer
from hdb_extract.container.pages import PAGE_TAG_NAMES

_ASCII_RUN = re.compile(rb"[\x20-\x7e]{4,}")


def dump_complete(container: HdbContainer) -> dict[str, Any]:
    """Produce the full byte-direct representation."""
    out = {
        "source": str(container.path) if container.path else "<bytes>",
        "size_bytes": len(container.raw),
        "header": {
            "version": container.header.version,
            "root_or_size": container.header.root_or_size,
            "record_count": container.header.record_count,
            "class_count": container.header.class_count,
            "unk_94982": container.header.unk_94982,
            "unk_1281": container.header.unk_1281,
            "max_or_limit": container.header.max_or_limit,
            "page_size": container.header.page_size,
            "hex": container.raw[:32].hex(),
        },
        "pages": [],
        "trailer": {
            "size": container.trailer.size,
            "is_zero_padding": container.trailer.is_zero_padding,
            "hex": container.trailer.raw.hex(),
        },
    }

    for page_id, page in container.iter_pages():
        tag = page[0] if page else None
        ascii_runs = []
        if page:
            for m in _ASCII_RUN.finditer(page):
                ascii_runs.append({
                    "off": m.start(),
                    "text": m.group(0).decode("ascii"),
                })
        markers = []
        for marker in (b"ID  ", b"NPfl", b"NPTc", b"NPlt", b"vers", b"null"):
            idx = 0
            while True:
                idx = page.find(marker, idx)
                if idx < 0:
                    break
                markers.append({
                    "off": idx,
                    "marker": marker.decode("ascii"),
                })
                idx += len(marker)
        out["pages"].append({
            "page_id": page_id,
            "abs_off": container.page_offset(page_id),
            "tag_hex": f"0x{tag:02x}",
            "tag_name": PAGE_TAG_NAMES.get(tag, "data"),
            "is_structural": tag in {0x00, 0xC0, 0xC2, 0xC3, 0xD2},
            "kind_byte": page[1] if len(page) > 1 else None,
            "hex": page.hex(),
            "ascii_runs": ascii_runs,
            "markers": markers,
        })
    return out


def verify_dump(container: HdbContainer, dump: dict[str, Any]) -> bool:
    """Verify the dump can reconstruct the exact original bytes."""
    # Re-emit from the JSON dump's hex fields.
    out = bytearray()
    out += bytes.fromhex(dump["header"]["hex"])
    for p in dump["pages"]:
        out += bytes.fromhex(p["hex"])
    out += bytes.fromhex(dump["trailer"]["hex"])
    return bytes(out) == container.raw
