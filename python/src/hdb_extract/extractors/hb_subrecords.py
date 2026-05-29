"""CNeoPart sub-record decoder — byte-direct STRUCTURE only, zero inference.

These decode the CNeoPart sub-records referenced by VCTrigger pairs:

  * Class ids 0x10-0x1d are NOT registered as named classes; they are
    CNeoPart sub-records managed by CNeoPartMgr (0x0a). They are labelled
    "part_0xNN", never with a fabricated name.

  * There is no fixed comparison-operator field. The only varying
    discriminator is at +14 and takes 2 values (0x01 x11, 0x11 x9) for
    class 0x14, measured byte-direct on the 20 real class-0x14 records. Its
    meaning is computed at run time and not stored in the file, so it stays
    `undetermined`. We expose the raw byte; we never guess the meaning.

VERIFIED 16-byte CNeoPart payload layout (measured on real XFILES.HDB,
classes 0x0f/0x11/0x14/0x1b/0x1c/0x1d, see hdb_part_subrecords.md):

    +0..+1   ref_a    BE u16   operand-A handle (NeoMark-like; monotonic)
    +2..+4   00 00 00          (always zero across all measured records)
    +5       class_a  u8        operand-A class_id (e.g. 0x43 VCBlob)
    +6       sub_flag u8        usually 0x00 (0x80 seen on some 0x1b)
    +7       00                 (always zero)
    +8..+9   ref_b    BE u16   operand-B handle / NeoID (0 when single-target)
    +10..+13 aux                mostly zero; record-specific bytes
    +14      relation u8        2-valued discriminator (semantics undetermined)
    +15      class_b  u8        operand-B class_id (0 when single-target action)

Everything below is byte-direct and certain EXCEPT the `relation`
semantics, which are honestly flagged `undetermined`.
"""
from __future__ import annotations

import struct
from dataclasses import dataclass


@dataclass
class PartRef16:
    """Verified 16-byte CNeoPart sub-record payload (byte-direct)."""
    ref_a: int        # +0  BE u16  operand-A handle
    class_a: int      # +5  u8       operand-A class_id
    sub_flag: int     # +6  u8       usually 0
    ref_b: int        # +8  BE u16  operand-B handle / NeoID
    aux: int          # +10 BE u32  record-specific (mostly 0)
    relation: int     # +14 u8       discriminator (semantics undetermined)
    class_b: int      # +15 u8       operand-B class_id

    @classmethod
    def read(cls, payload: bytes) -> "PartRef16 | None":
        if len(payload) < 16:
            return None
        return cls(
            ref_a=struct.unpack_from(">H", payload, 0)[0],
            class_a=payload[5],
            sub_flag=payload[6],
            ref_b=struct.unpack_from(">H", payload, 8)[0],
            aux=struct.unpack_from(">I", payload, 10)[0] & 0xFFFFFFFF,
            relation=payload[14],
            class_b=payload[15],
        )


def decode_part_ref(class_id: int, payload: bytes) -> dict | None:
    """Decode a CNeoPart sub-record (class_id in {0x0f,0x10,0x11,0x14,0x15,
    0x1b,0x1c,0x1d}) byte-direct. Returns None if payload < 16B.

    The `relation` byte is exposed raw with `relation_certainty:
    "undetermined"` — its meaning (e.g. condition operator vs. link type)
    is NOT inferred; it needs the runtime CNeoPart evaluate path.
    """
    ref = PartRef16.read(payload)
    if ref is None:
        return {"part_class": f"part_0x{class_id:02x}", "certainty": "truncated"}
    return {
        "part_class": f"part_0x{class_id:02x}",
        "operand_a_ref": ref.ref_a,          # byte-direct
        "operand_a_class_id": ref.class_a,    # byte-direct
        "operand_b_ref": ref.ref_b,          # byte-direct
        "operand_b_class_id": ref.class_b,    # byte-direct
        "sub_flag": ref.sub_flag,            # byte-direct (raw)
        "aux": ref.aux,                      # byte-direct (raw)
        "relation_raw": ref.relation,        # byte-direct (raw byte)
        "relation": None,                    # NOT decoded — see module doc
        "relation_certainty": "undetermined",
        "fields_certainty": "byte-direct",
    }
