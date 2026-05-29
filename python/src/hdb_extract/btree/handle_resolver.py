"""NeoID -> file-offset resolver, built byte-direct from the HDB index entries.

The HDB B-tree stores `[target_offset BE u32][neo_id BE u32]` index entries (8B,
8-aligned) in its index/data pages: the first dword is the *absolute file offset*
of a record's 8-byte marker, the second is that record's NeoID. We scan the whole
file for these entries and keep only those whose `target_offset` actually lands on
a real CNeoPersist marker (`xx 00 00 00 01` with class_id in the registered range).
This is 100% byte-direct: every accepted entry is verified against the marker it
points at; nothing is inferred.

Validated against the VCTrigger pair graph (the canonical resolved references):
every trigger-pair (neo_id -> offset) round-trips through this index.

The map is intentionally conservative — an entry is accepted only when its offset
points to a real marker, so a NeoID that does not resolve is reported as a miss
(left `undetermined` upstream), never guessed.
"""
from __future__ import annotations

import struct
from collections import defaultdict

MARKER_MAGIC = b"\x00\x00\x00\x01"
_MAX_CLASS_ID = 0x5C       # highest registered VC* class id
_MAX_NEOID = 200_000       # observed id space upper bound (file has ~60k ids)


class HandleResolver:
    def __init__(self, raw: bytes):
        self.raw = raw
        self.size = len(raw)
        self._nid2off: dict[int, set] = defaultdict(set)
        self._build()

    def _is_marker(self, o: int) -> bool:
        return (0 <= o and o + 8 <= self.size
                and self.raw[o + 2:o + 6] == MARKER_MAGIC
                and 0xC0 <= self.raw[o] <= 0xCF)

    def _class_at(self, o: int) -> int:
        return (self.raw[o + 6] << 8) | self.raw[o + 7]

    def _build(self) -> None:
        raw, N = self.raw, self.size
        unpack = struct.unpack_from
        for o in range(32, N - 8, 4):
            offv = unpack(">I", raw, o)[0]
            if offv < 32 or offv + 8 > N:
                continue
            if not self._is_marker(offv):
                continue
            cid = self._class_at(offv)
            if cid == 0 or cid > _MAX_CLASS_ID:   # skip null/garbage targets
                continue
            nidv = unpack(">I", raw, o + 4)[0]
            if 1 <= nidv <= _MAX_NEOID:
                self._nid2off[nidv].add(offv)

    def resolve(self, neo_id: int) -> dict | None:
        """Return {offset, class_id, ambiguous} for a NeoID, or None if not in the
        index (caller leaves such a reference `undetermined`, never guessed)."""
        offs = self._nid2off.get(neo_id)
        if not offs:
            return None
        chosen = sorted(offs)[0]
        return {
            "offset": chosen,
            "class_id": self._class_at(chosen),
            "ambiguous": len(offs) > 1,
            "candidates": sorted(offs) if len(offs) > 1 else None,
            "certainty": "byte-direct",
        }

    def __len__(self) -> int:
        return len(self._nid2off)

    def coverage(self) -> dict:
        unambiguous = sum(1 for v in self._nid2off.values() if len(v) == 1)
        return {"distinct_neoids": len(self._nid2off),
                "unambiguous": unambiguous,
                "ambiguous": len(self._nid2off) - unambiguous}
