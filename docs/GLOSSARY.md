# Glossary

Quick reference for the terms used throughout this project.

| Term | Meaning |
|------|---------|
| **HDB** | Hyperbole DataBase. The `.hdb` file holding all game data (`XFILES.HDB` for our subject). |
| **VirtualCinema** | HyperBole Studios' authoring engine for FMV adventure games. Runs the HDB at game time. |
| **NeoPersist** | NeoLogic Systems' C++ object persistence library. Versions 2.0, 2.1, 3.0 known. X-Files uses 3.0. |
| **NeoAccess** | NeoLogic's broader product line (same codebase as NeoPersist). |
| **Hammer** | NeoLogic's internal codename. Sometimes appears in SDK comments. |
| **CNeoPersist** | Base C++ class for any persistent object. On-disk size = **6 bytes** (2B flags + 4B BE version). |
| **CNeoStream** | Abstract serialization stream. Subclasses: `CNeoFileStream` (binary on-disk), `CNeoDebugStream` (human dump). |
| **CNeoFileStream** | The on-disk wire format used by HDB. **Does not write FOURCC tags** — pure byte-direct. |
| **NeoTag** | A FOURCC (4-byte int) passed to `CNeoStream::read*`/`write*` as a *documentary* parameter. Not written to disk by `CNeoFileStream`. |
| **NeoVersion** | A 32-bit version number written after the flags. `kNeoVersionDefault = 1`. |
| **NeoMark** | An absolute byte offset into the database file. Used for cross-references. |
| **NeoID** | A 32-bit unique ID for a persistent object within its class. |
| **VC*** | HyperBole's content classes (e.g. `VCTrigger`, `VCConversation`). 51 classes in X-Files, class_id range `0x27..0x5C`. |
| **VCObject** | HyperBole's direct subclass of CNeoPersist; the in-engine analogue. |
| **FOURCC** | "Four-character code" — 32-bit identifier readable as ASCII (e.g. `'NPfl' = 0x4E50666C`). |
| **TLV** | Tag-Length-Value. A traditional self-describing record format. The HDB **does not** use TLV on disk despite what older RE notes suggest — fields are written byte-direct. |
| **B-tree** | Self-balancing search tree used by NeoAccess to index records by ID. The HDB contains 989 internal pages (`0xC2`), 187 internal-alt pages (`0xD2`), 202 leaf pages (`0xC3`), and 754 freed pages (`0xC0`). |
| **Page** | A 256-byte aligned unit on disk. First byte identifies page type (tag). |
| **Page tag** | First byte of a page: `0x00` empty, `0xC0` freed, `0xC2` internal, `0xC3` leaf, `0xD2` internal-alt, anything else = data record page. |
| **Marker** | 8-byte structure at the start of each persistent record: `[flag1 1B][flag2 1B][NeoVersion 4B BE][flag16 2B]`. Equivalent to the 6-byte `CNeoPersist` header + 2B alignment to the 8-byte `kNeoDatabaseQuantum`. |
| **`kNeoDatabaseQuantum`** | 8. All file-space allocations are rounded to this quantum. Explains the marker alignment. |
| **`kNeoDatabaseHeadSpace`** | 256. File header occupies the first 256 bytes, but only the first 32 are signifiant; the rest is reserved padding. The Python decoder uses 32 directly. |
| **`kNeoPersistFileLength`** | 6 = `2 + sizeof(NeoVersion)`. The on-disk size of a bare `CNeoPersist` record. |
| **kind byte** | Second byte of a B-tree page. Identifies the sub-type within the page tag. Has 37 distinct values for `0xC2`, 18 for `0xD2`. Semantics depend on sources not publicly available (see [docs/FORMAT.md](FORMAT.md) §6). |
| **ClassSpec** | Python dataclass describing how to byte-direct read a VC* class: ordered sequence of `BaseInit`, `Dword`, `Byte`, `Short`, `String`, `SubObj`, `ObjList`, `Versioned` operations. |
| **`apply_read`** | The reader driver: takes a `HDBSerializer` and a `ClassSpec`, returns a populated `dict`. |
| **`HDBSerializer`** | Python wrapper around the byte cursor that records every read for audit purposes. |
| **`HdbSerializer`** | The original C++ serializer, reconstructed from the on-disk format. 119 typed read slots, includes `read_byte` (slot 0x80), `read_short` (0xb0), `read_dword` (0x9c), `read_buf_fourcc` (0x74), `read_dword_fourcc` (0xbc), `read_obj` (0x138). |
| **Trigger** | Game-logic element. A trigger has `Conditions[]` (up to 8), `Actions[]` (up to 12), `Targets[]` (up to 4). 61 instances in X-Files. |
| **Hotspot** | A clickable region on screen (rectangle + action references). 2 278 instances in X-Files. |
| **AGRIPPA** | A 2024-2025 community web-port (`xesf/agrippa`) of X-Files. Bypassed the HDB; ~15% playable. The gap this project fills. |
| **`.PFF`** | HyperBole's *pack file* format — bundles non-database assets (video, audio). Unrelated to the HDB. |
| **`.XT*`** | XTLanguage scripted cinematics. A separate, very simple VM. Decoded by the optional `xt_dump` extractor in this project but not the focus. |
| **DRM** | None. The HDB ships without any encryption, signing, or anti-tamper. No measures circumvented. |
