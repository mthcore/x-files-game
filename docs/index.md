# The X-Files: The Game — Decompilation Project

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://github.com/mthcore/x-files-game/blob/main/LICENSE)
[![Python 3.10+](https://img.shields.io/badge/Python-3.10+-blue.svg)](https://github.com/mthcore/x-files-game/blob/main/python/pyproject.toml)
[![C++17](https://img.shields.io/badge/C++-17-orange.svg)](https://github.com/mthcore/x-files-game/blob/main/cpp/CMakeLists.txt)
[![Coverage: 100% byte-direct](https://img.shields.io/badge/byte--direct-100%25-brightgreen.svg)](https://github.com/mthcore/x-files-game/blob/main/examples/coverage_final.md)

> An open-source decoder for **The X-Files: The Game** (1997,
> HyperBole Studios). Decodes the entire game ISO byte-direct:
> `XFILES.HDB`, `XFILES.GAM`, the `X.PFF` archives, the `XMV`/`NMV`/`AMV`
> QuickTime videos, the localization DLLs, and the XT text scripts.

## What this is

For 25+ years the data file `XFILES.HDB` was opaque: a 6 MB NeoPersist
3.0 container holding all the game logic — scenes, hotspots, triggers,
dialogue trees, decision graphs. This project:

- decodes the HDB **100% byte-direct** with a verified byte-identical
  round-trip;
- exports all the gameplay data to JSON and SQLite;
- ships the same byte-direct guarantee for the other ISO file formats
  (PFF, GAM, MOV, DLL).

The competing web-port `agrippa/xesf` explicitly bailed on the HDB
because they could not extract the flow, decision trees, or dialogue
data. **This project closes that gap.**

## Quickstart

```bash
pip install -e python/
python -m hdb_extract verify /path/to/XFILES.HDB
# container roundtrip byte-identical : True

python -m hdb_extract audit /path/to/XFILES.HDB
# byte-direct = 6,074,064 / 6,074,064 = 100.00%

python -m hdb_extract extract /path/to/XFILES.HDB --out=out/
# JSON + SQLite under out/
```

See [Quickstart](QUICKSTART.md) for the full walkthrough.

## Status

| Component                       | Status                          |
|---------------------------------|---------------------------------|
| HDB byte-direct coverage        | **100.00 %** (0 unknown bytes)  |
| 51 VC* classes byte-exact       | ✅ Python + C++ specs           |
| Container roundtrip identical   | ✅ confirmed                    |
| Python tests passing            | **92 / 92**                     |
| C++ build (gcc / clang / MSVC)  | ✅ tested                       |
| PFF / GAM / MOV / DLL decoders  | ✅ shipping (v0.2)              |
| Write path (Python)             | ✅ scalar fields (v0.3)         |
| Single-binary CLI               | ✅ via PyInstaller release.yml  |

## What's where

- **Python** (`python/`) — reference implementation, pure Python +
  `click`, no compile step.
- **C++** (`cpp/`) — symmetric library, built with CMake, suitable for
  embedding in fan-ports.
- **Reference headers** (`headers/neopersist_3.0/`) — verbatim NeoLogic
  SDK headers for interoperability, see `NOTICE`.
- **Example outputs** (`examples/`) — JSON + SQLite produced from the
  retail game, ready to inspect without owning the game executable.
- **Documentation** (`docs/`) — this site.

## License & legal

MIT — see [LICENSE](https://github.com/mthcore/x-files-game/blob/main/LICENSE).

The game binary itself is **never** redistributed. The decoder reads a
copy of the game you legitimately own. See
[NOTICE](https://github.com/mthcore/x-files-game/blob/main/NOTICE) for the
trademark and third-party content disclosure.
