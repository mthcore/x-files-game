# Game flow — scenes, order, and per-scene mechanics

This is the canonical play order of *The X-Files: The Game*, fused from public
walkthroughs (sequence, objectives, items, puzzle codes) and the **byte-direct**
decode of `XFILES.HDB` / `XFILES.GAM` (per-scene mechanics). The two agree
location-for-location — independent confirmation that the decoded data matches
the real game.

- Structured data: [`examples/outputs/game_flow.json`](../examples/outputs/game_flow.json)
  (29 scene-steps over 6 days, each with actions/items/puzzles + byte-direct
  mechanics) and [`examples/outputs/all_scenes.json`](../examples/outputs/all_scenes.json)
  (per-location phase machine + checkpoints + scoped state variables).
- Model: see [HOTSPOTS.md](HOTSPOTS.md), [TRIGGERS.md](TRIGGERS.md),
  [GAM_FORMAT.md](GAM_FORMAT.md).

Story: FBI agent **Craig Willmore** investigates the disappearance of agents
**Mulder & Scully** near Seattle. Field-office computer login: user
`CRAIG WILLMORE`, password `SHILOH`.

## How a scene works

Each explorable **location** is a small state machine, byte-direct from the
savegame:

- a **phase variable** `cAI<Location>WhereAreWe` — the scene's current step;
- **checkpoint flags** `bAI<Location>_<Action>` — objectives completed inside.

You stand at **viewpoints** (`XV/<id>.HOT` files: clickable rectangles), look
around / move with the directional navigation clips, and click hotspots; a click
plays a clip / opens a conversation / picks up an item / completes a checkpoint.
Completing checkpoints advances the phase; reaching the final phase unlocks the
next location (travelled to via the in-game PDA map). Example —
`cAIFieldOfficeWhereAreWe`: Phone → APB/Case Files → Equipment → Warehouse →
Node 3 → Done.

## Canonical order (6 days, 29 scene-steps)

- **April 2** — Field Office (Cook → desk/phone → Shanks & Skinner) → Comity Inn
  (motel) → Field Office → Dockside Warehouse (+ Wong) → Crime Lab → Field
  Office → Warehouse (night) → Apartment
- **April 3** — Warehouse → Tarakan ship → Coroner → Crime Lab → Field Office →
  Apartment → Warehouse parking (Gordon's Hauling)
- **April 4** — Coroner → Gordon's Hauling → Apartment
- **April 5** — Field Office → Smolnikoff's Warehouse → Crime Lab → Smolnikoff →
  Apartment
- **April 6** — Sand Point Hangar ("X") → Hospital (Scully) → Rail Yard → Field
  Office (Colonel Rauch)
- **April 7** — Rauch's House (Alaska) → Secret Base → end

Each location's byte-direct checkpoints map onto these actions (Warehouse =
collect bullet/blood/powder + crowbar; Tarakan = lead sphere + 2 Russian books;
Hauling = shovel + vent escape; Smolnikoff = gun + clear goons; Secret Base =
isolation chamber + lure Mulder; etc.). Full detail + items + puzzle codes are in
`game_flow.json`.

## What is and isn't recovered here

| Layer | Status |
|---|---|
| Scene order (29 steps, 6 days) | recovered (walkthrough × byte-direct, cross-checked) |
| Per-scene mechanics (phase, checkpoints, variables, views/clips) | byte-direct |
| Items, objectives, puzzle codes | from public walkthroughs |
| Dialogue text | byte-direct (1025 lines, [DIALOGUES.md](DIALOGUES.md)) |
| Exact per-conversation branch wiring | gated on NeoPersist index resolution (see [DIALOGUES.md](DIALOGUES.md)) |

## Sources

- Walkthrough King — <https://www.walkthroughking.com/text/xfiles.aspx>
- GameBoomers — <https://www.gameboomers.com/wtcheats/pcXx/XfilesWT1.htm>
- GameFAQs — <https://gamefaqs.gamespot.com/pc/37002-the-x-files-game/faqs/47902>
- Byte-direct decode of `XFILES.HDB` / `XFILES.GAM` (this project).
