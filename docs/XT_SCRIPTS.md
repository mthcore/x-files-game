# XT scripts — text content for the PDA, emails, journals

The `XT/` directory in the X-Files install contains **474 `.xtx` files**
(total 676 KB). Despite the `.xtx` extension and the project codename
"XTLanguage", these files are **plain text** — UTF-8 or UTF-16LE
depending on the entry — and contain the narrative content that the
in-game PDA presents to the player.

## Content breakdown

Counts measured against the retail release (verified during v0.1 RE
work):

| Category                | Count | Notes |
|-------------------------|------:|-------|
| Emails (Mulder / Scully inbox)   | 345 | The bulk of in-game reading material |
| Willmore field journal entries   |  42 | Multi-paragraph narrative entries    |
| In-game UI help / tooltips       |  10 | Short text blurbs                    |
| Articles (newspaper, files)      |  13 | Press-clipping style documents       |
| Labels, bios, Bible, credits     |  64 | Miscellaneous reference text         |
| **Total**                        | **474** | |

## File format

Each `.xtx` file is a **plain text file**. The first bytes tell you the
encoding:

- A leading `FF FE` BOM means UTF-16LE.
- No BOM, ASCII-range bytes means UTF-8 (effectively ASCII for English
  releases).

There is **no binary header, no length field, no checksum** — the file
is just the body text. Open one in any text editor.

Example: `XT/49526.xtx` (a Willmore journal entry from March 1996):

```
March 31, 1996
This work I'm doing is getting frustrating. I've been beating my head
against a brick wall trying to get a resolution to this case. Militias
are a pernicious force in America, and they …
```

## Extraction

The v0.1 release shipped the `xt_dump` extractor that reads every
`.xtx` file in a directory, classifies it by content heuristics
(date pattern → journal; "Dear …" → email; etc.), and emits a
JSON dump:

```bash
python -m hdb_extract extract /path/to/XFILES.HDB --out=out/
# After extract, look at:
cat out/json/xt_dump.json
```

For raw text without classification, just iterate the directory:

```python
from pathlib import Path

for xt in sorted(Path("/path/to/XT").glob("*.xtx")):
    raw = xt.read_bytes()
    if raw.startswith(b"\xff\xfe"):
        text = raw[2:].decode("utf-16-le")
    else:
        text = raw.decode("utf-8", errors="replace")
    print(f"=== {xt.name} ===")
    print(text)
    print()
```

## Cross-reference with HDB

Some HDB records (`VCEmail`, `VCPDANotes`, `VCEmailPending`) refer to
XT files by **scene_asset_map ID** (the same ID used by the production
map for media assets). The `decision_tree.json` extractor (v0.1) joins
these references for you:

```python
import json
tree = json.load(open("out/json/decision_tree.json"))
for email in tree["emails"]:
    print(email["from"], email["subject"], "->", email["xt_file"])
```

## XTLanguage VM — not in scope for v0.2

The name "XTLanguage" suggests a scripted-cinematic VM. We have **not**
located such a VM in the X-Files release: every `.xtx` file we've
inspected is plain text, and no engine routine decodes anything
beyond UTF text from these files.

It's possible that other HyperBole / NeoLogic titles (Quantum Gate,
Doom Mac) did ship a bytecode XTLanguage and that the X-Files release
simplified to text-only. Verifying this is in [ROADMAP.md](ROADMAP.md)
under v0.3+ (community welcome).

## Why include this doc in v0.2?

Two reasons:

1. **AGRIPPA** (the prior web-port effort) struggled to extract the
   narrative content from `.xtx` because the file structure looked
   "binary" at first glance (the `.xtx` extension + occasional UTF-16
   BOM threw off naïve readers). Clarifying that the format is plain
   text removes a stumbling block.
2. **For fan-ports**, the email and journal content is **a significant
   chunk of the narrative**. Knowing where it lives (separate from the
   HDB) is essential for re-assembly.

## File sizes

```
total : 676 KB
mean  : 1.4 KB
max   : ~10 KB (longer Willmore journal entries)
min   : <100 B (UI snippets)
```

Small enough that you can include the whole directory in a fan-port
distribution (cleared by the publisher, of course — see top-level
`NOTICE`).
