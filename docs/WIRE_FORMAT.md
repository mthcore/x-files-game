# Wire format — why there are no tags on disk

This document settles a question that confuses every new contributor:

> "I see tags like `'NPfl'`, `'vers'`, `'NPTc'` mentioned in the
> format notes — why don't they appear in the HDB file?"

**Short answer**: `CNeoFileStream`, the on-disk subclass of `CNeoStream`,
does not write them. They are API-level identifiers that the base
implementation marks as unused (`NeoUsed(aTag)`).

## Long answer — the proof

### Step 1 — The CNeoStream base implementations

[CNeoStream.cp:184-209](../headers/neopersist_3.0/CNeoStream.cp):

```cpp
void CNeoStream::readBits(void* aBits, short aCount, NeoTag aTag) {
    readChunk(aBits, aCount, aTag);     // delegates raw
}
char CNeoStream::readChar(NeoTag aTag) {
    char value;
    readChunk(&value, sizeof(value), aTag);
    return value;
}
long CNeoStream::readLong(NeoTag aTag) {
    long value = 0;
    readChunk(&value, sizeof(long), aTag);
    return value;
}
short CNeoStream::readShort(NeoTag aTag) {
    short value = 0;
    readChunk(&value, sizeof(short), aTag);
    return value;
}
```

Every primitive `read*(NeoTag)` calls `readChunk(buf, size, tag)`. The
base implementation (and the production `CNeoFileStream` subclass)
**does not use the tag in `readChunk`**. The tag exists for two reasons:

1. **`CNeoDebugStream`** — a different subclass — uses it to label
   fields in human-readable dumps.
2. **Documentation** — it makes fields traceable when reading the
   format notes alongside the source code.

### Step 2 — The object dispatcher

The dispatcher `CNeoPersist::readObject` decodes an object as follows
(the slot-12 dispatcher) :

```c
unsigned int __thiscall readObject(int this, _DWORD* a2, int a3) {
    int v4 = a2[3];     // stream->fFormat
    int v5 = *a2;       // stream->vtable

    // openList(this, 'null')
    (*(void (__thiscall **)(...))(*a2 + 168))(a2, this, 1853189228);
    //                                              ^^^^^^^^^^^^
    // 0x6E756C6C = 'null' in ASCII BE  (== kNeoNoTag)

    // readBits(this->flags, 2 bytes, 'NPfl')
    (*(void (__thiscall **)(...))(v5 + 116))(a2, this + 8, 2, 1313891948);
    //                                                          ^^^^^^^^^^
    // 0x4E50666C = 'NPfl' BE

    if (*(_BYTE *)(v4 + 15)) {   // stream->fFormat.fUseVersions
        // readLong('vers')
        *(_DWORD *)(this + 24) = (*(int (__thiscall **)(...))(v5 + 188))(a2, 1986359923);
        //                                                                       ^^^^^^^^^^
        // 0x76657273 = 'vers' BE
    }
    // ... in-memory bit-flag cleanup ...
}
```

This function **writes 6 bytes to disk** — 2 for the flags, 4 for the
version — *exactly* what `kNeoPersistFileLength` predicts. The
FOURCC arguments (`'null'`, `'NPfl'`, `'vers'`) are passed to the vtable
slots, which ultimately call `readChunk` with the buffer pointer and
the size. **The tag is never written**.

### Step 3 — Empirical scan

If FOURCCs were on disk, they'd appear as 4-byte ASCII sequences in
`XFILES.HDB`. A direct scan over the 6 074 064 bytes:

| FOURCC | Hex        | Occurrences in HDB |
|--------|------------|--------------------|
| `'NPfl'` | `0x4E50666C` | **0** |
| `'NPTc'` | `0x4E505463` | **0** |
| `'vers'` | `0x76657273` | **0** |
| `'NPlt'` | `0x4E506C74` | **0** |
| `'BBmk'` | `0x42424D6B` | **0** |
| `'ID  '` | `0x49442020` | 15 606 (in index records) |

Only `'ID  '` appears — and only because it's part of the CNeoIDList
*body* (it identifies which index the list belongs to). All the FOURCCs
that early notes thought were on the wire are simply absent.

## What this means in practice

When you read a HDB record, you do not need a "tag dispatcher". The
on-disk format is positional. The order in which a class's `Read()`
method calls `readByte` / `readShort` / `readLong` / etc., is the order
the bytes were written, and is the order you must read them in.

For example, `VCConversation` (class_id `0x31`, [per-class grammar](https://github.com/mthcore/x-files-game/blob/main/docs/reference/W23_4_hdb_tlv_per_class.md)):

```
Read() body:
  CNeoPersist::readObject(stream)      // 6B header + alignment = 8B marker
  field_e = stream->readLong('e')      // 4B BE → stored at this+0x28
  field_f = stream->readLong('f')      // 4B BE → stored at this+0x2c
  field_h = stream->readLong('h')      // 4B BE → stored at this+0x34
  field_j = stream->readLong('j')      // 4B BE → stored at this+0x3c
  field_g = stream->readLong('g')      // 4B BE → stored at this+0x30
  field_i = stream->readLong('i')      // 4B BE → stored at this+0x38
```

The `'e'`, `'f'`, `'h'`, etc. tags are stored only in the field names
of our `ClassSpec` for traceability, and in the in-memory offsets, not
on disk. The on-disk format is:

```
[8B marker][4B e][4B f][4B h][4B j][4B g][4B i] = 32 bytes
```

In Python:

```python
ClassSpec(
    name="VCConversation", class_id=0x31, size_bytes=0x80, parent="VCObject",
    ops=(
        BaseInit(),
        Dword(0x65, "e"),   # read_dword tagged 'e' = 0x65 ASCII byte
        Dword(0x66, "f"),
        Dword(0x68, "h"),
        Dword(0x6A, "j"),
        Dword(0x67, "g"),
        Dword(0x69, "i"),
        Versioned(min_version=1, ops=(...)),
    ),
)
```

The numeric tags `0x65`..`0x6A` are **one-byte** tags (ASCII `'e'..'j'`)
passed as immediate arguments to the slot calls
(`(*ctx->vtable[+0x9c])(0x65)`). They make field origins traceable but
they are **not** written by `CNeoFileStream`.

## Implications for porting

If you want to write your own decoder in another language (Rust, Go, JS,
C# — please open a PR!), you do **not** need:

- A FOURCC → byte-tag translation table.
- A TLV scanner.
- A schema file.

You **do** need:

- The marker scanner (`[byte][byte][00 00 00 01][BE u16]` on 8-byte alignment).
- The per-class field-order ClassSpec (we ship 51 in
  [all_classes.py](../python/src/hdb_extract/classes/generated/all_classes.py)).
- The polymorphic reader for sub-objects (read class_id, dispatch).

That's it.

## See also

- [FORMAT.md](FORMAT.md) — overall on-disk layout.
- [NEOPERSIST.md](NEOPERSIST.md) — the library this format comes from.
- [CLASSES.md](CLASSES.md) — the 51 VC* class grammars.
- [`../headers/neopersist_3.0/CNeoStream.cp`](../headers/neopersist_3.0/CNeoStream.cp)
  — the authoritative source for the API contract.
