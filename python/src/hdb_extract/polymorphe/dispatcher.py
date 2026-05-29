"""Polymorphic record reader for HDB on-disk format.

The on-disk HDB format does NOT inline polymorphic sub-objects.
Each `SubObj` field is a bookmark/handle (8-12 bytes) referencing the
sub-object's storage location elsewhere in the HDB. The on-disk
structure shows this :

    Path A (slot_0x18() != 0 == HDB mode) :
        [BBmk 4B (or 8B if vers>4)] [BBIn 4B] = 8 or 12 bytes total
        BBmk = "BookmarkMark" — storage reference
        BBIn = "BookmarkIndex" — index in that storage

    Path B (slot_0x18() == 0 == in-memory/save mode) :
        Inline class_id + record bytes (used by the InMemoryHDBSerializer
        for save files, NOT for HDB)

For on-disk HDB decoding, we follow Path A : read 8-12 bytes of handle
and record it as a `_handle` dict. Resolving the handle to the actual
sub-object record requires walking CNeoIDList — done at a higher level.

For object lists, the on-disk format is a fixed-size
HEADER (16-24 bytes : NPTc + null + conditional NPTl + NPlt + NPTo +
version-conditional NPrn/NPdu + ID). The list ELEMENTS are stored
separately, addressed by the embedded ID. We read just the header.
"""
from __future__ import annotations

from typing import Any

from hdb_extract.format.fourcc import FOURCC
from hdb_extract.serializer.cursor import HDBSerializer


def read_obj(ser: HDBSerializer, class_id_marker: int = FOURCC.NPTc) -> Any:
    """Read a polymorphic sub-object HANDLE (Pattern A) from on-disk HDB.

    Consumes 8-12 bytes :
      - 4 bytes (BBmk part 1, BE) — storage_ref
      - 4 bytes (BBmk part 2, BE, only if record version > 4) — storage_ref_ext
      - 4 bytes (BBIn, BE) — index in storage

    Returns a dict with the handle parts, or None on error/null.
    """
    if ser.error or ser.eof:
        return None

    # BBmk part 1 (4B BE)
    bbmk_1 = ser.read_dword(0)  # tag arg ignored on disk
    if ser.error:
        return None
    if bbmk_1 == FOURCC.NULL or bbmk_1 == 0:
        # null sentinel — no sub-object linked
        return None

    # BBmk part 2 (4B BE, only if vers > 4 in current record)
    bbmk_2 = 0
    if ser.version > 4:
        bbmk_2 = ser.read_dword(0)
        if ser.error:
            return None

    # BBIn (4B BE) — index in storage
    bbin = ser.read_dword(0)
    if ser.error:
        return None

    return {
        "_handle": True,
        "_handle_storage_ref": bbmk_1,
        "_handle_storage_ref_ext": bbmk_2 if ser.version > 4 else None,
        "_handle_index": bbin,
        "_class_id_marker": class_id_marker,
    }


def read_obj_list(
    ser: HDBSerializer,
    count_marker: int = FOURCC.NPlt,
    class_id_marker: int = FOURCC.NPTc,
) -> list:
    """Read a polymorphic object-list HEADER from on-disk HDB.

    The on-disk format is NOT a count + inline records iteration.
    It's a fixed-size header :
        [NPTc 4B] [null 4-8B] [NPTl 4B if flag_c, else default 0xb]
        [NPlt 4B if flag_d, else default 'null']
        [NPTo 4B if flag_e, else default 1]
        [NPrn+NPdu 2B if vers > 0x4ff]
        [ID 4B if vers > 0x500]

    Conservative implementation : read the COMMON case (NPTc + null + NPTl)
    = 12 bytes. Returns a single-item list wrapping the header dict.
    The actual list elements are resolved via the ID at a higher level.
    """
    items: list = []
    if ser.error or ser.eof:
        return items

    # NPTc-tagged class_id (4 BE)
    class_id = ser.read_dword(0)
    if ser.error:
        return items
    # 'null' marker — 4 or 8 bytes depending on version
    null_part = ser.read_dword(0)
    null_part_2 = None
    if ser.version > 4:
        null_part_2 = ser.read_dword(0)
    # NPTl tag (4B) — common case, no conditional check (we assume present)
    count_or_default = ser.read_dword(0)
    if ser.error:
        return items

    items.append({
        "_list_header": True,
        "_list_class_id": class_id,
        "_list_null_part": null_part,
        "_list_null_part_2": null_part_2,
        "_list_count_or_id": count_or_default,
    })
    return items
