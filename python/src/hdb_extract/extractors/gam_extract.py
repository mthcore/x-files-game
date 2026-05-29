"""GAM savegame extractor.

`XFILES.GAM` is a savegame file. It uses **the same container format**
as `XFILES.HDB` (NeoPersist 3.0) — same 32B header, same 256B pages,
same record markers. Only the file-version field differs (HDB = 3,
GAM = 5) and the trailer is non-zero (vs zero in HDB).

This means the HDB decoder reads GAM out of the box. This module
provides:

  * :class:`GamFile` — a thin wrapper that asserts GAM-specific
    invariants (version 5, distinct class set).
  * :func:`extract_gam_to_json` — dump the savegame to JSON for
    inspection or modding.

See :doc:`OSS/docs/GAM_FORMAT.md` for the on-disk diff between HDB and GAM.
"""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from hdb_extract.container.container import HdbContainer


GAM_FILE_VERSION = 5


class GamFile:
    """Wrapper over :class:`HdbContainer` with GAM-specific guards.

    The underlying parser is identical to the one used for HDB files.
    The only differences are:

    * ``self.container.header.version`` is expected to be 5 (vs 3 for HDB).
    * The trailer is **not** guaranteed to be all-zero (unlike HDB).
    * The class count is small (≈ 4 in the retail save) — only the
      classes actually mutated by the player are persisted.
    """

    def __init__(self, container: HdbContainer):
        self.container = container

    @classmethod
    def from_path(cls, path: str | Path) -> "GamFile":
        return cls(HdbContainer.from_path(path))

    @property
    def header(self):
        return self.container.header

    def is_gam(self) -> bool:
        """Return True if the file looks like a savegame (version == 5)."""
        return self.header.version == GAM_FILE_VERSION

    def roundtrip_ok(self) -> bool:
        return self.container.roundtrip_ok()

    def summary(self) -> dict[str, Any]:
        """Lightweight JSON-ready summary of the savegame."""
        c = self.container
        return {
            "file_size": len(c.raw),
            "header": {
                "version": c.header.version,
                "root_or_size": c.header.root_or_size,
                "record_count": c.header.record_count,
                "class_count": c.header.class_count,
                "page_size": c.header.page_size,
            },
            "pages": c.page_count,
            "trailer": {
                "size": c.trailer.size,
                "is_zero_padding": c.trailer.is_zero_padding,
            },
            "roundtrip_byte_identical": c.roundtrip_ok(),
            "is_gam": self.is_gam(),
        }


def extract_gam_to_json(gam_path: str | Path, out_path: str | Path) -> dict:
    """Write a JSON summary of a GAM file. Returns the summary dict."""
    gam = GamFile.from_path(gam_path)
    summary = gam.summary()
    out_p = Path(out_path)
    out_p.parent.mkdir(parents=True, exist_ok=True)
    out_p.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    return summary
