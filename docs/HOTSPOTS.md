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

## Hotspot index

A separate inventory CSV exists in the curated format-analysis
artifacts:

```
scene_id, hotspot_idx, type, x_min, y_min, x_max, y_max, action_id1, action_id2
   1     ,     0     ,  4  ,  120 ,  340 ,  280 ,  410 ,    14     ,    15
   1     ,     1     ,  4  ,  300 ,  340 ,  460 ,  410 ,    16     ,    17
   ...
```

This index was documented byte-direct from the format; the values for each
hotspot in the HDB can be cross-referenced against it. See the
hotspots inventory (in the upstream research tree) for the full
2 278-row inventory.

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
