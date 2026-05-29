# The 51 VC* classes

Catalog of every HyperBole content class in `XFILES.HDB`. Class IDs run
from `0x27` to `0x5C` with two gaps (`0x2F` and `0x40`, `0x4A` — possibly
classes deprecated mid-development).

## Read this table

- **class_id**: byte-direct identifier in the HDB stream (BE u32 when
  written by polymorphic readers).
- **Name**: the class name as it appears in the on-disk FOURCC tags and in
  this project's Python `ClassSpec`.
- **Size**: in-memory size (bytes). Does **not** equal on-disk size —
  on disk, classes are variable-length.
- **Parent**: the C++ parent class. All inherit from `VCObject`, which
  in turn extends `CNeoPersist`.
- **Ops**: the byte-direct field read sequence (after the 8-byte marker).
  See [WIRE_FORMAT.md](WIRE_FORMAT.md) for what each op means.

## Op types

| Op           | Bytes read    | Semantics |
|--------------|---------------|-----------|
| `BaseInit`   | 8 (marker)    | Implicit; always first. CNeoPersist header + alignment. |
| `Byte(tag)`  | 1             | Single byte. |
| `ByteAlt(tag)` | 1            | Single byte via alt-slot. |
| `Short(tag)` | 2 (BE)        | Big-endian u16. |
| `Dword(tag)` | 4 (BE)        | Big-endian u32. |
| `String(tag)`| variable      | Null-terminated string. |
| `BufF(n, fcc)`| n            | Fixed-size raw buffer. |
| `DwordF(fcc)` | 4 (BE)       | u32 read via "FOURCC slot" (semantically equivalent to Dword). |
| `SubObj(name)` | variable    | Polymorphic sub-object: reads class_id, dispatches, recurses. |
| `ObjList(name)`| variable    | List of polymorphic sub-objects. Count is read first. |
| `Versioned(min_version, ops)` | conditional | Apply `ops` only if `NeoVersion ≥ min_version`. |

## Full table

| class_id | Name | Size | Parent | Ops |
|---|---|---|---|---|
| 0x27 | VCTitle | 128B | VCObject | Versioned, SubObj, SubObj, SubObj, SubObj, Dword, Dword, Short, Short |
| 0x28 | VCNode | 112B | VCObject | ObjList, Dword, Dword |
| 0x29 | VCLocaton | 112B | VCObject | ObjList, Dword, Dword, Dword, Byte |
| 0x2A | VCViewPoint | 96B | VCObject | ObjList, SubObj, Dword, Dword |
| 0x2B | VCView | 96B | VCObject | Dword × 6 |
| 0x2C | VCCharacter | 48B | VCObject | Dword |
| 0x2D | VCCharView | 96B | VCObject | Dword, SubObj, Dword × 4 |
| 0x2E | VCCharViewList | 68B | VCObject | ObjList |
| 0x30 | VCName | 64B | VCObject | SubObj × 2 |
| 0x31 | VCConversation | 128B | VCObject | Dword × 4, Versioned × 2 |
| 0x32 | VCConversationList | 68B | VCObject | ObjList |
| 0x33 | VCNav | 56B | VCObject | Dword × 3 |
| 0x34 | VCNav_List | 68B | VCObject | ObjList |
| 0x35 | VCAssetRef | 64B | VCObject | SubObj, Dword × 2, Byte × 2 |
| 0x36 | VCAssetRefList | 68B | VCObject | ObjList |
| 0x37 | VCExplorable | 64B | VCObject | SubObj, Dword |
| 0x38 | VCExplorableList | 68B | VCObject | ObjList |
| 0x39 | VCHotSpot | 64B | VCObject | SubObj, Dword |
| 0x3A | VCHotSpotList | 68B | VCObject | ObjList |
| 0x3B | VCStdHotSpotList | 68B | VCObject | ObjList |
| 0x3C | VCAssetCategory | 48B | VCObject | Dword, Byte |
| 0x3D | VCCursor | 64B | VCObject | SubObj, Dword × 2 |
| 0x3E | VCDefaultCursors | 64B | VCObject | ObjList |
| 0x3F | VCDefaultHotSpots | 64B | VCObject | ObjList |
| 0x41 | VCAction | 64B | VCObject | SubObj, Byte |
| 0x42 | VCActionList | 68B | VCObject | ObjList |
| 0x43 | VCBlob | 48B | VCObject | SubObj |
| 0x44 | VCPlayer | 48B | VCObject | Dword |
| 0x45 | VCEnabled | 48B | VCObject | Dword, Byte × 2 |
| 0x46 | VCEmotionIcon | 68B | VCObject | Dword × 6, Versioned |
| 0x47 | VCActionIcon | 68B | VCObject | Dword × 6, Versioned |
| 0x48 | VCIdeaIcon | 68B | VCObject | Dword × 6, Versioned |
| 0x49 | VCEvidenceIcon | 68B | VCObject | Dword × 6, Versioned |
| 0x4B | VCInventory | 68B | VCObject | Dword × 6, Versioned |
| 0x4C | VCIdeaMap | 64B | VCObject | Dword × 2 |
| 0x4D | VCIdeaMapList | 68B | VCObject | ObjList |
| 0x4E | VCIFaceLayout | 64B | VCObject | ObjList, Dword, Byte |
| 0x4F | VCInterfaceItem | 64B | VCObject | SubObj, Dword × 2, Short, Versioned |
| 0x50 | VCString | 48B | VCObject | SubObj |
| 0x51 | VCTrigger | 64B | VCObject | Dword, Byte |
| 0x52 | VCTriggerList | 68B | VCObject | ObjList |
| 0x53 | VCVariable | 64B | VCObject | SubObj, Dword × 2, Byte × 2 |
| 0x54 | VCStdAction | 64B | VCObject | Dword × 3 |
| 0x55 | VCStdActionList | 68B | VCObject | ObjList |
| 0x56 | VCConversationHistory | 128B | VCObject | SubObj |
| 0x57 | VCGameState | 428B | VCObject | 16+ Dwords, sub-objs, bytes |
| 0x58 | VCPDANotes | 128B | VCObject | SubObj |
| 0x59 | VCPhoto | 120B | VCObject | SubObj × 2, Dword × 2, Byte × 2 |
| 0x5A | VCEmail | 120B | VCObject | SubObj |
| 0x5B | VCEmailRead | 120B | VCObject | SubObj |
| 0x5C | VCEmailPending | 120B | VCObject | SubObj |

## Class families

- **Scene graph**: `VCTitle`, `VCNode`, `VCLocaton`, `VCViewPoint`,
  `VCView`. Build the spatial hierarchy of in-game locations.
- **Characters**: `VCCharacter`, `VCCharView`, `VCCharViewList`,
  `VCName`. Mulder, Scully, NPCs.
- **Conversation system**: `VCConversation`, `VCConversationList`,
  `VCConversationHistory`. Branching dialogues. See [DIALOGUES.md](DIALOGUES.md).
- **Navigation**: `VCNav`, `VCNav_List`. Pre-rendered camera movements
  linking locations.
- **Assets**: `VCAssetRef`, `VCAssetRefList`, `VCAssetCategory`.
  References to external media (`.PFF` resources).
- **Explorables & HotSpots**: `VCExplorable`, `VCHotSpot`, `VCHotSpotList`,
  `VCStdHotSpotList`. UI click zones. See [HOTSPOTS.md](HOTSPOTS.md).
- **UI**: `VCCursor`, `VCDefaultCursors`, `VCDefaultHotSpots`,
  `VCIFaceLayout`, `VCInterfaceItem`. Interface widgets.
- **Actions / Triggers / Stdactions**: `VCAction`, `VCActionList`,
  `VCTrigger`, `VCTriggerList`, `VCStdAction`, `VCStdActionList`.
  Game logic. See [TRIGGERS.md](TRIGGERS.md).
- **Icons (PDA)**: `VCEmotionIcon`, `VCActionIcon`, `VCIdeaIcon`,
  `VCEvidenceIcon`, `VCInventory`. PDA inventory widgets.
- **Decision graph**: `VCIdeaMap`, `VCIdeaMapList`. See [DECISIONS.md](DECISIONS.md).
- **Game state**: `VCGameState`, `VCVariable`. Save state, runtime
  variables, scoring.
- **PDA content**: `VCPDANotes`, `VCPhoto`, `VCEmail`, `VCEmailRead`,
  `VCEmailPending`. In-game PDA documents.
- **Miscellaneous**: `VCString`, `VCBlob`, `VCPlayer`, `VCEnabled`.

## How to extend

To add a new class spec (or fix an existing one):

1. Find the class's `Read()` method in the format notes. Look for the
   slot-12 reader for that class.
2. Translate each `stream->slotXX('y')` call into a `ClassSpec` op:
   - slot `0x80` → `Byte(tag, name)`
   - slot `0xb0` → `Short(tag, name)`
   - slot `0x9c` → `Dword(tag, name)`
   - slot `0x14` → `String(tag, name)`
   - slot `0x138` → `SubObj(name)`
   - slot `0xbc` → `DwordF(fourcc)`
3. Append to
   [`python/src/hdb_extract/classes/generated/all_classes.py`](../python/src/hdb_extract/classes/generated/all_classes.py).
4. Run `pytest tests/unit/test_classes_apply_read.py` to validate.

See `VCTrigger` for a worked example (corrected from NOP in v0.1):

```python
ClassSpec(
    name="VCTrigger", class_id=0x51, size_bytes=0x40, parent="VCObject",
    ops=(
        BaseInit(),
        Dword(0x65, "e"),   # slot 0x9c read_dword('e') -> this+0xE4
        Byte(0x66, "f"),    # slot 0x80 read_byte('f') -> this+0xE8
    ),
    notes="Read body byte-direct.",
),
```
