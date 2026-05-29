"""FOURCC tags used in HDB records.

Each FOURCC is 4 ASCII bytes stored in stream order. We represent them
as big-endian uint32 so 'NPfl' (bytes 0x4e 0x50 0x66 0x6c) → 0x4e50666c.

Catalogued in `the format notes` :

    'NPfl' — NeoPersist FLags         (every record's base init flags)
    'vers' — Version                  (188 occurrences in HDB)
    'NPTc' — NeoPersist Type c        (polymorphic object marker — main)
    'NPTl' — Type l (length)
    'NPlt' — Type lt (list count marker)
    'NPTo' — Type o
    'NPrn' — Type rn
    'NPdu' — Type du
    'ID  ' — Record ID                (15,606 verbatim occurrences)
    'null' — NULL sentinel (factory)
"""
from __future__ import annotations


def fourcc_to_int(s: str) -> int:
    """Convert a 4-character FOURCC string to BE uint32."""
    if len(s) != 4:
        raise ValueError(f"fourcc must be 4 chars, got {len(s)}: {s!r}")
    b = s.encode("ascii")
    return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]


def fourcc_name(v: int) -> str:
    """Render a BE uint32 as a 4-character FOURCC string."""
    chars = [
        chr((v >> 24) & 0xff),
        chr((v >> 16) & 0xff),
        chr((v >> 8) & 0xff),
        chr(v & 0xff),
    ]
    return "".join(c if 32 <= ord(c) < 127 else "." for c in chars)


class FOURCC:
    NPfl = fourcc_to_int("NPfl")    # 0x4e50666c — flags byte
    vers = fourcc_to_int("vers")    # 0x76657273 — version dword
    NPTc = fourcc_to_int("NPTc")    # 0x4e505463 — polymorphic obj marker
    NPTl = fourcc_to_int("NPTl")    # 0x4e50546c — type l
    NPlt = fourcc_to_int("NPlt")    # 0x4e506c74 — list count marker
    NPTo = fourcc_to_int("NPTo")    # 0x4e50546f
    NPrn = fourcc_to_int("NPrn")    # 0x4e50726e
    NPdu = fourcc_to_int("NPdu")    # 0x4e506475
    ID = fourcc_to_int("ID  ")      # 0x49442020 — record ID
    NULL = fourcc_to_int("null")    # 0x6e756c6c — NULL sentinel
