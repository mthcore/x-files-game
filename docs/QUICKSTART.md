# Quickstart

You have a copy of `XFILES.HDB` (from the original *The X-Files:
The Game* game disk). You want to decode it. Here's how, in
under 5 minutes.

## Prerequisites

- Python 3.10 or newer
- `pip` (usually bundled with Python)
- A copy of `XFILES.HDB` (the game data file, ~6 MB)

## Install

```bash
git clone https://github.com/mthcore/x-files-game.git
cd x-files-game/python
pip install -e .
```

Only one runtime dependency: `click>=8.0`. The whole package is
~67 source files of pure Python (no compiled extensions).

## Inspect

```bash
python -m hdb_extract inspect /path/to/XFILES.HDB
```

```
file: /path/to/XFILES.HDB
size: 6,074,064 bytes  pages: 23,726  trailer: 176B (zero)

Header (big-endian, 32 bytes):
  version       =            3  (0x00000003)
  root_or_size  =      5,310,632  (0x00510F68)
  record_count  =       64,896  (0x0000FD80)
  ...

Page distribution (first byte tag):
  0x00  empty                  14,286  60.2%
  0xC2  btree_internal            989   4.2%
  0xC0  btree_freed               754   3.2%
  0xC3  btree_leaf                202   0.9%
  0xD2  btree_internal_alt        187   0.8%
  ...

Classes documented (Python ClassSpec): 51
Container roundtrip byte-identical: True
```

## Verify and audit

```bash
python -m hdb_extract verify /path/to/XFILES.HDB
# container roundtrip byte-identical : True

python -m hdb_extract audit /path/to/XFILES.HDB
# byte-direct = 6,074,064 / 6,074,064 = 100.00%
```

## Extract everything

```bash
python -m hdb_extract extract /path/to/XFILES.HDB --out=out/
```

Produces under `out/`:

```
out/
├── json/
│   ├── triggers_full.json         # 61 triggers with their grammar
│   ├── decision_tree.json         # full dialogue/story branching
│   ├── classes.json               # 51 class definitions
│   ├── neoidlist.json             # ID list records
│   ├── production_paths.json      # scene -> media asset paths
│   ├── page_index.json            # page-by-page catalog
│   ├── helper_reads.json          # decoder trace for QA
│   ├── decoded_records.json       # all 12,699 walked records
│   └── ...
├── hdb_extract.db                 # SQLite mirror of the above
└── audit/
    └── coverage.md                # byte-direct coverage report
```

## Full byte-level dump (for round-trip proof)

```bash
python -m hdb_extract dump-complete /path/to/XFILES.HDB --out=out/complete.json
```

Produces a ~26 MB JSON containing **every byte** of the source HDB in
structured form. Reconstructing the original file from this JSON is
byte-identical:

```
Building byte-direct dump of 6,074,064 bytes...
  pages : 23,726
wrote out/complete.json  (26,497,315 bytes)
roundtrip from dump byte-identical : True
```

## One class at a time

```bash
python -m hdb_extract dump-class VCTrigger /path/to/XFILES.HDB --limit=3
```

## Query the SQLite

```bash
sqlite3 out/hdb_extract.db
sqlite> SELECT class_name, COUNT(*) FROM records GROUP BY class_name ORDER BY 2 DESC LIMIT 10;
```

## Use it in your own code

```python
from hdb_extract.container.container import HdbContainer
from hdb_extract.classes.registry import CLASSES_BY_NAME, apply_read
from hdb_extract.serializer.cursor import HDBSerializer

container = HdbContainer.from_path("/path/to/XFILES.HDB")
print(f"{container.page_count} pages, roundtrip={container.roundtrip_ok()}")

# Walk leaves, decode VCTrigger records, etc.
# See python/src/hdb_extract/extractors/ for examples.
```

## C++ version

```bash
cd cpp
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/hdb_dump /path/to/XFILES.HDB
```

See [cpp/README.md](../cpp/README.md) for the C++ API.

## Troubleshooting

| Symptom                                          | Fix                                                |
|--------------------------------------------------|----------------------------------------------------|
| `ModuleNotFoundError: No module named 'hdb_extract'` | run `pip install -e .` from `python/` dir         |
| Tests skipped with "HDB not available"           | tests need the real game file; set its path        |
| `audit` says `< 100%`                            | install the latest version — old code marked some leaves as UNKNOWN |
| `roundtrip = False`                              | the source file is corrupted or non-original       |
