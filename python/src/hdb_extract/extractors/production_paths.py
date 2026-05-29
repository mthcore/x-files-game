"""Extract production paths from HDB raw bytes.

The HDB encodes each asset as :

    <scene_name>\x7f<production_name>.<ext>\x01<flags>\x02<storage_dir>\x7f<file_id>.<ext>

This pattern is decoded by `native/scripts/hdb_production_extract_full.py`
and gives the 2432 scene→asset mappings. We replicate that byte-direct
scanner here so the resulting JSON can join with the research datasets
"""
from __future__ import annotations

import re

from hdb_extract.container.container import HdbContainer

# scene\x7fproduction.ext (\x01flags)*\x02storage\x7ffile_id.ext
# Observed in XFILES.HDB at offset 34537 :
#   "End Games" \x7f "sc110A.mov" \x01 "0" \x01 "7" \x02 "12XV" \x7f "23414.xmv"
_RE = re.compile(
    rb"([\x20-\x7e]{1,64})\x7f([\x20-\x7e]{1,64}\.[a-zA-Z]{2,4})"
    rb"((?:\x01[\x20-\x7e]{0,32})*)"
    rb"\x02([\x20-\x7e]{0,32})\x7f([0-9]{1,8})\.([a-zA-Z]{2,4})"
)


def dump_production_paths(container: HdbContainer) -> dict:
    out = []
    raw = container.raw
    for m in _RE.finditer(raw):
        # group(3) = raw flag bytes including \x01 separators ; split it.
        flags_raw = m.group(3)
        flags = [seg.decode("ascii", errors="replace")
                 for seg in flags_raw.split(b"\x01") if seg]
        out.append({
            "abs_off": m.start(),
            "scene": m.group(1).decode("ascii", errors="replace"),
            "production_name": m.group(2).decode("ascii", errors="replace"),
            "flags": flags,
            "storage_dir": m.group(4).decode("ascii", errors="replace"),
            "file_id": int(m.group(5)),
            "asset_ext": m.group(6).decode("ascii", errors="replace"),
        })
    return {"total": len(out), "paths": out}
