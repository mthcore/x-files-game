# The X-Files: The Game — Decompilation Project

> An open-source decompilation/preservation project for *The X-Files:
> The Game* (1997, HyperBole Studios). It decodes the game's
> `XFILES.HDB` data file — serialized with NeoPersist 3.0 — byte-direct,
> with a verified byte-identical round-trip, and reconstructs the game's
> scenes, triggers, hotspots and runtime state purely from the data files.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Python 3.10+](https://img.shields.io/badge/Python-3.10+-blue.svg)](python/pyproject.toml)
[![C++17](https://img.shields.io/badge/C++-17-orange.svg)](cpp/CMakeLists.txt)
[![Coverage: 100% byte-direct](https://img.shields.io/badge/byte--direct-100%25-brightgreen.svg)](python/out/audit/coverage_final.md)

## What is this?

*The X-Files: The Game* (1997) is an FMV adventure game that ships
its entire game state — scenes, hotspots, dialogue trees, triggers, actions,
NPC conversations, email threads, decision graphs — inside a single binary
file called `XFILES.HDB`. That file is serialized with NeoLogic's
**NeoPersist 3.0** library (used by HyperBole's **VirtualCinema** engine).

For 25+ years, the contents of this file have been opaque to fans and
preservationists. The competing web port `agrippa/xesf` (≈15% playable)
explicitly bailed on the HDB because they could not extract the flow,
decision trees, or dialogue data.

**This project decodes the HDB at 100% byte-direct fidelity** and exports
it to JSON and SQLite. Every byte of the original file is classified,
re-serializable to a byte-identical copy.

## Quickstart

```bash
# Python (recommended — pure Python, no external deps beyond `click`)
cd python
pip install -e .
python -m hdb_extract inspect    /path/to/XFILES.HDB
python -m hdb_extract verify     /path/to/XFILES.HDB
python -m hdb_extract audit      /path/to/XFILES.HDB
python -m hdb_extract extract    /path/to/XFILES.HDB --out=out/
python -m hdb_extract dump-class VCTrigger /path/to/XFILES.HDB --limit=3
```

```bash
# C++ reference game engine — reads the HDB, rebuilds the game model
# (scenes, triggers, hotspots), and runs the game loop. See docs/ENGINE.md.
cd cpp && cmake -S . -B build && cmake --build build
./build/xfiles_engine /path/to/XFILES.HDB /path/to/XV
```

See [docs/QUICKSTART.md](docs/QUICKSTART.md) for the full walkthrough,
including the SQLite schema and the JSON output structure. For the in-depth
guide — how it all works, every way to use it, the reference engine, and an
honest roadmap of what a full playable rebuild still needs — read
[docs/REMAKE.md](docs/REMAKE.md).

## Repository layout

```
.
├── python/        Pure-Python decoder (hdb_extract package). Pytest-based,
│                  no compile step. The reference implementation.
├── cpp/           C++17 byte-direct decoder, mirror of the Python one.
│                  Builds with CMake + Catch2 single-header (vendored).
│                  Includes `xfiles_engine` — a self-contained reference game
│                  engine (reads the HDB, rebuilds the scene/trigger/hotspot
│                  model, runs the game loop). See docs/ENGINE.md.
├── headers/       NeoPersist 3.0 reference headers (13 .h + 2 .cp) included
│                  verbatim for interoperability. See NOTICE.
├── docs/          Curated documentation: format spec, history, glossary,
│                  per-domain guides (triggers, hotspots, dialogues, etc.).
├── examples/      Sample JSON + SQLite outputs (no game binary included).
├── tools/         Sync and release scripts; not needed for end users.
└── .github/       CI workflows + issue/PR templates.
```

## What's decoded byte-direct

### From `XFILES.HDB` (v0.1)

| Domain                | Records  | Format                  | Reference         |
|-----------------------|---------:|-------------------------|-------------------|
| Triggers (game logic) |       61 | JSON (full + grammar)   | [docs/TRIGGERS.md](docs/TRIGGERS.md) |
| Hotspots (UI regions) |    2 278 | CSV/JSON                | [docs/HOTSPOTS.md](docs/HOTSPOTS.md) |
| Scenes / asset map    |    2 432 | CSV                     | [docs/FORMAT.md](docs/FORMAT.md)     |
| Conversations         |  numerous| JSON                    | [docs/DIALOGUES.md](docs/DIALOGUES.md) |
| Decision tree         |   1.6 MB | JSON                    | [docs/DECISIONS.md](docs/DECISIONS.md) |
| Emails (PDA)          |  numerous| CSV/JSON                | [docs/DECISIONS.md](docs/DECISIONS.md) |
| VC* classes           |    51/51 | byte-exact ClassSpec    | [docs/CLASSES.md](docs/CLASSES.md)   |
| Full container        |    100%  | Round-trip identical    | [docs/FORMAT.md](docs/FORMAT.md)     |

### Other ISO files (v0.2 — new)

| File                  | Format                  | Reference         |
|-----------------------|-------------------------|-------------------|
| `X.PFF`, `X7.PFF`     | PICT archive (round-trip identical)  | [docs/PFF_FORMAT.md](docs/PFF_FORMAT.md) |
| `XFILES.GAM`          | Savegame (NeoPersist 3.0 v5)         | [docs/GAM_FORMAT.md](docs/GAM_FORMAT.md) |
| `*.xmv / .nmv / .amv / .dmv` | QuickTime MOV (Cinepak / MJPEG / IMA ADPCM) | [docs/VIDEO.md](docs/VIDEO.md) |
| `XFiles{c,e,s,t}.dll` | Windows PE resource strings (1 336 strings in FR) | [docs/LOCALIZATION.md](docs/LOCALIZATION.md) |
| `XT/*.xtx`            | Plain-text PDA content (474 files)   | [docs/XT_SCRIPTS.md](docs/XT_SCRIPTS.md) |
| **Full ISO**          | Inventory & SHA-256 catalog          | [docs/ISO_FILES.md](docs/ISO_FILES.md)   |

## Status

- **100.00 % byte-direct coverage** of `XFILES.HDB` (6 074 064 bytes,
  0 unknown). See [coverage report](python/out/audit/coverage_final.md).
- **51/51** VC* classes spec'd byte-exact.
- **137** Python unit + integration tests passing; **9** C++ standalone
  tests (round-trip on the dispatcher save/load JSON, HSPT decoder,
  `xfiles_play --probe` smoke).
- Container round-trip byte-identical confirmed via `dump-complete`.
- Reference engine ships a scripted-playthrough validator and a headless
  dispatcher (`xfiles_engine --validate-flow`): 18/29 canonical steps
  byte-direct PASS, 11 walkthrough-only, 0 FAIL; 113 trigger
  effect-summary statements interpreted, 48 variables mutated, ≥85 % of
  them validated against the GAM namespace.
- Optional SDL2 playable shell (`xfiles_play`, `-DXFILES_PLAY=ON`):
  resolves `--location "Field Office"` to a byte-direct scene_id, walks
  the canonical 29-step flow with SPACE / BACKSPACE, dispatches each
  scene's triggers on entry, F5 / F9 saves / loads the state.
- Python ↔ C++ dispatcher parity locked by integration test.

## Background

- See [docs/HISTORY.md](docs/HISTORY.md) for the engine (VirtualCinema by
  HyperBole), the library (NeoPersist 3.0 by NeoLogic), and the prior art
  (AGRIPPA web port).
- See [docs/FORMAT.md](docs/FORMAT.md) for the on-disk layout: file header,
  256-byte pages, 8-byte markers, B-tree structure.
- See [docs/WIRE_FORMAT.md](docs/WIRE_FORMAT.md) for the wire-packed
  serialization and why the "FOURCC → byte-tag mapping table" you may find
  mentioned in older RE notes does not actually exist on disk.

## Contributing

PRs welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for style, sign-off
requirements, and the rule about **never** attaching the game binary to an
issue or PR.

## Security

Open issues with `[security]` label, or follow [SECURITY.md](SECURITY.md).

## License

MIT — see [LICENSE](LICENSE). Third-party content disclosed in [NOTICE](NOTICE).
