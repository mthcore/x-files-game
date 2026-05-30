# ENGINE.md — the reference game engine (`xfiles_engine`)

`cpp/src/engine/xfiles_engine.cpp` is a small, self-contained **reference game
engine** for *The X-Files: The Game*. It is a "pseudo-engine": it
shows, end to end, **how the game is stored and how it runs**, without any of the
original graphics/video code. Give it `XFILES.HDB` (plus the sibling `XV/*.HOT`
files) and it rebuilds the game's object model and demonstrates the runtime loop.

```
xfiles_engine /path/to/XFILES.HDB  [/path/to/XV]
```

It is the practical companion to the format docs: read the code top-to-bottom and
you have a complete picture of the data and the game logic.

## What it does, in 7 parts (matching the source)

1. **Read the HDB container.** `XFILES.HDB` is a single ~6 MB file holding the
   entire game. It is a grid of 256-byte pages; data pages hold records on an
   8-byte grid. Each record begins with an 8-byte **CNeoPersist marker**
   (`flags1`, `sub_tag`, `00 00 00 01` version, then a big-endian **class_id** at
   +6..+7). The payload runs to the next marker. See [FORMAT.md](FORMAT.md).

2. **Read fields.** Integers are big-endian. The longest printable run inside a
   record is its inline **label** — a self-describing string baked in by the
   authoring tool, e.g. `Video:Node 1:Field Office:Shanks "Come in" …`. It
   carries the scene, character and dialogue, read straight from the bytes.

3. **CNeoPart (trigger logic).** A trigger's conditions are 16-byte CNeoPart
   sub-records: `ref_a/class_a` (operand A), `ref_b/class_b` (operand B), and a
   `relation` discriminator. Two typed operands = a **binary relation**; one
   operand = a **mono-target** reference. See [TRIGGERS.md](TRIGGERS.md).

4. **The object model.** The VirtualCinema classes (`class_id` 0x27..0x5C):
   `VCNode` (story moment), `VCLocation` (place), `VCCharacter` (NPC),
   `VCConversation` (dialogue), `VCNav` (navigation link), `VCHotSpot`
   (clickable region), `VCTrigger` (conditions→actions→targets), `VCAction` /
   `VCStdAction`, `VCVariable`, `VCGameState`, `VCEmail`. See [CLASSES.md](CLASSES.md).

5. **Load the game.** One pass over the records: index by class, harvest scene
   labels (a *scene-moment* is a distinct `Node N / Location` — the same place
   at different Nodes is a different scene), and decode the triggers. Hotspot
   **geometry** is then loaded from the `XV/<scene>.HOT` files (the `HSPT`
   format: a header plus 20-byte rectangle entries, screen-space 640×480, each
   carrying its `action_id`s). See [HOTSPOTS.md](HOTSPOTS.md).

6. **The game loop.** The runtime model, demonstrated headlessly:

   ```
   for each story Node (1..N), in order:
     present the scene-moment (play its media asset, draw the backdrop)
     loop:
       hit-test the cursor against the scene's hotspot rects (top z-order wins)
       on click: dispatch the hotspot's action_id (play video / open
                 conversation / pick up item / navigate via a VCNav / …)
       after a state change: evaluate the scene's triggers —
         if a trigger's condition parts hold, apply its actions to the
         objects it `affects`
       a VCNav action changes the current Node/Location → next scene-moment
   ```

7. **The scene state machine.** Each location is a small state machine: a phase
   variable `cAI<Location>WhereAreWe` plus checkpoint flags
   `bAI<Location>_<Action>` (byte-direct from the savegame). Completing
   checkpoints advances the phase; the final phase unlocks the next location.
   This is how the game gates progression and fixes the scene order. The
   `Conversation` struct (VCConversation grammar) is modelled here too. See
   [GAME_FLOW.md](GAME_FLOW.md) and [DIALOGUES.md](DIALOGUES.md).

## Honest boundary

Everything the engine prints is read directly from the data files. Two
runtime-internal details are **not** recoverable from the files alone and are
reported but never invented:

- the **relation operator** byte (`relation` in a CNeoPart) is exposed raw;
- the **inter-object handles** (`ref_a`/`ref_b`, and hotspot `action_id`
  targets that do not match a record) are exposed raw, marked unresolved.

These are computed at load time by the original engine from in-memory state, so a
faithful re-implementation supplies them from its own object store — the
structure, classes, geometry, dialogue, flow and wiring around them are all here.

## Scripted-playthrough validator (`--validate-flow`)

The engine accepts the **unified honest model**
(`examples/outputs/game_definition.json`) and walks the canonical 29-step flow
in `flow.days[].scenes[]`:

```
xfiles_engine --validate-flow <game_definition.json> [<XFILES.HDB>]
```

For each step, it classifies the outcome as one of:

- `PASS` — the step's `location` is in the byte-direct `scenes{}` catalog,
  every trigger the catalog references resolves in the global `triggers[]`
  list, and the step has at least one walkthrough action.
- `WALK` — the step's location is not in the byte-direct catalog (5 scenes
  in the shipped game: `Crime Lab`, `Apt`, `Coroner`, `Hangar`, `Hospital`),
  so it is honestly surfaced as walkthrough-only — counted toward coverage
  but flagged.
- `FAIL` — actions missing, or a referenced trigger does not resolve.

A second pass simulates the byte-direct **state machine** of each scene:
its phase variable (`cAI<Location>WhereAreWe`), the ordered `phase_machine`
values, and the checkpoint set are fired round-robin as the player visits
the scene across steps. Final tally:

```text
scripted playthrough: 18/29 byte-direct PASS, 11 walkthrough-only, 0 FAIL

State-machine simulation:
  Field Office   phase 1/7 (P:Phone)  checkpoints 6/8
  Comity         phase 1/3 (N:None)   checkpoints 1/5
  ...
```

Exit code is `0` when there are no `FAIL` steps, `1` otherwise.

## Trace pretty-printer (`--print-trace`)

```text
xfiles_engine --print-trace <playthrough_trace.json>
```

Reads the per-step trace produced by `hdb-extract trace`
(`examples/outputs/playthrough_trace.json`) and prints a readable
step-by-step summary: walkthrough action text, top triggers with their
`effect_summary`, and dialogue line samples reachable through the
location's scope. No HDB required — the trace is self-contained.

Both modes embed a small in-tree JSON reader (`namespace json` inside
`xfiles_engine.cpp`); no external dependency is added.

## Build & run

```bash
cd cpp && cmake -S . -B build && cmake --build build
./build/xfiles_engine /path/to/XFILES.HDB /path/to/XV
./build/xfiles_engine --validate-flow examples/outputs/game_definition.json
./build/xfiles_engine --print-trace examples/outputs/playthrough_trace.json
```
