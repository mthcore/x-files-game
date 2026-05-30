# Hotspots — clickable UI regions

A **hotspot** is a rectangular region on-screen that the player can
click. Each hotspot is associated with up to two **action ids** that fire
when clicked or hovered. The X-Files HDB defines **2 278 hotspots**.

## Records

Hotspots are persisted as `VCHotSpot` records (class_id `0x39`),
typically grouped into `VCHotSpotList` (class_id `0x3A`) collections per
scene, with a system-wide `VCStdHotSpotList` (class_id `0x3B`) for
ambient interactive elements (PDA buttons, menu items).

## VCHotSpot byte-direct grammar

After the 8-byte CNeoPersist marker:

```
+8   SubObj (polymorphic)   ← geometry (rectangle: 4 × i16 packed)
+...  Dword 'f'              ← action_id (primary click action)
```

The polymorphic sub-object usually resolves to a `VCAssetRef` carrying
the four screen-space coordinates and the asset reference used for
hover-state graphics.

## HSPT files (`XV/*.HOT`)

The on-disk geometry lives in sibling `XV/<scene_id>.HOT` files in the
`HSPT` container format (1 982 files in the shipped game).

```
header  : "HSPT" (4 B) + count (u32 LE, 4 B)
entry   : type_zorder (u32 LE, 4 B)
          x_min, y_min, x_max, y_max (i16 LE × 4)
          action_id_1 (u32 LE, 4 B)
          action_id_2 (u32 LE, 4 B)
          total = 20 B per entry
```

A strict size invariant (`8 + n*20`) is enforced. Coordinates are
screen-space 640×480.

Python decoder:

```python
from hdb_extract.extractors.hotspots import (
    parse_hot_file, build_hotspots_inventory, write_hotspots_inventory,
)
stats = write_hotspots_inventory("/path/to/XV", "hotspots_inventory.json")
```

The bundled output [`examples/outputs/hotspots_inventory.json`](../examples/outputs/hotspots_inventory.json)
contains every parsed scene plus an `action_ids_by_frequency` ranking
(680 scenes, 2 279 rects, 807 distinct action ids). The top action ids
in the shipped game appear on 26 – 32 different scenes.

## action_id semantics

`action_id` is exposed **raw**. The mapping from `action_id` to a
runtime effect (which video to play, which variable to set, which scene
to navigate to, ...) is not in the public on-disk format: it is
resolved at runtime by the original engine from in-memory state. A
faithful re-implementation supplies this mapping from its own object
graph; the bundled engine reports each `action_id` it would dispatch
but does not invent semantics for it.

## Event flow

```
mouse click
   ↓
hit-test against active hotspots in current scene
   ↓
match → emit EVT_CLICK with hotspot.action_id
   ↓
trigger system: any VCTrigger with a matching condition fires its actions
   ↓
side effects: variable update, scene change, video playback, ...
```

`VCStdHotSpotList` is checked **before** scene-specific hotspots, so
PDA / inventory buttons take precedence over in-scene clickables.

## Hover state

Hovering a hotspot fires `EVT_HOVER`, which conventionally:
- Updates the cursor sprite (via `VCCursor` / `VCDefaultCursors`).
- Optionally plays a tooltip audio cue.
- Does **not** advance the game state.

## Extracting hotspots

```bash
python -m hdb_extract extract /path/to/XFILES.HDB --out=out/
# Look for VCHotSpot records in the all-records dump:
jq '.[] | select(.["__class__"] == "VCHotSpot") | .["__class_id__"]' out/json/decoded_records.json | head
```

## See also

- [TRIGGERS.md](TRIGGERS.md) — how `EVT_CLICK` is routed.
- [CLASSES.md](CLASSES.md) — the full class catalog including
  `VCHotSpot`, `VCHotSpotList`, `VCStdHotSpotList`, `VCAssetRef`.
- [FORMAT.md](FORMAT.md) — on-disk record layout.
