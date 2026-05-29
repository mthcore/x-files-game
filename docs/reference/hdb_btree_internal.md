# G6 (ex-G3) — HDB B-tree Internal Pages (0xC2 / 0xD2)

**Status** : 70% decoded — generic structure of the sub-record markers mapped empirically (Sprint 1.A.1, 2026-05-19). Per-sub_tag semantics (the meaning of each sub_tag value 0x30/0x31/0x32/0x70/0x71/0xB0/0xB1/0xF0/0xF1/...) still need to be established from the on-disk structure (Sprint 1.A.2).

## Sprint 1.A.1 — Cartography findings (2026-05-19)

Full report : [g6_kinds_cartography.md](g6_kinds_cartography.md).

### Generic structure observed (all 0xC2 pages, and the 0xD2 pages with kind=0x30/0x32)

Each 256-byte page is a **list of nested records**. Each record starts with an 8-byte marker :

```
+0  tag       (0xC0..0xCF, dominant 0xC2 / 0xC3 / 0xC0)
+1  sub_tag   (0x30 / 0x31 / 0x32 / 0x70 / 0x71 / 0xB0 / 0xB1 / 0xF0 / 0xF1 / ...)
+2  reserved  (0x00)
+3  reserved  (0x00)
+4  reserved  (0x00)
+5  magic     (0x01 ← invariant used by the scan)
+6  flag16    (BE u16, observed value varies)
```

The record payload begins at `marker_off + 8` and runs up to the next marker OR to the end of the page.

**For 0xC2 all-kinds** : the page tag (`0xC2`) AND the `kind` byte (`0x30/0x31/0x32/...`) form the **first marker of the page** (marker at offset 0). The 6B "page header" and the "first marker" overlap.

**For 0xD2 all-kinds** : some kinds (0x30, 0x32) start with 8-136B of non-marker bytes (a dedicated header) before the first sub-record marker ; other kinds (0x00, 0x70, 0x71, 0x76) have the same structure as 0xC2 (marker at offset 0).

### Global statistics

| Tag | # distinct kinds | # pages | Total bytes |
|-----|-------------------|---------|-------------|
| 0xC2 | 38 | 989 | 253,184 |
| 0xD2 | 18 | 187 |  47,872 |
| **Total** | **56 combinations** | **1,176** | **301,056** |

Dominant 0xC2 kinds : `0x30` (369), `0x31` (291), `0x32` (70), `0x71` (58), `0x70` (38), `0xF0` (30), `0x00` (28), `0xF1` (23).
Dominant 0xD2 kinds : `0x30` (101), `0x32` (39), `0x31` (14), `0x70` (8), `0x00` (6).

### Recurring FOURCCs in the payloads (evidence of nesting)

- `'ID  '` (CNeoIDList class_id pointer) appears in **97%** of 0xC2 kind=0x30 pages, and 59% of kind=0x31.
- `'null'`, `'.mov'`, `'Navs'`, `'credits'` appear in 21-40% of kind=0x31 payloads — these are **string contents** inside sub-records, not structural tags.

### Marker spacing variability

For 0xC2 kind=0x30 (369 pages, the most common), the **number of markers per page** ranges from 1 to 13 (distribution : `{1:15, 2:40, 3:65, 4:66, 5:54, 6:50, 7:20, 8:19, 9:15, 10:9, 11:14, 12:1, 13:1}`). The dominant inter-marker spacings are `(168, 32)` × 21, `(168,)` × 20, `(24,) × 10`. So each page holds a variable number of records of sizes 8, 16, 24, 32, 40, 48, 56, 64, 168 bytes.

This is NOT a classic B-tree node (which would have a fixed record size). It is rather a **container of heterogeneous records** where the marker's `sub_tag` identifies the payload format.

### Implications for Sprint 1.A.2-1.A.4

1. The reference C++ walker (`walk_btree_records`, see `cpp/`) correctly finds every marker via the `0xCX + magic` invariant, but does not decode the payload.

2. To reach **100% strict byte-direct semantics**, the payload must be decoded **per sub_tag**. The `sub_tag` byte (marker offset +1) takes ~50 distinct values in total across the 1176 0xC2/0xD2 pages.

3. The `sub_tag → reader function` dispatch table still needs to be established from the on-disk structure. The HdbSerializer vtable (already identified) exposes these readers (slots `0x80/0x9c/0xb0/0x74/0xbc/0x138` already identified ; the other slots are the candidates).

4. **Next step (Sprint 1.A.2)** : establish from the on-disk structure the `read_subtag_XX` slots of the HdbSerializer vtable, and the main dispatcher that picks the slot based on `sub_tag`.

## Sprint 1.A.2 — Reading the NeoPersist 3.0 SDK (2026-05-19)

Reading the public NeoPersist 3.0 headers ([NeoPersist 3.0 headers](../neopersist_3.0.8/)) + public CW7 sources.

### Byte-direct confirmations from the headers

1. **`kNeoPersistFileLength = 2 + sizeof(NeoVersion) = 6 bytes`** ([CNeoPersist.h:175](../neopersist_3.0.8/CNeoPersist.h)) — the on-disk CNeoPersist is exactly 6 bytes : 2B flags + 4B BE version.
2. **`kNeoDatabaseQuantum = 8`** ([CNeoDatabase.h:30](../neopersist_3.0.8/CNeoDatabase.h)) — all allocations aligned to 8 bytes → confirms why the markers always fall at offsets that are multiples of 8.
3. **`kNeoDatabaseHeadSpace = 0x100 = 256 bytes`** — reserved space of the file header (32B significant + 224B padding).
4. **`kNeoVersionDefault = 1`** ([NeoTypes.h:80](../neopersist_3.0.8/NeoTypes.h)) — explains the `00 00 00 01` magic constant at offset +2..+5 of each marker.
5. **`kNeoNoTag = Neo4CharConst('nu', 'll')`** ([NeoTypes.h:98](../neopersist_3.0.8/NeoTypes.h)) — explains the `'null'` FOURCC that appears in 40% of 0xC2 kind=0x30 pages ; it is the default NeoTag passed to readChunk when no specific tag is required.

### NeoAccess system class IDs (NeoTypes.h:289-300)

```c
kNeoNullClassID     = 0
kNeoPersistID       = 1
kNeoBlobID          = 2
kNeoClassID         = 3
kNeoSubclassID      = 4
kNeoFreeListID      = 5
kNeoInodeID         = 6
kNeoIDIndexID       = 7   // B-tree index by NeoID
kNeoParentIndexID   = 8   // B-tree parent index
kNeoGenericIndexID  = 9   // generic B-tree index
kNeoPartMgrID       = 10
kNeoIDListID        = 11  // = 0x0b, confirmed empirically on page 91
```

Byte-direct confirmation on page 22299 : 4 records of 32B with class_id encoded at offset +12 = `00 00 00 09` = `kNeoGenericIndexID`. Page 91 confirmed byte-direct : 32B record with class_id at offset +16 = `00 0b 49 44` → `class_id=0x0B` + FOURCC `'ID  '`.

### Hypotheses TESTED and REFUTED

**Hypothesis H1 : `sub_tag byte = CNeoPersist flags packed`** (bit field `fLeaf/fRoot/fTemporary/fLocal/fShareFill/fBusy/fBool1`).

Test : if true, then the 0xC3 pages (btree_leaf) would be mostly `fLeaf=1`.
- With LSB-first decoding : 0xC3 dominant kinds = 0x70 (25%, fLeaf=0), 0x31 (23%, fLeaf=1), 0x71 (19%, fLeaf=1), 0x30 (16%, fLeaf=0) — a **mixed** distribution, fLeaf=1 accounts for only ~42%.
- With MSB-first decoding : even less coherent.

**Conclusion** : H1 is REFUTED. The sub_tag byte is NOT directly the CNeoPersist flags. It probably encodes some other information (B-tree level ? index type ? a combination of flags + counter ?).

### Limits of the format analysis

The distributed NeoPersist 3.0 SDK (CW7 / DoomMacSource) contains the **complete headers** but only **2 implementations** (`CNeoStream.cp` + `NeoAccess.cp`). The implementations critical to the wire format of the B-tree internal pages — `CNeoNode.cp`, `CNeoIndex.cp`, `CNeoIDIndex.cp`, `CNeoGenericIndex.cp` — are **not provided** in the public SDK.

Without these sources, the exact semantic decoding of the `sub_tag` byte (and therefore the 100% byte-direct semantics of each B-tree internal page) remains **unresolved**. The same observation had been made previously.

### Options to reach 100% strict byte-direct semantics

| Option | Effort | Risk | Benefit |
|--------|--------|------|----------|
| A. Analyze the page access order (`CNeoIndex::find` etc.) | 20-40h | medium  | 100% B-tree semantics |
| B. Re-download MacFormat-023 or Mac developer archives to find CNeoNode.cp | 5-15h | high (public sources are very old, variable quality) | 100% semantics if found |
| C. Accept "100% structural byte-direct" (markers + class_id encoding) + payload opaque per class_id | 0h | none | 70% semantics, but 100% structural byte-direct round-trippable |
| D. Pivot to G1+G2 (which unlock 31% of the file vs G6 = 5%) | 60-90h G1+G2 | medium | 100% semantics on the VC* user classes (where triggers, hotspots, dialogues live) |

**Recommendation** : Option **D** (pivot to G1+G2) + **C** (freeze G6 at 70% structural). The VC* user classes — where the triggers/hotspots/dialogues/decision trees live (the ones AGRIPPA could never read) — are decodable byte-direct via the HdbSerializer vtable, which is **already identified and partially documented** (5 known slots : `0x74/0x80/0x9c/0xb0/0xbc/0x138`). The OSS benefit for the community is concentrated in G1+G2, not G6.

## Reframing 2026-05-17 (session)

The analysis of the on-disk structures ruled out two false leads :

1. A neighboring FOURCC is actually a **media format detector**
   (FORM/AIFF/RIFF/AVI/BMP/GIF magic). Unrelated to HDB.
2. **(HIBYTE & 0xC0) == 0xC0** is a **clipping flag**
   (Cohen-Sutherland) on the rendering side. Unrelated to the HDB pages.
3. **VCObject slot 9** is a generic property accessor
   that dispatches on FOURCC ('NPfl', 'NPb1', 'NPpa', etc.), but for the
   **runtime property system**, not for HDB B-tree navigation.

The real B-tree navigator therefore belongs to the NL/Hammer layer.

## Strategy to finish G3

Rather than getting bogged down on the false leads :

1. **Page access observation** : spot the reads of
   0x100 = 256 bytes (page size) on the HDB to identify the navigation
   order.
2. **Cross-ref via the reference decoder** : the HDBSerializer is
   already delivered (see `cpp/`) ; check whether the B-tree walker is
   ported there.
3. **Follow the `root_or_size` field** : the header indicates that
   root_or_size points somewhere ; establish its role from the
   on-disk structure.

## Context

XFILES.HDB uses a B-tree to index its 64,896 records. Each page's tag
indicates its role :

| Tag | Name | Count | Bytes |
|-----|-----|-------|-------|
| 0xC2 | btree_internal | 989 | 253,184 |
| 0xC3 | btree_leaf | 202 | 51,712 |
| 0xC0 | btree_freed | 754 | 193,024 |
| 0xD2 | btree_internal_alt | 187 | 47,872 |

Total btree pages : 1,176 internal (C2+D2) + 202 leaves (C3) + 754 freed (C0) = 2,132 structural pages, out of 23,726 total pages.

## Findings from statistical inspection (2026-05-17)

### Common page header (6 bytes)

All structural pages (0xC2, 0xC3, 0xD2) start with :

```
+0  tag         (0xC2 / 0xC3 / 0xD2)
+1  kind        (0x30='0' / 0x31='1' / 0x32='2' typically; outliers : 0x70, 0x00)
+2  reserved    (00)
+3  reserved    (00)
+4  reserved    (00)
+5  flag        (01)
```

Distribution observed on 989 0xC2 pages :
- 369 pages with kind=0x30 ('0')
- 291 pages with kind=0x31 ('1')
- 70 pages with kind=0x32 ('2')
- ~259 pages with other kinds (to be investigated)

The `kind` looks like a **level** in the tree :
- 0x30 = '0' → low level (close to the leaves)
- 0x32 = '2' → high level (close to the root)

But it is encoded in ASCII (`'0'`, `'1'`, `'2'`) — a curious convention inherited
from NeoLogic Hammer (Mac-Apple origin, big-endian format).

### Record patterns observed (to be confirmed by format analysis)

**Page 91 (0xD2, kind=0x70)** : contains 30-byte CNeoIDList records
starting at offset +2 :

```
+02..+05  00 00 00 01    flag_const = 1
+06..+09  00 00 00 01    count = 1
+0a..+0d  00 22 04 f8    offset_or_size = 2,229,496 (file offset)
+0e..+11  00 00 00 0b    class_id = 0x0b (CNeoIDList)
+12..+15  49 44 20 20    'ID  ' FOURCC
+16..+19  00 00 00 01    value = 1
+1a..+1f  ff 00 00 00 00 00   pad
```

Perfect match with `the format notes`.

**Page 22,299 (0xD2, kind=0x00)** : repetitive 32-byte records :

```
+00  d2 00 00 00 00 01 00 57 1b 30 00 00 00 09 03 00 00 00 0e da 00 00 00 00 00 00 00 00 00 00 00 00
+20  cc ec 00 00 00 01 00 57 1b 50 00 00 00 09 03 00 00 00 0e d9 00 00 00 00 00 00 00 00 00 00 00 00
+40  d6 36 00 00 00 01 00 57 1b 70 00 00 00 09 03 00 00 00 0e d8 00 00 00 00 00 00 00 00 00 00 00 00
```

Inferred layout (to be validated) :
```
+0   key (BE u32)
+4   constant  (flag)
+8   target_offset (BE u32, file offset)
+12  class_id (BE u32) = 0x09 (= ?)
+14  type byte
+16  value (BE u32)
+20  padding (12 bytes of 0x00)
```

**0xC2 pages (e.g. page 28)** : pattern less clear, possibly
`(key, child_page_id)` pairs of 6-8 bytes each.

## Coverage gain from G3

If the layout is confirmed :
- 989 0xC2 pages × 250B useful = **247,250 decodable bytes**
- 187 0xD2 pages × 250B useful = **46,750 decodable bytes**
- **Total G3 potential : ~294,000 bytes** (4.84% of the HDB)

Combined with G4 (176B) : G3+G4 = ~294K additional byte-direct bytes.

## 0xC0 (freed) pages — separate analysis

754 tag 0xC0 pages — likely pages freed by deleted records. Opaque content
(residue of the old content). Probably to be marked "freed, content
not authoritative" and zero-filled at round-trip if regenerated.

## Next steps

1. **Identify the B-tree navigator** — from the on-disk structure,
   identify the +N offsets read to follow the root → leaf chain.
2. **Header `root_or_size`** — decode as a file offset or page_id
   - As file offset : page 20,998 byte +136 → first_byte 0x73 (= 's')
   - Not a valid page tag. So root_or_size is not a page offset.
   - Still to understand : total size ? root record pointer ?
3. **Page access order** : observe how the navigation traverses
   the 0xC2/0xC3/0xD2 pages.

## Immediate implication for what follows

The `hdb_extract/` project can start C1-C2 (container + serializer) without
G3 resolved, BUT not C4 (b-tree walker). The walker must wait for G3 complete.

In the meantime, we can linearly walk the 202 0xC3 leaf pages (best-effort)
and extract what we understand by grammar match.

## Byte-direct audit (state)

- Before G3 : 25,740 / 6,074,064 = 0.42%
- After G3 (if completed) : 25,740 + 294,000 = 319,740 / 6,074,064 ≈ **5.27%**

After that, G1 + G2 remain (the bulk — ~1.9 MB of undecoded records).
