# W23.4 — HDB TLV per-class grammar byte-direct

Extracted from each save class's `Read` method body. Two extraction patterns :
- **Direct vtable read** : `(*pcVar)(0xTAG)` followed by store to `this+offset`
- **Sub-object read** : `(this+offset, ctx, tag, this)`

## Coverage summary

| Class | Class ID | Size | Resolution | Direct fields | Sub-obj fields | Base init | Array iter |
|-------|----------|------|------------|---------------|----------------|-----------|------------|
| VCEmail | 0x5a | 0x78 | (direct) | 0 | 1 | True | True |
| VCEmailRead | 0x5b | 0x78 | (direct) | 0 | 1 | True | True |
| VCEmailPending | 0x5c | 0x78 | (direct) | 0 | 1 | True | True |
| VCConversation | 0x31 | 0x80 | (direct) | 6 | 0 | True | False |
| VCConversationHistory | 0x56 | 0x80 | (direct) | 0 | 1 | True | True |
| VCEvidenceIcon | 0x49 | 0x44 | (resolved) | 7 | 0 | True | False |
| VCInventory | 0x4b | 0x44 | (resolved) | 7 | 0 | True | False |
| VCPDANotes | 0x58 | 0x80 | (direct) | 0 | 1 | True | True |
| VCPhoto | 0x59 | 0x78 | (direct) | 2 | 1 | True | False |

---

## VCEmail (class_id 0x5a, size 0x78 = 120B)


**Base init** : yes (NPfl + vers)
**Array iter** : yes (array iterator)

**TLV grammar (sub-object reads)** :

| # | addr expr | this+offset | tag byte | tag char |
|---|-----------|-------------|----------|----------|
| 0 | `iVar1` | +0x0 | 0x66 | 'f' |

## VCEmailRead (class_id 0x5b, size 0x78 = 120B)


**Base init** : yes (NPfl + vers)
**Array iter** : yes (array iterator)

**TLV grammar (sub-object reads)** :

| # | addr expr | this+offset | tag byte | tag char |
|---|-----------|-------------|----------|----------|
| 0 | `iVar1` | +0x0 | 0x66 | 'f' |

## VCEmailPending (class_id 0x5c, size 0x78 = 120B)


**Base init** : yes (NPfl + vers)
**Array iter** : yes (array iterator)

**TLV grammar (sub-object reads)** :

| # | addr expr | this+offset | tag byte | tag char |
|---|-----------|-------------|----------|----------|
| 0 | `iVar1` | +0x0 | 0x66 | 'f' |

## VCConversation (class_id 0x31, size 0x80 = 128B)


**Base init** : yes (NPfl + vers)
**Array iter** : no

**Slot var assignments** :
- `pcVar2` = serializer.vtable[+0x9c] (`read_dword` -> `uint32`)

**TLV grammar (direct vtable reads)** :

| # | this+offset | tag byte | tag char | op | type |
|---|-------------|----------|----------|----|----- |
| 0 | +0x28 | 0x65 | 'e' | read_dword | uint32 |
| 1 | +0x2c | 0x66 | 'f' | read_dword | uint32 |
| 2 | +0x34 | 0x68 | 'h' | read_dword | uint32 |
| 3 | +0x3c | 0x6a | 'j' | read_dword | uint32 |
| 4 | +0x30 | 0x67 | 'g' | read_dword | uint32 |
| 5 | +0x38 | 0x69 | 'i' | read_dword | uint32 |

## VCConversationHistory (class_id 0x56, size 0x80 = 128B)


**Base init** : yes (NPfl + vers)
**Array iter** : yes (array iterator)

**TLV grammar (sub-object reads)** :

| # | addr expr | this+offset | tag byte | tag char |
|---|-----------|-------------|----------|----------|
| 0 | `iVar1` | +0x0 | 0x66 | 'f' |

## VCEvidenceIcon (class_id 0x49, size 0x44 = 68B)


**Base init** : yes (NPfl + vers)
**Array iter** : no

**Slot var assignments** :
- `pcVar2` = serializer.vtable[+0x9c] (`read_dword` -> `uint32`)

**TLV grammar (direct vtable reads)** :

| # | this+offset | tag byte | tag char | op | type |
|---|-------------|----------|----------|----|----- |
| 0 | +0x28 | 0x65 | 'e' | read_dword | uint32 |
| 1 | +0x2c | 0x66 | 'f' | read_dword | uint32 |
| 2 | +0x30 | 0x67 | 'g' | read_dword | uint32 |
| 3 | +0x34 | 0x68 | 'h' | read_dword | uint32 |
| 4 | +0x3c | 0x69 | 'i' | read_dword | uint32 |
| 5 | +0x40 | 0x6a | 'j' | read_dword | uint32 |
| 6 | +0x38 | 0x6b | 'k' | read_dword | uint32 |

## VCInventory (class_id 0x4b, size 0x44 = 68B)


**Base init** : yes (NPfl + vers)
**Array iter** : no

**Slot var assignments** :
- `pcVar2` = serializer.vtable[+0x9c] (`read_dword` -> `uint32`)

**TLV grammar (direct vtable reads)** :

| # | this+offset | tag byte | tag char | op | type |
|---|-------------|----------|----------|----|----- |
| 0 | +0x28 | 0x65 | 'e' | read_dword | uint32 |
| 1 | +0x2c | 0x66 | 'f' | read_dword | uint32 |
| 2 | +0x30 | 0x67 | 'g' | read_dword | uint32 |
| 3 | +0x34 | 0x68 | 'h' | read_dword | uint32 |
| 4 | +0x3c | 0x69 | 'i' | read_dword | uint32 |
| 5 | +0x40 | 0x6a | 'j' | read_dword | uint32 |
| 6 | +0x38 | 0x6b | 'k' | read_dword | uint32 |

## VCPDANotes (class_id 0x58, size 0x80 = 128B)


**Base init** : yes (NPfl + vers)
**Array iter** : yes (array iterator)

**TLV grammar (sub-object reads)** :

| # | addr expr | this+offset | tag byte | tag char |
|---|-----------|-------------|----------|----------|
| 0 | `iVar1` | +0x0 | 0x66 | 'f' |

## VCPhoto (class_id 0x59, size 0x78 = 120B)


**Base init** : yes (NPfl + vers)
**Array iter** : no

**TLV grammar (direct vtable reads)** :

| # | this+offset | tag byte | tag char | op | type |
|---|-------------|----------|----------|----|----- |
| 0 | +0x94 | 0x69 | 'i' | ? | ? |
| 1 | +0x95 | 0x6a | 'j' | ? | ? |

**TLV grammar (sub-object reads)** :

| # | addr expr | this+offset | tag byte | tag char |
|---|-----------|-------------|----------|----------|
| 0 | `in_ECX + 0x28` | +0x28 | 0x65 | 'e' |

---

## Notes byte-direct

- All classes use a **uniform structure** : base-init followed by per-field reads.
- The base init reads :
  - `NPfl` (0x4e50666c) flags byte at offset +0x14
  - `vers` (0x76657273) version dword at offset +0x18
- The per-field reader `(addr, ctx, tag, this)` reads ONE TLV field :
  - `addr` = where to store the read value (= this + field_offset)
  - `ctx` = serializer context (HDB reader stream)
  - `tag` = single byte tag for this field
  - `this` = optional, the containing object (used for vtable dispatch)

## Next step
- W23.4 step 2 : runtime corroboration via 6-hook session (catch FOURCC values + types byte-direct)
- W23.4 final : compare static extraction vs runtime capture, resolve any discrepancies