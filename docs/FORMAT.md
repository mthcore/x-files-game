# HDB on-disk format

Authoritative reference for the byte-direct layout of `XFILES.HDB`.

Every claim in this file is either:
- **Derived from a public NeoPersist 3.0 header** (cited inline with file/line).
- **Empirically validated** against `XFILES.HDB` (tests in `python/tests/`).
- **Documented as a structural-only zone** (round-trip-identical but not
  semantically decoded — see §6 B-tree internal pages).

## 1. File overall layout

```
+----------------------------------------------------------+
| File header           32 bytes  (BE u32 fields)          |
+----------------------------------------------------------+
| 224 reserved bytes    (kNeoDatabaseHeadSpace = 256, of   |
|                        which the first 32 carry meaning) |
+----------------------------------------------------------+
| Page 0                256 bytes                          |
| Page 1                256 bytes                          |
| ...                                                       |
| Page N-1              256 bytes                          |
+----------------------------------------------------------+
| Trailer               176 bytes  (zero padding for       |
|                        XFILES.HDB; kNeoDatabaseQuantum=8 |
|                        alignment)                        |
+----------------------------------------------------------+
```

For `XFILES.HDB`: `N = 23 726`, total file size = `32 + 23 726 × 256 + 176
= 6 074 064` bytes.

Implementation note: the Python decoder treats `HEADER_SIZE = 32`. The
24-byte gap to `kNeoDatabaseHeadSpace = 256` is implicitly counted as
part of "page 0" (which is itself a zero-padded reserved page in
practice).

## 2. File header (32 bytes, big-endian)

| Offset | Size | Field             | Description                       |
|--------|------|-------------------|-----------------------------------|
| 0      | 4    | `version`         | Database format version           |
| 4      | 4    | `root_or_size`    | B-tree root pointer / db size     |
| 8      | 4    | `record_count`    | Approx. record count              |
| 12     | 4    | `class_count`     | Number of classes registered      |
| 16     | 4    | `unk_94982`       | Unknown (constant in this build)  |
| 20     | 4    | `unk_1281`        | Unknown (constant in this build)  |
| 24     | 4    | `max_or_limit`    | Allocator high-water mark         |
| 28     | 4    | `page_size`       | = 256                             |

`inspect` prints these. The two `unk_*` fields don't change between save
states; they're an internal allocator state (not needed for decoding).

## 3. Page types (first byte of each 256-byte page)

| Tag    | Name                 | Count (XFILES.HDB) | Role |
|--------|----------------------|------|-------|
| `0x00` | empty                | 14 286 (60.2%) | Allocated but unused; zero-padded. |
| `0xC2` | btree_internal       | 989 (4.2%)  | B-tree internal node (primary index). |
| `0xC0` | btree_freed          | 754 (3.2%)  | Page recycled after delete; opaque residue. |
| `0xC3` | btree_leaf           | 202 (0.9%)  | B-tree leaf node (CNeoIDList records). |
| `0xD2` | btree_internal_alt   | 187 (0.8%)  | B-tree internal node (alternate / secondary index). |
| other  | data record page     | 7 308 (30.8%) | Arbitrary first byte = data; contains VC* records. |

Empty pages, freed pages, and trailer together account for 59.98% of
the file (zero-padded space reserved for future growth).

## 4. Record marker (8 bytes per record)

Every persistent record begins with an 8-byte marker. The layout matches
`CNeoPersist::readObject` ([CNeoPersist.h:175](../headers/neopersist_3.0/CNeoPersist.h):
`kNeoPersistFileLength = 2 + sizeof(NeoVersion) = 6` bytes, plus 2 bytes
of alignment to the `kNeoDatabaseQuantum = 8`):

```
+0   uint8   flag1       ← packed CNeoPersist bit field A
+1   uint8   flag2       ← packed CNeoPersist bit field B (and B-tree role byte)
+2   uint32  NeoVersion  ← big-endian; default value 
+6   uint16  flag16      ← B-tree level / refcount-like; varies
```

The "magic" value  at offset +2..+5 is **not a magic** — it
is just `kNeoVersionDefault = 1` ([NeoTypes.h:80](../headers/neopersist_3.0/NeoTypes.h)).
Records that override the version (rare in X-Files) will show a
different value here.

Records are aligned to multiples of 8 bytes within a page. Hence the
"marker invariant scanner" (`page[off+2..off+5] == 00 00 00 01`,
`off % 8 == 0`) finds every record start reliably.

## 5. After the marker — field-by-field byte-direct

For each VC* class (51 in X-Files), the body following the marker is
a fixed sequence of typed field reads. **No tags are written between
fields**. The field order matches the class's `Read()` method
(see [WIRE_FORMAT.md](WIRE_FORMAT.md) for the format details
showing that `CNeoFileStream` ignores the FOURCC arguments).

Example: `VCTrigger` (class_id `0x51`, [ClassSpec](../python/src/hdb_extract/classes/generated/all_classes.py)):

```
+0  marker            8 bytes  (CNeoPersist header + alignment)
+8  field 'e' u32 BE  4 bytes  (read_dword(tag='e'))
+12 field 'f' u8      1 byte   (read_byte(tag='f'))
+13 padding           3 bytes  (alignment to next record)
```

The 51 grammars live in
[`python/src/hdb_extract/classes/generated/all_classes.py`](../python/src/hdb_extract/classes/generated/all_classes.py).

## 6. B-tree internal pages (G6 — structural only)

Pages with tag `0xC2` (989 pages), `0xD2` (187 pages), and to some extent
`0xC3` headers, contain B-tree internal node data. They have:

- **6-byte page header**: `tag | kind | 3 reserved | flag`.
- **Variable-length records** in the body, marked by the same 8-byte
  invariant `[0xCX][sub_tag][00 00 00 01][flag16]`.

`0xC2` shows **37 distinct `kind` bytes**; `0xD2` shows **18**. The exact
semantic layout per `(tag, kind)` combination cannot be derived from the
public NeoPersist 3.0 headers, which only define the **interface**
(`CNeoIndexIterator`, `CNeoMetaClass`); the **implementation** that
writes these page layouts (`CNeoNode.cp`, `CNeoIndex.cp`,
`CNeoGenericIndex.cp`) was never distributed publicly in any SDK we've
found.

**This does not break the byte-direct round-trip**: the page bytes are
captured verbatim under `PAGE_INTERNAL` / `PAGE_LEAF_BODY` codes and
re-serialized identically. It does mean the **fine semantic structure**
of these pages (e.g. "this byte is a key", "this byte is a child page
pointer") is not exposed in the JSON extractors.

If you find `CNeoNode.cp` in an archive, please open a PR — it would
close this gap immediately.

See the [B-tree cartography report](../python/out/audit/) and the
internal note [`hdb_btree_internal.md`](https://github.com/mthcore/x-files-game/blob/main/docs/reference/hdb_btree_internal.md)
for the empirical layout per kind.

## 7. Trailer (176 bytes)

The trailer is the final padding that aligns the file size to the
`kNeoDatabaseQuantum = 8` (and historically, allocator superblocks).
For `XFILES.HDB` it is **176 bytes of zero** (`b"\x00" * 176`).

```
6 074 064 = 32 + 23 726 × 256 + 176
                                ^^^
                                trailer
```

The Python `verify` command asserts this is 100% zero.

## 8. Endianness

NeoAccess is **big-endian on disk** (Macintosh origin, kept for binary
compatibility across the Mac→Windows port). On x86 the runtime byte-swaps
via a byte-swap helper (`bswap32`).
Every `long`, `short`, `version`, `class_id`, and pointer field in the HDB
is BE.

The only bytes that are *not* logically swapped are:
- The first byte of each page (page tag) — naturally byte-positioned.
- Flag bytes (single-byte fields).
- The 2-byte `flag16` at marker offset +6 (which is BE u16).

## 9. References

- Source primitives: [`../headers/neopersist_3.0/CNeoStream.cp`](../headers/neopersist_3.0/CNeoStream.cp)
- File header layout: [`../headers/neopersist_3.0/CNeoDatabase.h`](../headers/neopersist_3.0/CNeoDatabase.h)
- On-disk record size: [`../headers/neopersist_3.0/CNeoPersist.h`](../headers/neopersist_3.0/CNeoPersist.h)
- Wire format proof: [WIRE_FORMAT.md](WIRE_FORMAT.md)
- Class catalog: [CLASSES.md](CLASSES.md)
- Coverage certificate: [`../examples/coverage_final.md`](../examples/coverage_final.md)
