"""HDB record index — byte-direct, mirrors the C++ reference decoder (see `cpp/`).

Scans every 0xC0-0xCF page of the HDB and indexes every CNeoPersist
record by its absolute byte offset, exposing its class_id and payload.

KEY FACT (verified 2026-05-20): the record's **class_id is bytes +6..+7
of the 8-byte marker** (big-endian). The full marker is:

    +0  flags1   (0xC0..0xCF)        ─┐ packed_flags (BE u16)
    +1  flags2   (sub_tag)           ─┘
    +2  version  (4B BE = 00 00 00 01 = kNeoVersionDefault)
    +6  class_id (BE u16)            <- THE class id (0x27..0x5C for VC*,
                                        0x10..0x1E for HB sub-records,
                                        0x00..0x0B for NeoAccess system)

Payload of a record runs from marker+8 to the next marker in the page
(or page end). This is the on-disk slot grid.
"""
from __future__ import annotations

from dataclasses import dataclass

from hdb_extract.container.container import HdbContainer

HEADER_SIZE = 32
PAGE_SIZE = 256
MARKER_MAGIC = b"\x00\x00\x00\x01"

# Class IDs and names taken from the NeoPersist class registry that the HDB
# stores in its own NeoClass/NeoSubclass records — byte-direct, ground truth.
#
# IMPORTANT: class ids 0x10-0x1d are NOT registered as named classes. They are
# CNeoPart sub-records (managed by CNeoPartMgr 0x0a); they are labelled
# "part_0xNN", never with a fabricated name.
CLASS_NAMES = {
    0x00: "NeoNullClass", 0x01: "NeoPersist", 0x02: "NeoBlob",
    0x03: "NeoClass", 0x04: "NeoSubclass", 0x05: "NeoFreeList",
    0x06: "NeoInode", 0x07: "NeoIDIndex", 0x08: "NeoParentIndex",
    0x09: "NeoGenericIndex", 0x0A: "NeoPartMgr", 0x0B: "NeoIDList",
    0x1E: "PersistentList",
    0x27: "VCTitle", 0x28: "VCNode", 0x29: "VCLocaton", 0x2A: "VCViewPoint",
    0x2B: "VCView", 0x2C: "VCCharacter", 0x2D: "VCCharView",
    0x2E: "VCCharViewList", 0x2F: "VCName", 0x31: "VCConversation",
    0x32: "VCConversationList", 0x33: "VCNav", 0x34: "VCNav_List",
    0x35: "VCAssetRef", 0x36: "VCAssetRefList", 0x37: "VCExplorable",
    0x38: "VCExplorableList", 0x39: "VCHotSpot", 0x3A: "VCHotSpotList",
    0x3B: "VCStdHotSpotList", 0x3C: "VCAssetCategory", 0x3D: "VCCursor",
    0x3E: "VCDefaultCursors", 0x40: "VCDefaultHotSpots", 0x41: "VCAction",
    0x42: "VCActionList", 0x43: "VCBlob", 0x44: "VCPlayer", 0x46: "VCEnabled",
    0x47: "VCActionIcon", 0x48: "VCEmotionIcon", 0x49: "VCEvidenceIcon",
    0x4A: "VCIdeaIcon", 0x4B: "VCInventory", 0x4C: "VCIdeaMap",
    0x4D: "VCIdeaMapList", 0x4E: "VCIFaceLayout", 0x4F: "VCInterfaceItem",
    0x50: "VCString", 0x51: "VCTrigger", 0x52: "VCTriggerList",
    0x53: "VCVariable", 0x54: "VCStdAction", 0x55: "VCStdActionList",
    0x56: "VCConversationHistory", 0x57: "VCGameState", 0x58: "VCPDANotes",
    0x59: "VCPhoto", 0x5A: "VCEmail", 0x5B: "VCEmailRead", 0x5C: "VCEmailPending",
}

# CNeoPart sub-record class_ids (NOT registered metaclasses).
PART_SUBRECORD_IDS = {0x0F, 0x10, 0x11, 0x14, 0x15, 0x1B, 0x1C, 0x1D}


def class_label(class_id: int) -> str:
    """Registry class name, or an honest part-subrecord label."""
    if class_id in CLASS_NAMES:
        return CLASS_NAMES[class_id]
    if class_id in PART_SUBRECORD_IDS:
        return f"part_0x{class_id:02x}"
    return f"0x{class_id:02x}"


# Convenience constants.
kClassVCTrigger = 0x51


@dataclass
class RecordRef:
    byte_offset: int       # absolute offset in HDB
    payload_size: int      # bytes after the 8-byte marker (to next marker)
    class_id: int          # from marker bytes +6..+7
    packed_flags: int      # marker bytes +0..+1
    page_id: int


class HDBRecordIndex:
    """Index of every CNeoPersist record in the HDB (by offset + class)."""

    def __init__(self, container: HdbContainer):
        self.raw = container.raw
        self.size = len(self.raw)
        self.records: list[RecordRef] = []
        self._by_offset: dict[int, int] = {}
        self._build()

    def _build(self) -> None:
        raw = self.raw
        page_count = (self.size - HEADER_SIZE) // PAGE_SIZE
        for pi in range(page_count):
            page_off = HEADER_SIZE + pi * PAGE_SIZE
            if not (0xC0 <= raw[page_off] <= 0xCF):
                continue
            marker_offs: list[int] = []
            for off in range(0, PAGE_SIZE - 8 + 1, 8):
                a = page_off + off
                if not (0xC0 <= raw[a] <= 0xCF):
                    continue
                if raw[a + 2:a + 6] != MARKER_MAGIC:
                    continue
                marker_offs.append(off)
            for i, off in enumerate(marker_offs):
                next_off = marker_offs[i + 1] if i + 1 < len(marker_offs) else PAGE_SIZE
                a = page_off + off
                r = RecordRef(
                    byte_offset=a,
                    payload_size=next_off - off - 8,
                    class_id=(raw[a + 6] << 8) | raw[a + 7],
                    packed_flags=(raw[a] << 8) | raw[a + 1],
                    page_id=pi,
                )
                self._by_offset[a] = len(self.records)
                self.records.append(r)

    def find_by_offset(self, off: int) -> RecordRef | None:
        idx = self._by_offset.get(off)
        return self.records[idx] if idx is not None else None

    def find_by_class(self, class_id: int) -> list[RecordRef]:
        return [r for r in self.records if r.class_id == class_id]

    def payload_at(self, off: int) -> bytes | None:
        if off + 8 > self.size:
            return None
        return self.raw[off + 8:]

    def class_histogram(self) -> dict[int, int]:
        h: dict[int, int] = {}
        for r in self.records:
            h[r.class_id] = h.get(r.class_id, 0) + 1
        return h
