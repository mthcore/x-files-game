"""HDB container : header + 256B pages + trailer.

The on-disk layout of XFILES.HDB :

    +0x00  HdbHeader (32 bytes, big-endian)
    +0x20  pages[N]   each 256 bytes
    +eof-T trailer    typically 176 zero bytes (G4)
"""
from hdb_extract.container.header import HEADER_SIZE, HdbHeader, parse_header
from hdb_extract.container.pages import (
    PAGE_SIZE,
    PAGE_TAG_NAMES,
    PageType,
    classify_page,
)
from hdb_extract.container.trailer import (
    TRAILER_EXPECTED_HDB,
    parse_trailer,
)
from hdb_extract.container.container import HdbContainer

__all__ = [
    "HEADER_SIZE",
    "PAGE_SIZE",
    "PAGE_TAG_NAMES",
    "PageType",
    "classify_page",
    "HdbHeader",
    "parse_header",
    "TRAILER_EXPECTED_HDB",
    "parse_trailer",
    "HdbContainer",
]
