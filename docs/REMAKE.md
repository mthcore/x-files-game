# How it works, how to use it, and what a full rebuild takes

This page is the in-depth companion to the per-format references. It explains
the project end to end: the data model, the decoding pipeline, every way to
drive it, the reference engine, and — honestly — the work still required to
rebuild *The X-Files: The Game* as a playable game.

Everything here is read from the game's own data files. The project owns no game
content; you supply your own legal copy of the files.

---

## 1. The big picture

*The X-Files: The Game* (1997) is a full-motion-video adventure. Its
entire game state — scenes, navigation, hotspots, dialogue, triggers, actions,
NPC conversations, the e-mail arc and the decision graph — lives in a single
file, `XFILES.HDB`, serialized with NeoLogic's **NeoPersist 3.0** library (used
by HyperBole's **VirtualCinema** engine). The media (video, stills, fonts, text)
lives alongside it in `.PFF` archives, QuickTime `.xmv/.nmv/.amv/.dmv` movies,
`.xtx` text files and the localization DLLs.

This project decodes all of that **byte-direct**: every byte of `XFILES.HDB` is
classified and re-serializable to a byte-identical copy, and the other ISO
formats are parsed with verified round-trips. On top of the decoder sits a small
**reference engine** that rebuilds the game's object model and demonstrates the
runtime loop without any of the original rendering code.

What you get is the complete, factual blueprint of the game's logic and content
— the raw material a faithful remake needs.

---

## 2. How it works — the data model

### 2.1 The container

`XFILES.HDB` is a 32-byte header, a grid of **256-byte pages**, and a trailer.
Data pages (`flags1` in `0xC0..0xCF`) hold records on an 8-byte grid. Every
record opens with an 8-byte **CNeoPersist marker**:

```text
+0  flags1   (0xC0..0xCF)        ┐ packed flags (big-endian u16)
+1  sub_tag                      ┘
+2  00 00 00 01                  version
+6  class_id (big-endian u16)    the record's class
```

The payload runs from `marker+8` to the next marker. See
[FORMAT.md](FORMAT.md) for the page grid, B-tree pages and trailer.

### 2.2 The classes

`class_id` `0x27..0x5C` are the VirtualCinema (`VC*`) classes — the game
objects: `VCNode` (a story moment), `VCLocation`, `VCCharacter`,
`VCConversation`, `VCNav` (a navigation link), `VCHotSpot`, `VCTrigger`,
`VCAction`/`VCStdAction`, `VCVariable`, `VCGameState`, `VCEmail`, and so on.
`0x00..0x0B` are NeoPersist system classes (index, free-list, part manager).
Class names come from the registry the HDB stores in its own
`NeoClass`/`NeoSubclass` records — byte-direct. Each class has an exact
`Read()` grammar; see [CLASSES.md](CLASSES.md).

### 2.3 The trigger graph (the game logic)

A `VCTrigger` carries pairs that point at sub-records by **direct byte offset**
(plus a small alignment adjustment). Those sub-records are **CNeoPart** blobs
(class ids `0x10..0x1d`, managed by `CNeoPartMgr` `0x0a`). Each is a 16-byte
payload: operand A (`ref_a`/`class_a`), operand B (`ref_b`/`class_b`), and a
`relation` discriminator at `+14`. Two typed operands = a binary relation; one
operand = a mono-target reference. The trigger pair graph resolves byte-direct
(~96%). See [TRIGGERS.md](TRIGGERS.md).

> The `relation` byte's meaning, and operand handles that are node-relative
> rather than direct offsets, are computed at run time from in-memory state and
> are **not stored in the file**. They are exposed raw and marked
> `undetermined`, never guessed. This is the one genuine boundary of a
> file-only reconstruction — see §6.

### 2.4 Hotspot geometry

Clickable regions live in `XV/<scene>.HOT` files (the `HSPT` format: a header
plus 20-byte rectangle entries in screen space, each carrying its
`action_id`s). Round-trip byte-identical on all scenes. See
[HOTSPOTS.md](HOTSPOTS.md).

### 2.5 Runtime state — the savegame

`XFILES.GAM` is the same NeoPersist container (version 5). It stores the game's
**runtime variables** with their values. Each variable record holds, relative
to its marker magic: a storage mark, then the **value byte at magic+8**, then a
type tag. Booleans read 0/1; integers read small; the value cell is confirmed by
cross-save comparison (e.g. `bAIHauling_GetShovel` flips 0↔1 with picking up the
shovel). The saves also embed the authors' own annotations — enum legends like
`cAIFieldOfficeWhereAreWe = {P:Phone, A:APB/Case Files, E:Equipment,
W:Warehouse, …}` and plain-English condition labels. See
[GAM_FORMAT.md](GAM_FORMAT.md).

### 2.6 Media and text

- `.PFF` — PICT archives (round-trip identical). [PFF_FORMAT.md](PFF_FORMAT.md)
- `.xmv/.nmv/.amv/.dmv` — standard QuickTime MOV (Cinepak / MJPEG / IMA ADPCM).
  [VIDEO.md](VIDEO.md)
- `XFiles{c,e,s,t}.dll` — Windows PE resource strings (localization).
  [LOCALIZATION.md](LOCALIZATION.md)
- `XT/*.xtx` — plain-text PDA content (e-mails, journal, articles).
  [XT_SCRIPTS.md](XT_SCRIPTS.md)

---

## 3. How to use it

You need your own copy of the game files. Point the tools at them; nothing is
bundled.

### 3.1 Python CLI

```bash
cd python
pip install -e .

hdb-extract inspect        /path/to/XFILES.HDB     # header, page/class histogram
hdb-extract verify         /path/to/XFILES.HDB     # byte-direct round-trip check
hdb-extract audit          /path/to/XFILES.HDB     # coverage report
hdb-extract extract        /path/to/XFILES.HDB --out=out/   # JSON + SQLite + per-scene
hdb-extract dump-class VCTrigger /path/to/XFILES.HDB --limit=3
hdb-extract dump-complete  /path/to/XFILES.HDB     # full classified dump (+ verify)

# Other ISO formats
hdb-extract gam  /path/to/XFILES.GAM               # savegame variables + values
hdb-extract pff  /path/to/X.PFF --out=out/pff/
hdb-extract mov  /path/to/XV/17923.xmv --out=out/clip.mov
hdb-extract loc  /path/to/XFilese.dll --out=out/strings_en.json
hdb-extract merge-locale out/json/decision_tree.json XFiles*.dll --out=out/decision_tree_localized.json
```

`extract` writes the full set: `game_definition.json` (the canonical blueprint —
scenes, triggers, hotspots, classes, plus `gam_runtime` state when a sibling
`XFILES.GAM` is present), `decision_tree.json`, per-scene JSON, readable scene
scripts, the SQLite database, and the coverage report.

### 3.2 Python API

```python
from hdb_extract.container.container import HdbContainer
from hdb_extract.btree.record_index import HDBRecordIndex
from hdb_extract.extractors.game_definition import build_game_definition

defn = build_game_definition("XFILES.HDB")     # auto-loads a sibling XFILES.GAM
for tg in defn["triggers"]:
    ...                                          # every pair byte-direct
```

Every emitted element carries a `certainty` field — `byte-direct` or
`undetermined` (with a documented reason; never guessed).

### 3.3 C++ decoder

```bash
cd cpp && cmake -S . -B build && cmake --build build
./build/hdb_dump /path/to/XFILES.HDB             # opens the container, prints a summary
ctest --test-dir build                           # decoder unit tests
```

The C++ side mirrors the Python decoder (container, record index, class readers,
serializer) and includes the byte-validated video decoders (Cinepak, IMA ADPCM,
QuickTime container, PICT).

---

## 4. The reference engine (the remake starting point)

`cpp/src/engine/xfiles_engine.cpp` is a small, self-contained **reference
engine**. Give it `XFILES.HDB` (and the sibling `XV/*.HOT`) and it rebuilds the
game's object model and demonstrates the runtime loop headlessly — no graphics
or video, just the structure and flow:

```bash
./build/xfiles_engine /path/to/XFILES.HDB /path/to/XV
```

The loop it models:

```text
for each story Node (1..N), in order:
  present the scene-moment (its media asset + backdrop)
  loop:
    hit-test the cursor against the scene's hotspot rects (top z-order wins)
    on click: dispatch the hotspot's action_id
              (play video / open conversation / pick up item / navigate)
    after a state change: evaluate the scene's triggers; if a trigger's
              condition parts hold, apply its actions to what it affects
    a VCNav action changes the current Node/Location -> next scene-moment
```

Read [ENGINE.md](ENGINE.md) for the six-part walkthrough. This engine is the
skeleton a full remake fleshes out: the data, the object model, the wiring and
the loop are all here; what is missing is presentation (rendering, video, audio,
input) and the run-time-only evaluation noted in §6.

---

## 5. What is already solved

- **Container**: 100% byte-direct, byte-identical round-trip.
- **Classes**: every VC* class has an exact read grammar.
- **Flow**: ordered story Nodes + navigation links, byte-direct.
- **Triggers**: the pair graph resolves ~96% byte-direct; conditions/actions
  decoded structurally.
- **Hotspots**: every scene's clickable rectangles + action ids, byte-direct.
- **Runtime state**: savegame variable values + the authors' enum legends and
  condition labels, byte-direct.
- **Media/text**: PFF, QuickTime video, localization strings, PDA text — all
  parsed with verified round-trips.

That is the complete content-and-logic blueprint of the game.

---

## 6. What it takes to fully rebuild X-Files

A faithful, playable remake needs the following on top of this blueprint. The
items are ordered roughly by effort and grouped by whether the project already
gives you the data.

### 6.1 Presentation layer (data is ready; engineering remains)

1. **Video playback.** The clips are standard QuickTime (Cinepak / MJPEG / IMA
   ADPCM); the decoders are here and byte-validated. A remake must wire them to
   a frame pump + audio sync and play the right clip per scene-moment. The
   scene→asset map gives the clip for each moment.
2. **Backdrops and stills.** The PFF/PICT decoder yields the images. A remake
   composites them with the hotspot layer.
3. **Navigable 360° views.** Some scenes are panoramic (`NAV*.nmv`). The frames
   decode; the panorama projection + look-around input is engineering work.
4. **Audio.** IMA ADPCM tracks decode; mixing, music and ambient loops must be
   wired.
5. **Input + UI.** Cursor, the PDA/inventory interface, the phone keypad and the
   font/style system are all described by data (hotspots, `IFL_*` constants, the
   enum legends), but the widgets must be built.

### 6.2 Run-time evaluation (the genuine file boundary)

The original engine computes two things at run time from in-memory state that
are **not stored in the data files**, so a remake supplies them from its own
object store rather than reading them out:

6. **The CNeoPart `relation` operator** (`+14`). The file stores the operand
   pair and a raw discriminator; the comparison/relation it expresses is decided
   at run time. A remake defines the operator set its conditions use.
7. **Node-relative operand handles.** Most references resolve byte-direct, but
   some operand handles are node-relative rather than direct offsets. A remake
   resolves them through its own loaded object graph (it has every object in
   memory, exactly as the original did).

These are not gaps in the *content* — every object, value and wire is decoded —
they are the two places where a re-implementation provides the run-time
semantics instead of reading them from disk.

### 6.3 The action vocabulary (data-driven; semantics to implement)

8. **Action effects.** Each hotspot/trigger action carries an `action_id` and
   typed arguments (`VCStdAction`), byte-direct. A remake implements what each
   action *does* (play clip, set variable, open conversation, navigate, grant
   inventory, send e-mail). The savegame's enum legends and condition labels
   document the intended behaviour of the scene state machines (the `cAI*…`
   variables), which makes this implementation concrete rather than guesswork.
9. **Conversation flow.** `VCConversation` records give the dialogue graph
   (prompt/response ids, next-conversation links). A remake drives it and binds
   the response ids to the localized strings (the `loc` + `merge-locale`
   commands produce the text join).
10. **E-mail / decision arc.** The 323-message arc and the ending conditions are
    decoded; a remake schedules e-mails on state changes and selects the ending.

### 6.4 Structural completeness (optional, for purists)

11. **B-tree internal pages.** The `0xC2`/`0xD2` pages are decoded structurally
    (every byte classified, round-trip identical) but not per-kind
    semantically. A remake does not need this — it reads records by offset — but
    closing it would make the index fully self-describing. See
    [FORMAT.md §6](FORMAT.md) and [ROADMAP.md](ROADMAP.md).

### 6.5 Suggested build order for a remake

1. Load `game_definition.json` into an in-memory object graph (scenes,
   navigation, hotspots, triggers, variables, conversations, e-mails).
2. Stand up presentation: play the per-scene clip, draw the backdrop, overlay
   hotspots, handle clicks. (You now have a clickable, navigable slideshow.)
3. Implement the action vocabulary (§6.3) so clicks have effects and the state
   machine advances.
4. Add the condition/relation evaluation (§6.2) against your object graph so
   triggers fire and gate progression.
5. Layer in conversations, the PDA/e-mail arc, audio and the panoramic views.
6. Tune against the savegame data: load a real save, confirm your variable
   state matches, and walk the decision arc to an ending.

At that point you have a faithful, data-driven rebuild whose every scene,
hotspot, line of dialogue, variable and flow edge traces back to the original
files — and whose only invented parts are the presentation engine and the two
run-time evaluation rules, which the original computed in memory too.
