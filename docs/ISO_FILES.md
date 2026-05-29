# ISO files — what to copy from the game

This decoder operates on the files shipped with the **retail X-Files: The Game (1997)** CD-ROM set. Before running any extractor, copy the game files from your legitimate installation or ISO to a directory on your machine.

The decoder **never asks you to upload** any of these files anywhere; everything runs locally.

## Default install layout (Windows)

After running the retail installer, the game lives under (default path):

```
C:\Program Files\Hyperbole Studios\The X Files Game\
```

(Or `C:\Program Files (x86)\...` on 64-bit Windows.) Copy the entire installed directory — about **3.1 GB** — to a working folder, e.g. `~/xfiles/game/`.

## What each part of the game is

The game splits into three kinds of files: **one data file** that holds all the
logic, a set of **media archives/clips** grouped into `X*` directories by role,
and a few **support files** (fonts, runtime, localization).

- **`XFILES.HDB` — the brain.** Everything that is not a picture or a sound: the
  scenes, the navigation graph, hotspots, triggers, conditions, actions,
  variables, conversations, the e-mail arc and the endings. One file, fully
  decoded. `XFILES.GAM` is a savegame in the same format (it holds the runtime
  variable values).

- **The `X*` media directories — the eyes and ears.** Each clip/asset is named
  by a numeric id that the HDB references. The leading letter groups them by
  role:
  - **`XV/` — Views.** The bulk of the game: one `.XMV` video per scene-moment
    plus one `.HOT` file per scene giving the clickable rectangles. This is what
    you see and click on. (~1300 videos + ~680 hotspot files.)
  - **`XN/` — Navigation.** The `.XMV` transition clips played when you move
    between viewpoints/locations. (~300 clips.)
  - **`XG/` — Graphics / global.** Interface and shared assets: a mix of `.XMV`
    clips and `.PFF` image archives (icons, overlays).
  - **`XS/` — Sound.** Audio-only clips: `.AMV` (ambient), `.DMV` (dialogue) and
    `.MUS` (music) — all QuickTime audio tracks.
  - **`XT/` — Text.** The PDA content as plain text: e-mails, Willmore's
    journal, in-game articles and UI help. (474 `.xtx` files.)

- **`NAV*.NMV` (top level) — the panoramic navigation movies** used for the
  360°-style look-around scenes.

- **Support files.** `XFiles{c,e,s,t}.dll` hold the localized UI strings; the
  `.TTR` files are the game's TrueType fonts (dialogue, journal, phone, HUD),
  each paired with a tiny `.FOR` loader stub; `X.PFF`/`X7.PFF` are the main PICT
  image archives; `QuickTime.qts`/`QuickTimeVR.qtx` are Apple's bundled
  QuickTime runtime (not game data); the game executable, an icon, the readme
  and the uninstaller round out the install.

The mnemonic letters (V = views, N = navigation, G = graphics, S = sound,
T = text) describe what each directory contains; the numeric file names are the
ids the HDB uses to pull the right asset for each scene.

## Top-level files

| File          | Size    | Role                          | Decoder                      |
|---------------|--------:|-------------------------------|------------------------------|
| `XFILES.HDB`  | ~6 MB   | Hyperbole DataBase (game data) | `python -m hdb_extract` (v0.1) |
| `XFILES.GAM`  | varies  | Savegame (same container)     | `python -m hdb_extract gam`   |
| `X.PFF`       | varies  | PICT archive (assets)         | `python -m hdb_extract pff`   |
| `X7.PFF`      | varies  | PICT archive (assets, alt)    | `python -m hdb_extract pff`   |
| `NAV1.NMV` … `NAV7.NMV`, `NAVM.NMV` | ~120 MB total | Navigation videos (QuickTime MOV) | `python -m hdb_extract mov` |
| Game executable  | ~3.3 MB | Game executable               | not decoded     |
| `XFilesc.dll`, `XFilese.dll`, `XFiless.dll`, `XFilest.dll` | varies | Localization strings (Windows PE resources) | `python -m hdb_extract loc` |
| `DLG.TTR`, `JRN.TTR`, `PHN.TTR`, `HCD.TTR` | 40–110 KB | TrueType fonts (dialogue, journal, phone, HUD) — `sfnt` magic `00 01 00 00` | not decoded — open as `.ttf` |
| `Dlg.FOR`, `Jrn.FOR`, `Phn.FOR`, `Hcd.FOR` | ~1.4 KB | Small MZ loader stub paired with each `.TTR` font | not decoded |
| `QuickTime.qts`, `QuickTimeVR.qtx` | varies | Apple QuickTime runtime (bundled) | not decoded — not game data |
| `Jeu.ico`, `Readme.rtf`, `Lisez-Moi.txt`, `unins000.*`, `Uninst.isu` | small | Icon, readme, uninstaller | not decoded |
| `ddraw.dll`, `msvcr80.dll`, `msvcm80.dll`, `msvcp80.dll` | varies | Runtime support (DirectDraw + MSVC redist) | not decoded — system DLLs |

## Subdirectories

| Dir   | # files | Total size | Role                                  | Decoder                     |
|-------|--------:|-----------:|---------------------------------------|-----------------------------|
| `XG/` |     83  | 18 MB      | Graphics / global UI assets (64 XMV + 19 PFF) | `mov` + `pff`         |
| `XN/` |    309  | 183 MB     | Navigation transition clips (XMV)     | `mov`                       |
| `XV/` |  1 982  | 2.8 GB     | Views: 1 302 scene videos (XMV) + 680 hotspot files (HOT) | `mov` + `hot` |
| `XS/` |    169  | 88 MB      | Sound: 99 ambient (AMV) + 9 dialogue (DMV) + 61 music (MUS) | `mov` (audio MOV) |
| `XT/` |    474  | 676 KB     | Text scripts (emails, notes, articles) | `python -m hdb_extract xt` |

## Per-extension reference

| Extension | Container        | Common payload codec                | Files |
|-----------|------------------|-------------------------------------|------:|
| `.HDB`    | NeoPersist 3.0   | structured records (51 VC* classes) | 1     |
| `.GAM`    | NeoPersist 3.0   | savegame (same container as HDB)    | 1     |
| `.PFF`    | NeoLogic PICT    | PICT images + sub-records           | many  |
| `.NMV`    | QuickTime MOV    | Cinepak video (~79%), MJPEG, RPZA   | 8     |
| `.XMV`    | QuickTime MOV    | Cinepak video (scene/nav/UI clips)  | 1 675 |
| `.AMV`    | QuickTime MOV    | IMA ADPCM audio (ambient)           | 99    |
| `.DMV`    | QuickTime MOV    | IMA ADPCM audio (dialogue)          | 9     |
| `.MUS`    | QuickTime MOV    | IMA ADPCM audio (music)             | 61    |
| `.HOT`    | `HSPT`           | hotspot rectangles + action ids     | 680   |
| `.xtx`    | text             | plain text (emails, journal, …)     | 474   |
| `.dll`    | Windows PE       | localized strings in `RT_STRING`    | 4     |
| `.TTR`    | TrueType `sfnt`  | game UI fonts (rename to `.ttf`)    | 4     |
| `.FOR`    | MZ stub          | per-font loader stub                | 4     |
| `.qts`/`.qtx` | QuickTime    | Apple QuickTime runtime (not data)  | 2     |

## Validation (recommended)

To make sure your copy of the files matches the **retail US/EU 1997 release**, run:

```bash
python -m hdb_extract verify /path/to/XFILES.HDB
# container roundtrip byte-identical : True
```

If `verify` reports a mismatch, your file is either a pirate copy with extra bytes, a corrupted download, or a different regional release (the decoder still works structurally, but byte-direct claims need re-validation).

We do not ship a master SHA-256 table; if you want to contribute one, please open a `format-finding` issue with the hash and the disk-label string of your physical copy.

## What this project does NOT need

The X-Files game also includes optional content not used by the decoder:

- Install splash bitmaps (`SETUP.BMP`, etc.) — purely cosmetic.
- DirectDraw redistributable installer (`ddrawsetup.exe`).
- Manual / readme PDFs that may have been bundled.

You can ignore those when copying.

## Permanent reminder

The game files are **copyrighted by Fox Interactive / HyperBole Studios**. The decoder published here is independent format-analysis output for the purpose of interoperability (see top-level `NOTICE`). Do **not** redistribute the game files. Do **not** attach them to issues. Do **not** push them to your fork.
