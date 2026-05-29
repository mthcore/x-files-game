"""Extract all printable ASCII strings from the HDB.

The HDB contains hundreds of readable strings : scene names, asset names,
production paths, variable names, etc. A byte-direct sweep produces them
all with their absolute offset, enabling full text-search of the database.

Strings are runs of >=4 consecutive printable ASCII (0x20..0x7e), bounded
by non-printable bytes.
"""
from __future__ import annotations

from hdb_extract.container.container import HdbContainer


def dump_strings(container: HdbContainer, min_len: int = 4) -> dict:
    raw = container.raw
    out: list[dict] = []
    i = 0
    while i < len(raw):
        b = raw[i]
        if 0x20 <= b < 0x7f:
            start = i
            while i < len(raw) and 0x20 <= raw[i] < 0x7f:
                i += 1
            if i - start >= min_len:
                # Decode as ASCII (safe — all chars are printable).
                text = raw[start:i].decode("ascii")
                out.append({
                    "abs_off": start,
                    "len": i - start,
                    "text": text,
                })
        else:
            i += 1
    return {"total": len(out), "min_len": min_len, "strings": out}
