# Changelog

## 0.3.x — in development

- **SDL2 playable shell** (`xfiles_play`), opt-in via `cmake -DXFILES_PLAY=ON`.
  Opens a 640x480 window, plays a Cinepak/IMA scene from `XV/<scene_id>.xmv`,
  overlays its HSPT rects, dispatches the rect's `action_id` on click.
  Build flag-gated — when OFF, no SDL2 fetch happens, the existing build
  matrix is untouched.
- `--location <name>` resolves a location string to its byte-direct
  interactive scene_id via `scene_asset_map.json`.
- SPACE / BACKSPACE walk the canonical 29-step flow (`flow.days[].scenes[]`)
  byte-direct; each step loads its scene's FMV + HOT and fires the
  location's triggers via the shared headless dispatcher.
- F5 / F9 save / load the variable state to / from `xfiles_play.save.json`
  (full round-trip; the new `test_dispatcher_save_load` test locks it).
- New byte-direct data artifacts:
  `examples/outputs/scene_asset_map.json` (1 439 entries, 20 locations),
  `examples/outputs/navigation_targets.json` (68 VCNav records, 14 labelled),
  `examples/outputs/scenes_with_hotspots.json` (603 interactive, 835
  cinematic-only, 77 orphan HOT files).
- HSPT parser, mini JSON reader, and headless dispatcher extracted into
  `cpp/include/runtime/` so the engine and the playable shell share one
  byte-direct implementation. Three new CLI commands:
  `hdb-extract scene-map / nav-targets / scenes-with-hotspots`.
- `--probe` mode loads every artifact, reports state, exits 0 before
  `SDL_Init` — safe for headless CI. New tests:
  `test_scene_resolver`, `test_play_probe`, `test_dispatcher_save_load`.
- Unified game model (`hdb_extract game-def`) and C++ engine validator
  (`xfiles_engine --validate-flow`). Reports byte-direct PASS, walkthrough-only,
  and FAIL per step (18/29 PASS, 11 walkthrough-only, 0 FAIL on the shipped HDB).
- Headless dispatcher in the engine: fires each step's triggers, parses every
  byte-direct `effect_summary`, mutates a variable-state map. 113 effects
  applied, 48 distinct variables set, ≥85% overlap with the GAM namespace
  (cross-validated by integration test).
- Scene state-machine simulator: per-location phase variable + checkpoints
  fired round-robin as the canonical flow visits each scene.
- `--print-trace` mode reads `playthrough_trace.json` and emits a readable
  per-step summary (action text, top triggers, dialogue line samples).
- `--json-out` exports the validator + state machine + dispatcher report.
- `hdb_extract trace` produces `playthrough_trace.json` cross-referencing
  every step with its triggers, conversations, and dialogue lines.
- `hdb_extract hotspots` decodes every `XV/*.HOT` (HSPT) file: 680 scenes,
  2 279 rects, 807 distinct action ids, with a per-action_id frequency ranking.
- `btree_pages_inventory.json` surfaces the (tag, kind) statistics for every
  page; per-kind semantics stays honestly `undetermined`.
- Master resolver covers NeoIDList / NeoIDIndex pair tables;
  `classify_handle` returns `neoid` / `direct-offset` / `unknown`.
- VCConversation extractor walks the on-disk list container; dialogue text
  resolved against `XFilest.dll`.
- VCStdAction byte-direct vocabulary (opcode → verb stays undetermined — not
  in the public format).
- Empirical CNeoPart `+14` relation byte distribution surfaced in
  `docs/TRIGGERS.md`.

## 0.3.0

- PICT decoder via vendored `stb_image.h`.
- `hdb_extract merge-locale` joins the decision tree against the
  `XFiles{c,e,s,t}.dll` string tables.
- MSVC build path: layout asserts macro-wrapped, write path with round-trip
  on scalar specs.
- PyInstaller release workflow, MkDocs Material site.

## 0.2.0

- PFF / GAM / localization DLL / QuickTime MOV extractors
  (`hdb_extract pff|gam|loc|mov`).
- C++ video decoders: Cinepak, IMA ADPCM, QuickTime container.
- Catch2 vendored; 12 unit tests ported.

## 0.1.0

- Initial release. NeoPersist 3.0 container, 51/51 VC* classes byte-exact,
  100% byte-direct coverage on `XFILES.HDB`.
- CLI: `inspect`, `audit`, `verify`, `extract`, `dump-class`, `dump-complete`.
- C++17 decoder, 50 tests.
