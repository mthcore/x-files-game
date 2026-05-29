# History — engine, library, prior art

## The X-Files: The Game (1997)

A point-and-click FMV adventure based on the TV series, published by
Fox Interactive and developed by **HyperBole Studios** in Seattle, WA.
Released July 1997 on PC (Windows 95). A multi-CD release containing
~700 MB of pre-rendered video, audio, and game state.

The game engine, called **VirtualCinema**, was HyperBole's in-house
authoring framework. The same engine had earlier shipped *Quantum Gate*
(1993) and other FMV titles. VirtualCinema persisted **all** game-state
data — scenes, hotspots, character dialogue, trigger logic, decision
trees, inventory items, email threads — inside a single file called
`XFILES.HDB` (Hyperbole DataBase).

## NeoPersist 3.0 — the underlying library

The `.HDB` file format is not HyperBole-specific. It is the on-disk
representation of **NeoPersist 3.0**, a C++ object-persistence library
authored by **NeoLogic Systems Inc.** between 1992 and 1996.

NeoPersist provided automatic save/load of arbitrary C++ object graphs
into a single file, with B-tree indexing for fast lookups. It was
distributed as part of the **CodeWarrior 7 SDK** (Metrowerks) and used
internally at NeoLogic for their own products under the brand
**NeoAccess**.

A handful of mid-1990s commercial titles shipped using NeoPersist:
- *The X-Files: The Game* (HyperBole, 1997) — this project
- *Quantum Gate II: The Vortex* (HyperBole, 1994)
- *Doom for Macintosh* save format (id Software, 1995) — partial
- Various Mac-platform productivity tools

The library was Macintosh-first; the X-Files Windows port preserves the
**big-endian** byte order in the on-disk format despite running on
little-endian x86. Every `long` and `short` field in the HDB is BE.

For the API contract, see the verbatim headers under
[`../headers/neopersist_3.0/`](../headers/neopersist_3.0/) — published
in the public CodeWarrior 7 SDK.

## format analysis history

### Phase A (2024-2025) — community probes
Various fan attempts to read the HDB stopped at the wire layer. The
8-byte marker invariant was recognized empirically but its semantic
(CNeoPersist header) wasn't connected to the public NeoPersist headers.

### Phase B (2025) — AGRIPPA web port
A semi-public effort at a web-based playback of X-Files
(`xesf/agrippa` ≈ 15% playable) bypassed the HDB entirely. Their notes
state:

> *We were unable to extract the flow, decision trees, or dialogue data
> from `XFILES.HDB`. Our port reconstructs these by patching binary
> assets directly.*

This is the gap this project closes.

### Phase C (2026) — this project
Full byte-direct decoding achieved through the convergence of:

1. **Byte-direct format analysis** of the on-disk records. Established the
   HdbSerializer read interface (119 typed slots), the polymorphic dispatch
   for `CNeoPersist::readObject`, and the per-class `Read()` field order for
   51 VC* classes.

2. **Cross-referencing the NeoPersist 3.0 public headers**, which gave
   the exact on-disk size of `CNeoPersist` (6 bytes: 2 flags + 4 BE
   version), the page quantum (8 bytes), and the meaning of every
   FOURCC tag observed on disk. The crucial insight: **`CNeoStream`
   primitives ignore the tag on disk** — the wire format is byte-direct,
   no tags written. The "FOURCC → byte-tag remap table" suggested by
   earlier analysis simply does not exist.

3. **Empirical validation**: a byte-identical round-trip test that
   re-serializes the entire HDB from the parsed structure and confirms
   bit-for-bit identity with the original.

This release is the consolidated, end-user-facing portion of that work.

## Why now?

The game is **28 years old** as of 2026. The publisher's interest in
this title is negligible. The community of fans is small but devoted.
Software preservation efforts (Internet Archive, OldGames.sk, individual
collectors) have made the HDB data file widely available, but its **content**
has remained opaque until now.

Opening the HDB serves three purposes:
1. **Preservation** — the game's narrative content is no longer locked
   inside a proprietary binary.
2. **Modding / fan ports** — fan-made HTML5 or modern-engine ports
   (like AGRIPPA) can finally use the original story data.
3. **Format documentation** — NeoPersist was an important early-1990s
   library, but its on-disk format was never publicly documented.
   This project fills that gap.

## Naming

- **HDB** = Hyperbole DataBase (HyperBole's naming convention).
- **NeoPersist** = NeoLogic's persistence library (the user-facing name
  for the on-disk format).
- **NeoAccess** = NeoLogic's broader product brand (same code).
- **Hammer** = NeoLogic's internal codename for the library
  (occasionally seen in old SDK comments).
- **VirtualCinema** = HyperBole's authoring engine.
- **VCObject**, **VC*** = HyperBole's content classes (51 in total),
  derived from CNeoPersist.

See [GLOSSARY.md](GLOSSARY.md) for the full term index.
