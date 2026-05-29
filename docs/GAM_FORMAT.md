# GAM — savegame format

The `XFILES.GAM` file is the player's **savegame**. It uses the same
on-disk container as `XFILES.HDB` (NeoPersist 3.0), with a few small
differences that this decoder accounts for automatically.

## Same parser, same byte-direct guarantee

```bash
python -m hdb_extract gam /path/to/XFILES.GAM
```

```
file       : XFILES.GAM
size       : 155,544 bytes
version    : 5  (GAM=5, HDB=3 — this file is GAM)
record_count : 38,864
class_count  : 4
pages        : 607
trailer      : 120B  (non-zero)
roundtrip byte-identical : True
```

The container roundtrip is **byte-identical**, confirming that the
parser fully accounts for the GAM-specific differences.

## How GAM differs from HDB

| Aspect             | HDB                  | GAM                                |
|--------------------|----------------------|------------------------------------|
| File version       | 3                    | **5**                              |
| Total size         | ~6 MB                | ~150 KB                            |
| Pages              | ~23 700              | ~600                               |
| Record count       | ~64 900              | ~38 900                            |
| Class count        | 51 (full VC* set)    | **4** (only player-mutated state) |
| Trailer            | 176 B of zero        | **~120 B of non-zero metadata**    |
| Empty pages        | ~60%                 | ~42%                               |
| Purpose            | Game definition      | Player progress snapshot           |

The trailer difference is significant: HDB ships with a fixed-size
zero trailer (allocator slack), whereas GAM uses the trailing bytes to
store the final allocator state of the save (last-write checkpoint).

## What's inside a GAM?

The savegame stores only the **mutable** state: which scenes the player
has visited, which variables have changed value, which conversations
were heard, current inventory, current PDA emails read. The game
definition itself (scenes, hotspots, trigger code, dialogue trees)
lives in HDB and is not duplicated in GAM.

Because of this, the GAM has very few **classes** but proportionally
more **state-bearing records**. Typically you'll see:

- `VCGameState` (class_id `0x57`) — the master state object.
- `VCVariable` records (class_id `0x53`) — touched variables.
- `VCConversationHistory` (class_id `0x56`) — heard nodes.
- `VCEmailRead` / `VCEmailPending` — PDA email progress.

## The runtime state-variable manifest (608 variables, byte-direct)

The variable **names** are stored as literal NUL-terminated strings in the
GAM, so the full runtime state namespace can be read byte-direct — **608
distinct variables** in the shipped save (see
[`examples/outputs/gam_state_variables.json`](../examples/outputs/gam_state_variables.json)):

| Type (naming-convention prefix) | Count |
| --- | --- |
| `b…` boolean | 390 |
| `i…` integer | 117 |
| `c…` counter | 31 |
| `s…` string | 4 |
| `n…` id | 2 |
| other | 64 |

They group into meaningful scopes: `IFL_` (110, interface/folder logic),
`Inventory_` (32 items), `NODE_`/`ANODE_` (node state),
`SHANKSN2_/N3_/N6_` (46, the FBI-boss dialogue nodes), `EMAIL_` (sent
flags), `EndGame_` (ending flags), `Pref_` (preferences), and the `bAI*`
AI-scene families (Tarakan, SecretB, Smol, Wong, Warehouse, Comity,
FieldOffice, …).

This is an **independent corroboration** of the game structure decoded from
`XFILES.HDB`: the `bAI*` flags match the trigger system, `SHANKSN*` match
the dialogue nodes, `EndGame_*` the endings, `Inventory_*` the item set.

> **Naming note.** The HDB stores *authoring labels* with spaces
> (`bLink 2`, `cFax Log Translation`); the GAM stores *runtime
> identifiers* without spaces (`bAILink2_*`). They are two views of the
> same state space.
>
### Variable values (now decoded byte-direct)

Each state-variable record stores, relative to the CNeoPersist marker
magic (`00 00 00 01`) that immediately follows the name:

```text
magic+4..+5  flags
magic+6..+7  storage mark (u16)
magic+8      VALUE        (u8 — bool 0/1 or small int)
magic+9      class/type tag (u8)
```

So `decode_gam_values()` reads the full variable state of any save (477
variables in the shipped default; see
[`examples/outputs/gam_values_default.json`](../examples/outputs/gam_values_default.json)).
The value offset is **confirmed two independent ways**:

1. Across several genuinely different real saves, the byte at magic+8 of
   `bAIHauling_GetShovel` flips `0↔1` in lockstep with picking up the
   shovel, and `Inventory_iPhotoSmolnikoff` likewise — the value tracks the
   actual game event.
2. Read over a whole save, **96 % of boolean (`b…`) variables come out
   `0`/`1`** and integer (`i…`) variables come out small — exactly the
   expected shape. A wrong offset could not produce that.

> **Honesty note.** The decoder reports the single value byte (sufficient
> for the boolean / small-int majority that the game uses); wider integers
> may extend into the following bytes, which are exposed raw rather than
> guessed.

### Authoring documentation embedded in the saves

The saves also carry the **authors' own annotations** for many variables —
literal strings, so byte-direct (see
[`examples/outputs/gam_var_docs.json`](../examples/outputs/gam_var_docs.json)):

- **Enum legends** (value meanings) — e.g.
  `cAIFieldOfficeWhereAreWe = {P:Phone, A:APB/Case Files, E:Equipment,
  W:Warehouse, T:Node 3, N:None, D:Done}` (the Field-Office AI scene's state
  machine), `cViewState = {a:default, b:both, d:darkNSA, t:tanNSA}`,
  `cHowIsWillmoreSleeping = {B:like a baby, W:not a wink}`. 18 such legends.
- **Variable descriptions** — `bSuperAgent → "TRUE=All equipment"`,
  `iCurrNode → "+10 for links"`, `Pref_iVolumeMusic → "0-100"`. 32 of them.
- **Condition labels** in plain English — `Has talked to Shanks?`,
  `Is warehouse locked?`, `NSA car spotted?`, `Has W retrieved case files?`.

These come straight from the original data and explain the game-logic
variables and conditions without any inference.

### The symbolic constant table (values byte-direct, proven)

One record family (`01 2b 27 8x`) stores `name → integer value` directly,
and the value is **provable** (not inferred) by an unmistakable
ASCII-mnemonic pattern. This yields the game's symbolic constant /
enumeration table (see
[`examples/outputs/gam_constants.json`](../examples/outputs/gam_constants.json)):

- **Phone keypad** — `IFL_cPhone0..9` = `48..57` = ASCII `'0'..'9'`;
  buttons `Send/End/Clear/Menu/Up/Down/Off` = `'S'/'E'/'C'/'M'/'U'/'D'/'O'`.
- **Font style** — `IFL_cFontColorBlack/White/Courier/AlignLeft/Right/`
  `ShadowYes/No` = `'b'/'w'/'c'/'l'/'r'/'y'/'n'`.
- **Inventory item IDs** — the in-game item enumeration: `iNone=0, iGun=2,
  iPDA=3, iCaseFiles=4, iCattleProd=5, iCrowbar=6, … iFaxLogTranslation=31`.
- **NPC interrogation IDs** — `Amis=0, Arley=1, Stearns=2, Cook=3, Rauch=4,
  Wong=5, …`.
- **Language string IDs** — `English=3998, Spanish=4067, Italian=4069,
  French=4071, German=4073`.

The keypad `0-9 == '0'-'9'` match alone is statistically impossible by
chance, so the value field's meaning is certain for these families. A
handful of `Pref_*`/misc entries on the same field are ambiguous
(index vs value) and are kept in a separate `misc` group, never
presented as decoded.

## Reading a GAM in code

```python
from hdb_extract.extractors.gam_extract import GamFile

gam = GamFile.from_path("/path/to/XFILES.GAM")
assert gam.is_gam()                # version-based check
assert gam.roundtrip_ok()          # parser is correct
print(gam.summary())               # JSON-ready dict
```

Or use the underlying HDB container directly (record walking, marker
analysis, etc., all work the same):

```python
from hdb_extract.container.container import HdbContainer
container = HdbContainer.from_path("/path/to/XFILES.GAM")
for page_id, page in container.iter_pages():
    if page[0] not in (0x00, 0xC0, 0xC2, 0xC3, 0xD2):
        # data page — VC* records live here
        ...
```

## Use cases

- **Save analysis**: diff two saves to see what state changed between
  two checkpoints.
- **Save modification**: edit a variable, re-serialize the GAM, drop it
  back into the game's save folder. (Read-only in v0.2; the
  modification API arrives in v0.3 with the broader write path.)
- **Save preservation**: bundle a save with a fan-port to ship a known
  game state to players.

## CLI usage

```bash
# Quick inspection
python -m hdb_extract gam XFILES.GAM

# Write a JSON summary
python -m hdb_extract gam XFILES.GAM --out=save_summary.json
```

## Note on the trailer

GAM trailers (~120 bytes, non-zero) are **preserved byte-identical**
through the roundtrip but **not semantically decoded** in v0.2. They
appear to encode allocator state — first/last-used page, free list
head, last-modified mark. The structure is small enough that it could
be documented in a single session if anyone needs the per-byte
meaning. See [`ROADMAP.md`](ROADMAP.md) for the open question.
