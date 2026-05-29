# NeoPersist 3.0 — third-party reference content

This directory contains 13 header files and 2 implementation files from
**NeoPersist / NeoAccess 3.0**, authored by NeoLogic Systems Inc. between
1992 and 1996.

## Provenance

These files were extracted **verbatim** (no edits) from two publicly
distributed sources:

1. **CodeWarrior 7 SDK** (Metrowerks, ~1996) — distributed to thousands of
   Mac C++ developers in the late 1990s. Contained the `NeoPersist 3.0` /
   `NeoAccess` headers and `CNeoStream.cp`, `NeoAccess.cp` to allow third-
   party app developers to integrate persistence.
2. **`DoomMacSource.iso`** (id Software, December 1997 release of *Doom*
   Mac source) — included `NeoIncludes/` and matching compiled libraries.

Both sources are publicly accessible and have been redistributed widely
(software preservation archives, Mac developer collections).

## Files

| File                    | Size    | Role                                          |
|-------------------------|---------|-----------------------------------------------|
| `NeoTypes.h`            | 5.9 KB  | Core types: NeoID, NeoMark, NeoTag, class IDs |
| `NeoFailure.h`          | 2.7 KB  | Error codes / exception macros                |
| `NeoAccess.h`           | 0.7 KB  | Public umbrella header                        |
| `NeoAccess.cp`          | 0.2 KB  | Init stub                                     |
| `CNeoPersist.h`         | 8.2 KB  | Persistent object base class                  |
| `CNeoStream.h`          | 5.5 KB  | Abstract stream interface                     |
| `CNeoStream.cp`         | 12.3 KB | Stream primitives (readBits, readLong, ...)   |
| `CNeoDatabase.h`        | 7.3 KB  | Database / file container                     |
| `CNeoMetaClass.h`       | 2.8 KB  | Class metadata / registry                     |
| `CNeoIterator.h`        | 2.5 KB  | Generic iterator                              |
| `CNeoIndexIterator.h`   | 2.2 KB  | B-tree index iterator                         |
| `CNeoBlob.h`            | 3.3 KB  | Blob (raw binary data)                        |
| `CNeoIOBlock.h`         | 1.5 KB  | I/O block cache                               |
| `CNeoDoc.h`             | 1.0 KB  | Document container (Mac)                      |
| `CNeoApp.h`             | 1.0 KB  | Application container (Mac)                   |

## Purpose of inclusion in this project

These headers define the **on-disk format and runtime API contract** of
the X-Files HDB file. The byte-direct decoder under `../../python/` and
`../../cpp/` was developed by:

1. Reading these headers to understand the C++ class layout and serialized
   primitives (e.g. `kNeoPersistFileLength = 2 + sizeof(NeoVersion) = 6`).
2. Cross-checking the resulting interpretation against the actual bytes
   of `XFILES.HDB`.
3. Confirming via byte-identical round-trip that no field was missed.

Without these reference headers, the project would be ~10x harder to
verify against the original API contract.

## Legal basis

Inclusion is supported by the **interoperability** provisions of:

- **17 U.S.C. § 1201(f)** (USA, DMCA) — format analysis exemption for
  achieving interoperability between an independently created computer
  program and other programs.
- **Article 6, Directive 2009/24/EC** (European Union, Computer Programs
  Directive) — interoperability exception for interoperability.
- **§ 69e Urheberrechtsgesetz** (Germany) and equivalent statutes in
  other jurisdictions.

The headers are not modified, are clearly marked third-party, and are
provided for the **sole purpose of interoperability** with an independent
decoder.

## Removal request

If you are the rights holder of these files (NeoLogic Systems Inc. or its
successor in interest) and you prefer they be removed, please open an issue
in this repository. They will be deleted promptly, and the decoder will
continue to function (since it does not link against these files; they are
**reference only**).

## Original copyright notices

All files retain their original copyright headers:

> Copyright © 1992-1996 NeoLogic Systems Inc. All Rights Reserved.

These are reproduced unchanged in each file.
