# Triggers — the game logic engine

The X-Files game logic is expressed as **triggers**. A trigger is a
small condition-action machine bound to a scene or interface state.
When all of a trigger's **Conditions** are satisfied, its **Actions**
are executed against its **Targets**.

`XFILES.HDB` contains **61 triggers** (see
[`../examples/outputs/triggers_full.json`](../examples/outputs/triggers_full.json)).

## Class hierarchy

```
VCTrigger        (class_id 0x51, 64 bytes in memory)
└── VCTriggerList (class_id 0x52)  ← ObjList parent containers
```

Each `VCTrigger` is composed of three inline sub-collections (read after
the base record header):

- `Conditions[]` — up to 8 entries.
- `Actions[]` — up to 12 entries.
- `Targets[]` — up to 4 entries.

The sub-collections are not part of `VCTrigger`'s direct fields. They
are populated via the **Attach pattern**: each sub-object instantiated
elsewhere in the HDB registers itself with its parent trigger at parse
time. (This is the `VCTrigger_Attach` mechanism established from the
on-disk structure — see [WIRE_FORMAT.md](WIRE_FORMAT.md) for the format details.)

## VCTrigger byte-direct grammar

After the 8-byte CNeoPersist marker:

```
+8   uint32 BE   field 'e'  ← cache_e (semantic: trigger flags)
+12  uint8       field 'f'  ← cache_f (semantic: trigger type)
+13  padding to 8-byte alignment
```

See [CLASSES.md](CLASSES.md) row for `VCTrigger` and the Python
[ClassSpec](../python/src/hdb_extract/classes/generated/all_classes.py).

## Event types

From the byte-direct analysis of the `Eval` (slot 11) and `OnEvent` (helper)
methods, triggers respond to one of 9 event types ([reference](https://github.com/mthcore/x-files-game/blob/main/docs/reference/trigger_system_official.md)):

| Event code | Symbolic name        | Meaning                          |
|------------|----------------------|----------------------------------|
| 0          | `EVT_ENTER`          | Scene/state entered              |
| 1          | `EVT_EXIT`           | Scene/state exited               |
| 2          | `EVT_CLICK`          | Hotspot or interface item clicked |
| 3          | `EVT_HOVER`          | Cursor over hotspot              |
| 4          | `EVT_ACTION`         | Custom action triggered          |
| 5          | `EVT_CONDITION`      | Variable condition met           |
| 6          | `EVT_VIDEO_END`      | FMV playback finished            |
| 7          | `EVT_AUDIO_END`      | Audio cue finished               |
| 8          | `EVT_TIMER`          | Timer elapsed                    |

The event type is part of the trigger's parent-context binding, not of
the `VCTrigger` record itself.

## Condition types

A condition is a `VCAction` instance (yes — actions and conditions
share the class, the runtime interprets them differently). Common
condition codes:

- `OP_EQ`, `OP_NE`, `OP_LT`, `OP_GT`, `OP_LE`, `OP_GE` — variable
  comparisons.
- `OP_AND`, `OP_OR`, `OP_NOT` — logical composition.
- `OP_VAR_SET`, `OP_VAR_CLEAR` — variable presence tests.

## Action types

Actions resolve to one of ~8 functional families:

- `ACT_SET_VAR(var_id, value)`
- `ACT_CALL_UDF(udf_id, args...)` — user-defined function (89 UDFs in
  X-Files; see `python/src/hdb_extract/ext/udfs.py`)
- `ACT_LOAD_SCENE(scene_id)`
- `ACT_PLAY_VIDEO(asset_id)`
- `ACT_PLAY_AUDIO(asset_id)`
- `ACT_SHOW_HOTSPOT(hotspot_id, enabled)`
- `ACT_ENABLE_NPC(npc_id, enabled)`
- `ACT_FIRE_TRIGGER(trigger_id)` — trigger chaining

Action arg counts and types are encoded as `VCStdAction` records
(class_id `0x54`) which provide a 3-`Dword` payload.

## VCStdAction byte-direct vocabulary

A byte-direct walk of every `VCStdAction` record (class_id `0x54`) is
exported to
[`../examples/outputs/action_vocabulary.json`](../examples/outputs/action_vocabulary.json).

The 8-byte header is decoded byte-direct as four big-endian `u16` slots:

| Offset    | Field                   | Notes                                                                          |
| --------- | ----------------------- | ------------------------------------------------------------------------------ |
| `+0..+1`  | `persistent_record_id`  | intra-class id (60 distinct, no collisions)                                    |
| `+2..+3`  | `reserved`              | observed `0x0000` in every record                                              |
| `+4..+5`  | `action_opcode`         | the verb (23 distinct opcodes seen)                                            |
| `+6..+7`  | `flags`                 | `0`, `1`, `0x0300`, `0x037F`, `0x8000`, `0x8100`, `0x8200`, `0x8600` observed  |
| `+8…`     | `arg_bytes`             | opcode-specific payload, surfaced raw                                          |

### Resolution status (honest)

- **Records walked** — 60 / 60 `VCStdAction` records decoded
  byte-direct.
- **Header grammar** — byte-direct (table above).
- **Argument bytes** — emitted raw as hex plus ASCII runs (so e.g. asset
  filenames like `21704.xmv`, character names, scene labels are visible
  without inventing a schema).
- **Opcode → action verb mapping** — `undetermined` for every opcode.
  The 23 distinct opcodes are tabulated with their flags histogram,
  argument-size histogram, ASCII runs, and the matching
  `persistent_record_id` set, but no opcode is assigned a verb because
  the mapping is not part of the public format. The JSON header carries
  the honesty contract verbatim
  (`_honesty: "candidate_effect is 'undetermined' for every opcode..."`).
- **`ACT_*` symbolic families listed above** are the *expected* runtime
  groupings carried over from earlier notes — they are **not** byte-direct
  attributions of any specific opcode in `action_vocabulary.json`. Treat
  them as analysis hypotheses until each opcode is confirmed from a
  trace.

## Empirical relation byte distribution

Every condition part of a trigger (a `CNeoPart`) carries a single-byte
`relation` field at offset `+14`. The byte is **exposed raw** by this
project; the existing **Honest boundary** wording in
[ENGINE.md](ENGINE.md#honest-boundary) is unchanged:

> the **relation operator** byte (`relation` in a CNeoPart) is exposed
> raw; …These are computed at load time by the original engine from
> in-memory state, so a faithful re-implementation supplies them from
> its own object store…

The subsection below adds the **distribution** of that byte across all
HDB records (and across the 9 in-HDB triggers whose part-bytes
decoded byte-direct) as **auxiliary evidence**, not as a decode of its
semantics.

### Histograms (byte-direct)

`count_global` is the number of `relation` bytes seen across all
`CNeoPart` slots that decoded cleanly (737 records, 622 with at least
one decoded part, ~830 part-relations after sub-record traversal).
`count_in_canonical_triggers` is the same count restricted to the 9
triggers (out of 169 trigger marker offsets) whose condition parts
walked successfully — these are the only HDB triggers from which a
per-trigger relation byte is observable; "canonical-flow-firing" here
means "trigger record present in HDB whose part bytes decoded under
the byte-direct walker", not a name-identity match to a
`game_flow.json` step (see honesty note below).

Top values only (full table in
[`../examples/outputs/relation_byte_distribution.csv`](../examples/outputs/relation_byte_distribution.csv)):

| relation_byte | count_global | count_in_canonical_triggers | notes                                                     |
| ------------- | -----------: | --------------------------: | --------------------------------------------------------- |
| `0x00`        |          426 |                          32 | dominant value overall                                    |
| `0x01`        |          114 |                          18 | second most common; always `ref_b!=0`                     |
| `0x11`        |           66 |                          15 | third most common; always `ref_b!=0`                      |
| `0x43`        |            9 |                           0 | record-class-specific (see CSV)                           |
| `0x81`        |           12 |                           0 | record-class-specific (see CSV)                           |
| `0x69`        |            7 |                           0 | record-class-specific                                     |
| `0x5C`        |            7 |                           0 | record-class-specific                                     |
| `0x6E`        |            4 |                           0 | record-class-specific                                     |
| `0x09`        |            2 |                           3 | only non-{0,1,17} value seen in triggers above zero floor |
| `0x1A`        |            1 |                           1 | trigger-only outlier                                      |
| `0x41`        |            1 |                           1 | trigger-only outlier                                      |
| `0x6D`        |            2 |                           1 | trigger-only outlier                                      |
| 49 others     |           93 |                           0 | each ≤ 7 globally; see CSV                                |

61 distinct relation byte values are observed globally. Sum of
`count_global` = 830. Sum of `count_in_canonical_triggers` = 71.

### Empirical observation

- **Empirical observation:** `0x01` and `0x11` together account for
  `(114 + 66) / 830 ≈ 21.7 %` of global part-relations and
  `(18 + 15) / 71 ≈ 46.5 %` of the part-relations seen on the 9
  decoded triggers. Both values **always** appear with a non-zero
  `ref_b` (`0x01`: 114/114, `0x11`: 66/66), while `0x00` is the only
  value that appears with `ref_b == 0` in a non-trivial share
  (`120 / 426 ≈ 28 %`).
- A natural reading is that `0x01` and `0x11` are *paired*
  edge-types — one expressing an "equal" / "is" relation, the other a
  "not-equal" / "is-not" relation — and that the engine treats
  `0x00` as a structural slot (the `ref_b == 0` cases look like
  list-anchor edges, not comparisons). This is **a hypothesis from
  the distribution, not a decode**: the recon explicitly records the
  verdict `"inconclusive"` and the counter-evidence (e.g. `0x00`
  outnumbers `0x01`; `VCVariable` is absent from the top operand
  pairs of both `0x01` and `0x11`).
- **The exact semantic of every `relation` byte remains a game-side
  runtime computation.** Nothing in this section attributes a
  comparison verb to any value. The byte stays surfaced raw in
  `triggers_full.json` with a `byte-direct` certainty marker on the
  byte itself and an `undetermined` marker on its *meaning*.

The full recon (overall histogram, per-part-class breakdown, per
operand-class-pair table, top operand pairs per relation, the
`ref_b == 0` vs `ref_b != 0` split, and the formal
hypothesis-evaluation verdict) lives in
`order_recovery/relation_byte_empirical.json` and is intentionally
**not** mirrored into the OSS tree (it cites decomp-side observations).

### Honesty caveats

- The 9 "canonical-flow-firing" triggers are an upper bound: the recon
  could not name-identity-match HDB triggers to `game_flow.json` steps
  because `state_variable` names are XFiles.exe symbols, not HDB
  bytes (`variable_gated_trigger_count: 0`). The 71 part-relations
  attributed here are simply the parts that decoded inside the 169
  trigger marker offsets — every other trigger is `part_count: 0`.
- 41 of the 61 distinct relation byte values occur **exactly once**
  globally; treating their position in the histogram as signal would
  be over-fitting. The CSV includes them so a downstream consumer can
  re-aggregate against their own grouping.
- Per the existing **Honest boundary**, the OSS pipeline neither
  consumes nor re-emits a decoded relation operator. Conditions in
  `triggers_full.json` keep their `relation` field as a raw `uint8`
  with `certainty: "byte-direct"` on the byte and `meaning:
  "undetermined"` on the verb.

## How to extract triggers

```bash
python -m hdb_extract extract /path/to/XFILES.HDB --out=out/
cat out/json/triggers_full.json | jq '.[] | {id: .trigger_id, conds: (.conditions | length), acts: (.actions | length)}'
```

## How to extend (decode action UDFs)

The 89 user-defined functions referenced by triggers are decoded in
[`python/src/hdb_extract/ext/udfs.py`](../python/src/hdb_extract/ext/udfs.py).
That file maps each UDF id to a human-readable name based on the
format-analysis notes. Contributions welcome — many UDFs are
still `udf_NN` placeholders awaiting a meaningful name.

## See also

- [DECISIONS.md](DECISIONS.md) — how triggers connect to the dialogue
  decision tree.
- [HOTSPOTS.md](HOTSPOTS.md) — how `EVT_CLICK` events are routed to
  triggers.
- [`../examples/outputs/triggers_full.json`](../examples/outputs/triggers_full.json)
  — the full 61-trigger dump from XFILES.HDB.
