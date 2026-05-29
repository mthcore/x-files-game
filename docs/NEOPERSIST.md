# NeoPersist 3.0 — deep dive

This is a focused walk through the NeoPersist 3.0 architecture, derived
from the public headers under [`../headers/neopersist_3.0/`](../headers/neopersist_3.0/).
The intent is to give a new contributor enough to grok the library in
30 minutes.

## What NeoPersist solves

In 1992, persisting a C++ object graph to disk and getting it back
intact was hard. Standard approaches required hand-written
serializers, brittle versioning, manual pointer fix-ups, and either
a separate index file or O(n) lookup.

NeoPersist offered:
- A base class `CNeoPersist` that any persistable object inherits from.
- Automatic save/load via virtual `readObject` / `writeObject` methods.
- B-tree indexed lookup by `NeoID` (class-scoped unique ID).
- An object cache with refcount-based purging.
- Schema migration via per-object version numbers.

A user-facing C++ class became persistent simply by:

```cpp
class MyObject : public CNeoPersist {
    int payload_;
public:
    void readObject(CNeoStream* s, NeoTag tag) override {
        CNeoPersist::readObject(s, tag);   // flags + version
        payload_ = s->readLong('plod');     // field
    }
    void writeObject(CNeoStream* s, NeoTag tag) override {
        CNeoPersist::writeObject(s, tag);
        s->writeLong(payload_, 'plod');
    }
};
```

That `'plod'` tag is *only* used by the optional debug stream
(`CNeoDebugStream`) for human-readable dumps. The production
`CNeoFileStream` writes nothing for it.

## Core class hierarchy

```
CNeoPersist  (abstract base — 6 bytes on disk)
├── CNeoBlob          (raw binary blob)
├── CNeoClass         (metaclass — registry entry)
├── CNeoSubclass      (subclass record)
├── CNeoFreeList      (free space manager)
├── CNeoInode         (file allocation node)
├── CNeoIDIndex       (B-tree index by NeoID)
├── CNeoParentIndex   (B-tree index by parent NeoID)
├── CNeoGenericIndex  (B-tree generic index)
├── CNeoPartMgr       (part manager — composite container)
├── CNeoIDList        (list of NeoIDs — used as a small reference table)
└── (user subclasses — for X-Files: 51 VC* classes)
```

The system class_ids 0..11 are reserved ([NeoTypes.h:289-300](../headers/neopersist_3.0/NeoTypes.h)).
User classes start at higher class_ids. In X-Files they occupy the range
**0x27..0x5C**.

## `CNeoPersist` on-disk layout

`kNeoPersistFileLength = 2 + sizeof(NeoVersion) = 6` bytes:

```
+0   uint8   flags1   (fLeaf | fRoot | fTemporary | fLocal | fShareFill | fBusy | fBool1)
+1   uint8   flags2   (fBool2 | fBool3 | fReserved | fPeerDirty | fDirty)
+2   uint32  fVersion (big-endian, kNeoVersionDefault = 1)
```

Plus 2 bytes of alignment to `kNeoDatabaseQuantum = 8` → **8-byte
marker** observed in every record.

A non-trivial detail: the bit-field layout was not portably specified in
1992, so the **actual bit assignment** within `flags1` / `flags2` depends
on the compiler used to build the writing program. For HyperBole's
Windows build of X-Files, we empirically observe combinations like:

| flags1 | Interpretation                            |
|--------|-------------------------------------------|
| `0x30` | typical interior page byte (kind ASCII '0') |
| `0x31` | typical leaf page byte (kind ASCII '1')    |
| `0x32` | typical root page byte (kind ASCII '2')    |
| `0x70`, `0x71` | rarer variants                    |

But these correlations don't always hold (see [FORMAT.md §6](FORMAT.md)),
so we don't claim a definitive bit-to-field mapping in this release.

## `CNeoStream` API contract

The abstract `CNeoStream` ([CNeoStream.h](../headers/neopersist_3.0/CNeoStream.h))
defines the read/write primitives every persistent object can call.
**Crucially, every primitive takes a `NeoTag aTag` parameter that the base
implementation marks as unused** ([CNeoStream.cp:184](../headers/neopersist_3.0/CNeoStream.cp)):

```cpp
void CNeoStream::readBits(void* aBits, short aCount, NeoTag aTag) {
    readChunk(aBits, aCount, aTag);   // <-- delegates
}
long CNeoStream::readLong(NeoTag aTag) {
    long value = 0;
    readChunk(&value, sizeof(long), aTag);   // <-- aTag goes nowhere
    return value;
}
```

The concrete `CNeoFileStream` (binary on disk) does **not** write tags.
The concrete `CNeoDebugStream` (text dump) **does** — for human reading.
Both share the same `readObject` / `writeObject` calls in the user code.

This explains the long-standing confusion about whether the HDB format
is "TLV". **It is not.** Tags exist only at the API level; the wire
format is byte-direct.

## B-tree indices

Three index classes provide B-tree lookup:

- `CNeoIDIndex` (class_id 7): indexed by NeoID within a class.
- `CNeoParentIndex` (class_id 8): indexed by parent NeoID (for trees).
- `CNeoGenericIndex` (class_id 9): generic ordered index.

The B-tree is split across:
- **Internal pages** (tag `0xC2` primary, `0xD2` secondary).
- **Leaf pages** (tag `0xC3`), containing the actual `CNeoIDList` entries.

A single 256-byte page holds a number of variable-size records
constrained by `CNeoFormat::fMaxClassEntries`
([CNeoFormat.h](../headers/neopersist_3.0/CNeoFormat.h) — from the
publicly available NeoPersist 3.0 headers, not redistributed here as it's
the same content as the `neopersist_3.0/` directory).

Internal node navigation (deciding which child to descend into for a
given key) is handled by methods in `CNeoNode.cp` / `CNeoIndex.cp` —
files **not present** in any public NeoLogic SDK we've located. This is
the only zone of the HDB format that the project does not decode
semantically; structurally, the bytes are still 100% round-trippable.

## CNeoIDList — the small reference table

`CNeoIDList` (class_id 11 = `0x0B`, [NeoTypes.h:300](../headers/neopersist_3.0/NeoTypes.h))
is a thin wrapper around a sorted array of NeoID values. Used pervasively
in the HDB to store cross-references (e.g. "this scene has these N
hotspots").

On disk, a CNeoIDList record we've decoded (page 91, offset 0):

```
+0  d2 70 00 00 00 01 00 00     ← marker (8B)
+8  00 01                        ← count BE u16 = 1
+10 00 22 04 f8                  ← target offset/data BE u32 (a NeoMark)
+14 00 00 00 0b                  ← class_id BE u32 = 11
+18 49 44 20 20                  ← FOURCC 'ID  '   (this one IS on disk —
                                    it's the FOURCC NAME of the index, not
                                    the tag-argument to readChunk)
+22 00 00                        ← padding
+24 00 01                        ← value BE u16
+26 ff 00                        ← flag
+28 00 00 00 00                  ← zero padding
```

Total: 32 bytes (8 marker + 24 body). The `'ID  '` FOURCC is the **only
FOURCC actually written on disk** in our observations — it identifies the
index this list belongs to (the primary `IDIndex`).

## Why this matters

NeoPersist was an obscure-but-clever piece of mid-1990s engineering. Its
on-disk format is:

- Compact (no schema overhead, no field tags).
- Self-versioning (per-object `NeoVersion` field).
- Indexable (B-tree built-in).
- Portable across Mac/Windows (big-endian on disk; runtime byte-swap).

Decoding it from the outside required (a) the public headers, and (b)
careful analysis of one shipping title. Both are now available, and
this project encapsulates the result.

## Further reading

- [WIRE_FORMAT.md](WIRE_FORMAT.md) — why the wire is byte-direct without tags.
- [FORMAT.md](FORMAT.md) — exact on-disk byte layout.
- [CLASSES.md](CLASSES.md) — catalog of the 51 VC* classes.
- [`../headers/neopersist_3.0/`](../headers/neopersist_3.0/) — verbatim
  NeoPersist 3.0 headers as ground truth.
