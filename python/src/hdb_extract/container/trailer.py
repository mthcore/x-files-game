"""HDB trailer parser (G4 resolved 2026-05-17).

Per `the format notes` :

  - XFILES.HDB trailer = exactly 176 bytes, all zero (padding).
  - GAM files have a different (non-zero) 120-byte trailer containing
    persistent variable names — out of scope here.

Trailer size convention : (file_size - HEADER_SIZE) % PAGE_SIZE.
For XFILES.HDB :
    (6,074,064 - 32) % 256 = 176 bytes.
"""
from __future__ import annotations

from dataclasses import dataclass

TRAILER_EXPECTED_HDB = b"\x00" * 176


@dataclass(frozen=True)
class Trailer:
    raw: bytes
    is_zero_padding: bool

    @property
    def size(self) -> int:
        return len(self.raw)


def parse_trailer(raw: bytes) -> Trailer:
    """Validate and wrap the trailer bytes."""
    is_zero = all(b == 0 for b in raw)
    return Trailer(raw=raw, is_zero_padding=is_zero)
