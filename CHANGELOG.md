# Changelog

## 0.3.x — in development

- Unified game model (`hdb_extract game-def`) and C++ engine validator
  (`xfiles_engine --validate-flow`). Reports byte-direct PASS, walkthrough-only,
  and FAIL per step.
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
