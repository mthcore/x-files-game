# Example outputs

This directory contains **sample outputs** produced by `hdb_extract` from
the original `XFILES.HDB` (1997). They are committed to the repository so
new contributors can see what the decoder produces **without owning the
game**.

The game binary itself is **not redistributed** (see top-level NOTICE).

## Files

| File                                | Size    | Description                                                              |
|-------------------------------------|---------|--------------------------------------------------------------------------|
| `coverage_final.md`                 | 5 KB    | Certificate: 100.00% byte-direct coverage on XFILES.HDB                  |
| `outputs/triggers_full.json`        | 20 KB   | All 61 game triggers (Conditions / Actions / Targets grammar)            |
| `outputs/decision_tree.json`        | 1.6 MB  | Complete decision tree (dialogue branches, story flow)                   |
| `outputs/classes_summary.json`      | 22 KB   | Summary of 51 VC* classes (class_id, size, field count)                  |
| `outputs/hdb_extract.sample.db`     | 768 KB  | SQLite snapshot: classes, NeoIDList, production_paths, summary tables    |

## How to regenerate

```bash
cd python
pip install -e .
python -m hdb_extract audit          /path/to/XFILES.HDB
python -m hdb_extract verify         /path/to/XFILES.HDB
python -m hdb_extract extract        /path/to/XFILES.HDB --out=out/
python -m hdb_extract dump-complete  /path/to/XFILES.HDB --out=out/complete_dump.json
```

After `extract`, fresh JSON files appear under `python/out/json/` and a
SQLite database under `python/out/hdb_extract.db`.

## What about the full byte-direct dump?

The `dump-complete` command produces a ~26 MB JSON that contains *every
byte* of the source HDB in structured form. The roundtrip is byte-identical
to the original file. We do **not** commit it here because:

1. It is ~12× the size of the original HDB.
2. Reconstructing the original HDB from it would technically count as
   redistributing the game data.

If you own the game, run `dump-complete` locally.

## Quick inspection

Open `triggers_full.json` in any JSON viewer and search for `"VCTrigger"`.
You'll see records like:

```jsonc
{
  "trigger_id": 12,
  "class": "VCTrigger",
  "conditions": [...],
  "actions": [...],
  "targets": [...]
}
```

This is exactly the data structure that competitor projects (AGRIPPA web
port) failed to extract — and that is now openly available.
