# C++ Decoder

C++17 byte-direct decoder for `XFILES.HDB`. Mirror of the Python decoder
under `../python/`. Uses **only the C++ standard library** — no external
dependencies.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Run the demo executable

```bash
./build/hdb_dump /path/to/XFILES.HDB
```

Prints a structural summary: file size, page count, trailer size, and page
type distribution. For full record extraction, use the Python CLI (the C++
public API for record-level dumping is planned for v0.2).

## Status

| Component       | Status                                         |
|-----------------|------------------------------------------------|
| `hdb/`          | Container, context, TLV reader — byte-direct   |
| `nl/`           | NeoStream serializer, polymorphic dispatch     |
| `nl/classes/`   | VC* class families (action, conversation, ...) |
| `vc/`           | Individual VC* class definitions               |
| `assets/`       | Cinepak / IMA ADPCM / QT container decoders    |
| `hdb_dump`      | Demo CLI — structural summary only             |
| 12 unit tests   | Standalone `int main()`-style (no test framework dep) |
| Catch2          | Vendored under `third_party/catch2/` for v0.3+ use |

## Compiler support

| Compiler       | Status         | Note                                       |
|----------------|----------------|--------------------------------------------|
| **gcc** 11+    | ✅ supported   | Primary target; CI runs `g++` on Linux.    |
| **clang** 14+  | ✅ supported   | Linux + macOS.                             |
| **MSVC** 2022  | ⚠️ **v0.2 TBD** | A handful of `static_assert(sizeof(X) == N)` constraints fire on MSVC due to layout differences in vtable pointer alignment. The struct sizes are correct on gcc/clang. Fix planned for v0.3 (use `#pragma pack` or alignment macros). |

If you need MSVC support today, build with `clang-cl` (Clang frontend
with MSVC ABI) which has been validated locally.

## Subset

This C++ tree is the **decoder subset** of the larger C++ reference
implementation. The following modules are **excluded** because they
are game-runtime concerns (not needed to decode the HDB):

- `engine/`, `audio/`, `render/`, `input/` — game runtime
- `gameplay/`, `scripts/` — game logic
- `script_engine.{h,cpp}` — script VM
- `trigger_executor.{h,cpp}` — runtime trigger evaluator
- `registry_script_host.h` — script host

The Python decoder is the **reference implementation** and is feature-
complete (51/51 classes, 100% byte-direct, 50/50 tests passing). The C++
mirror tracks Python.

## Why C++ matters

Some users will want to embed the decoder in a compiled game-asset pipeline
(e.g. a Unity plugin reading HDB at edit time, or a fan-made re-release
engine). Pure Python is too slow for batch processing of dozens of HDB
files. The C++ library is positioned for that integration use case.
