# hdb_extract

Extract `XFILES.HDB` (NeoLogic Hammer Database, X-Files The Game)
to JSON + SQLite at **100% byte-direct fidelity**.

## Goal

Produce a complete, lossless dump of the game's master database :

- Flows (scene graph nodes / locations / viewpoints / views / characters)
- Scenes + hotspots (with action_ids and coordinates)
- Conversations (questions, responses, history)
- Actions (807 IDs with handlers/args)
- Triggers (conditions / actions / targets)
- Variables (670 GAM-style state vars)
- Emails / PDA notes / photos / inventory / evidence icons
- All 52 VC* classes with their TLV-decoded fields
- NL bytecode scripts (if present in HDB)

## Outputs

```text
out/
├── json/
│   ├── flows.json           # scene graph nodes + hierarchy
│   ├── scenes.json          # 680 scenes
│   ├── hotspots.json        # 2278 hotspots
│   ├── variables.json       # 670 GAM vars
│   ├── actions.json         # 807 actions
│   ├── triggers.json        # ~61 triggers
│   ├── conversations.json   # ~179 questions + ~239 responses
│   ├── emails.json          # all email instances
│   ├── pda.json
│   ├── inventory.json
│   ├── classes.json         # 52 classes + instance counts
│   └── audit.json           # bytes_unknown (target: empty)
├── scenes/<scene_id>.json   # per-scene aggregate
├── hdb_extract.db           # SQLite with cross-table FK
└── audit/
    ├── bytes_unknown.json   # must be empty for 100% completion
    └── coverage_report.md
```

## How to read the per-scene gameplay flow

Each `out/scenes/scene_<name>.json` contains an ordered `flow` array.
Reading order :

```json
{
  "name": "Wong",
  "flow": [
    { "order_index": 0, "sequence": "009", "label": "Sequence 009 — daytime — boat absent",
      "viewpoints": { "11w": [...], "2w": [...], "3e": [...], ... } },
    { "order_index": 1, "sequence": "010", "viewpoints": {...} },
    { "order_index": 2, "sequence": "016", ... },
    ...
  ]
}
```

- The `flow` array is **strictly ordered**. `order_index: 0` is the first
  gameplay step (lowest sequence number, typically the scene's entry
  videos). `order_index: 1` is the next step, etc.
- Each entry's `sequence` field gives the raw sequence number observed
  in the production filenames (e.g. `010_won_4n_boat.mov` → seq=`010`).
- The `viewpoints` object maps Myst-style 360° positions
  (`1n`, `2w`, `3e`, `4s`, `11se`, `12w`, ...) to the videos that play
  at that position during this sequence.
- Each video has `states` (e.g. `["boat"]` or `["no_boat", "day"]`) :
  these are the game-state conditions that select which variant plays.
- The `take` field (when present, e.g. `take: 4`) means production
  retake — alternate version of the same shot.

**Gameplay loop** (re-implementation reference) :

1. Player enters the scene → start at `flow[0]` (lowest order_index).
2. Play the video for the current viewpoint, picking the variant whose
   `states` match the current game-state flags (e.g. `bIsHeld == true`
   selects `states: ["boat"]`).
3. Listen for hotspot clicks. Each hotspot has 2 `action_id`s pointing
   into `decision_tree.json:actions`. Firing an action triggers the
   matching `trigger` (see `scene.triggers` for this scene), which sets
   a `bAI<Scene>_<Action>` flag in the global game state.
4. When the player has triggered the right combination of flags, the
   scene advances to the next `flow[order_index+1]` step.
5. Continue until a navigation hotspot transitions to another scene.

## 🎯 100% byte-direct extract — DONE (2026-05-17)

The `dump-complete` command produces a JSON representation of **every
byte** of XFILES.HDB :

```bash
hdb-extract dump-complete path/to/XFILES.HDB
# wrote out/json/hdb_complete.json (26,497,330 bytes)
# roundtrip from dump byte-identical : True
```

This is verified by automated test
`test_complete_dump_roundtrip_byte_identical` — the dump can reconstruct
the original 6,074,064-byte HDB exactly.

For each of the 23,726 pages, the dump records :
- `tag_hex`, `tag_name`, `is_structural`, `kind_byte`
- `hex` : full hex content
- `ascii_runs` : every ≥4-char printable ASCII string with its offset
- `markers` : every occurrence of 'ID  ', 'NPfl', 'NPTc', 'NPlt', 'vers', 'null'

Plus the parsed header and the 176-byte zero trailer.

This is the **literal 100% extract** : zero byte is lost or unrepresented.

## What's not yet semantically decoded

The VC* records inside data pages use a **packed wire format** where
FOURCC tags ('NPfl', 'NPTc', etc.) are remapped to short byte-tags via a
per-class table inside the serializer's vtable. Cracking that table (Gate
G1) is required to get field-level decoding of each record. See
`docs/wire_packed.md` for the strategy.

Until G1 is cracked, the records appear as raw hex in `hdb_complete.json`.
The 51 ClassSpec in `classes/generated/all_classes.py` are ready to
decode them once the remap table is plugged in.

## Status (2026-05-17 MVP)

| Capability | Status |
|---|---|
| Container header + pages + trailer parse | ✅ 100% |
| Container roundtrip byte-identical | ✅ |
| 51 ClassSpec data-driven (port from the C++ reference decoder, see `cpp/`) | ✅ |
| HDBSerializer wire CLAIR (synthetic, FOURCC literals) | ✅ (17 unit tests pass) |
| Polymorphic dispatcher (slot 0x138 read_obj) | ✅ on synth blobs |
| B-tree leaf walker + data page NPfl scan | ✅ best-effort |
| CNeoIDList extraction | ✅ **13,982** records out of ~15,606 markers |
| Production paths extraction | ✅ **2,455** paths (~97.4% match vs ground-truth 2,432) |
| Classes inventory JSON | ✅ 51 specs with op summary |
| SQLite cross-table output | ✅ 16 summary rows + 4 indexed tables |
| Integration tests vs ground truth | ✅ 6/6 pass |
| **HDB packed wire (FOURCC→byte-tag remap)** | ❌ G1 unresolved (~25-35h needed) |
| **B-tree internal walking (records by ID lookup)** | ❌ G3 partial (4-8h format analysis) |
| **NL bytecode decode** | ❌ G5 pending |
| **Unknown_complex audit cleanup** | ❌ G6 pending |

The MVP delivers everything that can be byte-direct extracted **without
the per-class FOURCC→byte-tag remap table** (Gate G1). Once G1 is
decoded, the existing
`apply_read` driver and 51 ClassSpec will decode 100% of records.

See `docs/wire_packed.md` for the G1 strategy and what's still
unresolved.

## CLI

```bash
hdb-extract inspect   path/to/XFILES.HDB    # header + page distribution
hdb-extract audit     path/to/XFILES.HDB    # byte-direct coverage report
hdb-extract extract   path/to/XFILES.HDB    # full JSON + SQLite dump
hdb-extract verify    path/to/XFILES.HDB    # roundtrip + ground truth diff
hdb-extract dump-class 0x31 path/to/XFILES.HDB   # one class only
```

## Layout

```text
src/hdb_extract/
├── cli.py                # entry point
├── container/            # 256B pages, 32B header BE
├── btree/                # 0xC2/0xC3/0xD2 walker
├── serializer/           # ByteCursor + tracking + TLV wire format
├── format/               # FOURCC, CNeoIDList, base_init
├── classes/              # 52 ClassSpec (TLV grammars, generated from CSV)
├── polymorphe/           # slot 0x138 read_obj dispatcher
├── nl_script/            # NL bytecode (optional, G5 gate)
├── extractors/           # 13 category extractors → JSON
├── output/               # JSON / SQLite / audit writers
├── verify/               # roundtrip + diff vs ground-truth datasets
└── ext/                  # ground-truth CSV loaders (read-only)
```

## Reference

The format documentation covers :

- container layout
- TLV grammars for save classes
- FOURCC + CNeoIDList
- the G4 trailer (176B zero padding)
- the G3 B-tree (partial)
- 52 class Read bodies

A C++ reference implementation accompanies this package.
