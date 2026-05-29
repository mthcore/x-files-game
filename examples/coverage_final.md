# HDB Coverage — final 100% strict byte-direct certificate

**Date** : 2026-05-19
**Source** : `XFILES.HDB` (6,074,064 bytes)
**Container roundtrip byte-identical** : ✅ **TRUE**
**pytest tests** : ✅ **50/50 green**
**ClassSpec coverage** : ✅ **51/51 VC* classes**

## 100% byte-direct breakdown

| Usage code | Bytes | % | Description |
|------------|-------|---|-------------|
| `PAGE_EMPTY` | 3,642,930 | 59.98% | 0x00 pages, reserved zero-padding (allocated but unused) |
| `PAGE_DATA` | 1,863,540 | 30.68% | "data" pages : user VC* records (triggers, hotspots, conversations, decision trees, emails, etc.) |
| `PAGE_INTERNAL` | 299,880 | 4.94% | B-tree internal nodes (tags 0xC2 / 0xD2), indexing structure |
| `PAGE_FREED` | 192,270 | 3.17% | 0xC0 pages freed after delete, opaque content |
| `PAGE_LEAF_BODY` | 49,376 | 0.81% | Bodies of the 202 B-tree leaf nodes (tag 0xC3) |
| `PAGE_TAG` | 23,726 | 0.39% | First byte of each page (page type discriminator) |
| `NEOIDLIST_HEADER` | 1,140 | 0.02% | CNeoIDList 30B records walked (38 records) |
| `PAGE_HEADER` | 994 | 0.02% | 5-byte header of each 0xC3 leaf (`tag/kind/3B reserved`) |
| `TRAILER_ZERO` | 176 | 0.003% | Final zero padding for quantum-8 alignment |
| `HEADER` | 32 | 0.001% | File header (version, root, record_count, class_count, etc.) |
| **TOTAL** | **6,074,064** | **100.00%** | |
| `UNKNOWN` | **0** | **0.00%** | No unclassified byte |

## Byte-direct guarantees provided

### G1 — Byte-identical round-trip
```
$ python -m hdb_extract verify XFILES.HDB
container roundtrip byte-identical : True

$ python -m hdb_extract dump-complete XFILES.HDB --out=out/complete_dump.json
Building byte-direct dump of 6,074,064 bytes...
  pages : 23,726
wrote out\complete_dump.json  (26,497,315 bytes)
roundtrip from dump byte-identical : True
```

### G2 — Wire format understood (Sprint 1.C resolved)
- `kNeoPersistFileLength = 6B` confirmed via the NeoPersist 3.0 headers (`CNeoPersist.h`)
- Marker 8B = on-disk `CNeoPersist` (`[2B flags 'NPfl'][4B NeoVersion BE 'vers']`) + 2B alignment `kNeoDatabaseQuantum = 8`
- **No FOURCC→byte-tag table to decode** : `CNeoStream::readBits/readLong` pass the tag in `NeoUsed(aTag)` (documentary), `CNeoFileStream` never writes the tags to disk
- Main dispatcher : `CNeoPersist::readObject` in the native engine

### G3 — 51/51 VC* classes spec'd byte-exact
- [hdb_extract/src/hdb_extract/classes/generated/all_classes.py](../../../hdb_extract/src/hdb_extract/classes/generated/all_classes.py) contains the 51 ClassSpec
- VCTrigger fixed 2026-05-19 (was a NOP, added `Dword(0x65,'e') + Byte(0x66,'f')` per the native reader)
- The `test_classes_apply_read.py` tests cover the byte-direct application

### G4 — Trailer closed (2026-05-17)
- 176 bytes (not 208), strict zero padding
- Confirmed by `c.trailer.is_zero_padding == True`

### G5 — NL bytecode absent (2026-05-19)
- Verdict : NL bytecode absent (byte-direct format analysis)
- No VC* class reads an inline bytecode stream ; everything is typed records

### G6 — B-tree internal structure documented (70% semantic, 100% structural)
- Byte-direct markers identified via the `[0xCX][sub_tag][00 00 00 01]` invariant
- Exhaustive semantics of the 56 (tag, kind) combinations blocked by the absence of `CNeoNode.cp` in the public NeoLogic SDK
- **Bytes classified 100%** via `PAGE_INTERNAL` (4.94%) + roundtrip-identical

## Conclusion

The `hdb_extract` decoder reaches **100% strict byte-direct** for the `XFILES.HDB` file of the game *The X-Files: The Game* (1997). Every byte of the file is :

1. **Structurally classified** under one of the 10 categories (header, page tag, page body by type, trailer)
2. **Represented byte-identical in a JSON dump** (`dump-complete`)
3. **Bit-for-bit reversible** : reconstruction of the original HDB from the dump confirmed

The fine-grained semantics (per field of each VC* class) are available for **51/51 user classes** via the `ClassSpec`. The fine-grained semantics of the B-tree internal nodes (G6) remain indexed by class, but the exact layout of the entries depends on non-public NeoLogic sources — this does **not** prevent 100% byte-direct decoding and round-trip.

**Phase 1 (cracking) considered COMPLETE for the OSS objectives.**
