# Roadmap

Current release: **v0.3.0** (full-ISO decoder + write path + MSVC + Pages, 2026).

## Done in v0.1.0

- 100.00% byte-direct coverage of the original `XFILES.HDB` (6 074 064
  bytes classified, 0 unknown).
- 51/51 VC* classes specified with byte-exact `Read()` grammars.
- 50/50 Python pytest tests passing.
- Container roundtrip byte-identical (proof via `dump-complete`).
- Python CLI: `inspect`, `audit`, `verify`, `extract`, `dump-class`,
  `dump-complete`.
- C++ decoder subset (compiles, smoke test, demo `hdb_dump` tool).
- NeoPersist 3.0 reference headers included verbatim with full provenance.
- 12 documentation pages (this is the 12th).

## Done in v0.2.0

- **PFF extractor** + byte-identical round-trip on `X.PFF` / `X7.PFF`.
- **GAM savegame** integration (same container as HDB, version 5).
- **Localization DLL decoder** — 1 336 French strings extracted from
  the 4 `XFiles*.dll` resource-only PEs.
- **QuickTime MOV parser** for `.xmv`, `.nmv`, `.amv`, `.dmv`.
- **C++ video decoders** (Cinepak, IMA ADPCM, QT container) ported and
  byte-validated against FFmpeg.
- **Catch2 vendored** + 12 standalone unit tests ported.
- **6 new docs** (ISO_FILES, PFF_FORMAT, GAM_FORMAT, LOCALIZATION,
  VIDEO, XT_SCRIPTS) + 3 reference notes.

## Done in v0.3.0

- ✅ **MSVC build parity** (with macro-wrapped `static_assert`).
- ✅ **Single-binary CLI** via PyInstaller + release workflow.
- ✅ **MkDocs Material** docs site on GitHub Pages.
- ✅ **Auto-merge** `decision_tree.json` × localization DLLs.
- ✅ **Write path** Python encoder (scalar fields).
- ✅ **PICT decoder** restored with `stb_image` vendored.

## Planned for v0.4

- **C++ Catch2-based test suite** (currently standalone `int main()`).
- **Write path** for polymorphic SubObj / ObjList ops.
- **B-tree internal node semantic decoder** if the `CNeoNode.cp` source
  surfaces from a public archive (currently the project covers these
  pages structurally / round-trip-identically, but doesn't decode the
  per-kind layout — see [FORMAT.md §6](FORMAT.md)).

## Planned for v0.4+

- Support for **other NeoPersist games**:
  - *Quantum Gate II: The Vortex* (HyperBole, 1994) — same engine,
    smaller HDB.
  - *Doom for Macintosh* save format — minor variant of NeoPersist 3.0.
- **XTLanguage bytecode VM** — if any of these other titles ship a
  binary bytecode (X-Files is plain text), the interpreter would go
  here. See [XT_SCRIPTS.md](XT_SCRIPTS.md).
- **PICT decoder** ported (currently needs the `stb_image` vendored
  dependency).

## Maybe / community-driven

- **Web playground**: paste an HDB byte range, see a parsed structure.
  Browser-only, no upload, all client-side.
- **C# / Rust bindings** on top of the C++ static library.
- **Comparison tool**: diff two HDB files semantically (helpful for
  the AGRIPPA team to validate their reconstructions).

## Known limitations (won't fix in v0.1)

- The `0xC2` / `0xD2` B-tree internal pages have 56 distinct
  `(tag, kind)` combinations whose exact semantic layout cannot be
  derived from the public NeoPersist headers. They are decoded
  **structurally** (every byte is classified and round-trip-identical)
  but not **semantically** per-kind. Closing this requires either:
  - The `CNeoNode.cp` source file from NeoLogic (not in any public
    SDK we've located).
  - Further analysis of the navigator routine's on-disk page-access
    order (~20-40h of focused format-analysis work).

  See [FORMAT.md §6](FORMAT.md) for the structural decode. This
  limitation does **not** affect the byte-direct round-trip nor the
  extraction of all 51 user-facing VC* classes (where game content
  lives).

## Contributing

Highest-impact contributions:
1. Tracking down `CNeoNode.cp` from public NeoLogic / DoomMac /
   CodeWarrior archives.
2. Porting one Python ClassSpec to C++ (or vice versa) for a class
   currently not represented in both.
3. Documenting an X-Files game scenario (a specific trigger or dialog
   branch) using the decoded data, as a concrete usage example.

See [../CONTRIBUTING.md](../CONTRIBUTING.md) for the workflow.
