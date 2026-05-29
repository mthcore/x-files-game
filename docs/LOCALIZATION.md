# Localization — string tables in XFiles*.dll

The X-Files retail release ships **four resource-only Windows PE DLLs**
holding all the localized strings:

| File           | Size      | Category        | Strings |
|----------------|----------:|-----------------|--------:|
| `XFilesc.dll`  | 22 528 B  | credits (cast + staff) |  27   |
| `XFilese.dll`  | 28 160 B  | error messages + system reqs |  33   |
| `XFiless.dll`  | 28 160 B  | save / scene titles  | 251   |
| `XFilest.dll`  | 84 480 B  | in-game text (dialogues + UI) | 1 025 |
| **Total**      |           |                 | **1 336** |

(Numbers measured on the retail French release; English/Spanish/...
releases will have similar shapes with translated content.)

## Why four DLLs?

The four files all declare `LANG_FRENCH_STANDARD (0x040C)`. They are
**not** different languages — they are **per-category partitions** of
the localized strings, presumably split at build time for editorial
reasons (different teams writing the dialogue vs. the error messages
vs. the credits).

A fan-port targeting modern engines should merge them into a single
locale file (see `--merge` planned for v0.3).

## On-disk layout

Each DLL is a **resource-only Windows PE** (no executable code):

- `ImageBase = 0x10000000`
- One resource type: `RT_STRING (6)`
- One language sub-tree per string block
- Strings stored as UTF-16LE, grouped into 16-entry blocks

### Block layout

A "string block" is a Windows resource convention. Each block has a
resource ID in the range `1..N` and contains exactly 16 string slots
(IDs `(block_id - 1) * 16 + 0..15`):

```
for i in 0..15:
    uint16_t length_in_chars        // little-endian
    wchar_t  chars[length_in_chars] // no null terminator, UTF-16LE
```

A slot with `length == 0` means "no string at this ID" (sparse).

## CLI usage

```bash
# Inspect (count, language, ID range)
python -m hdb_extract loc XFilest.dll

# Extract to JSON
python -m hdb_extract loc XFilest.dll --out strings_t.json
```

Example output:

```
file       : XFilest.dll
category   : game_text
language   : 0x040c (1036)
strings    : 1025
id range   : 1..1105
wrote strings_t.json
```

The JSON file shape:

```jsonc
{
  "file": "XFilest.dll",
  "category": "game_text",
  "language_id": 1036,
  "language_hex": "0x040c",
  "string_count": 1025,
  "id_range": [1, 1105],
  "strings": {
    "1": "Qui est le gros bonnet de D.C.?",
    "2": "Sur quelles affaires travailles-tu?",
    "3": "...",
    ...
  }
}
```

## Python API

```python
from hdb_extract.extractors.loc_extract import LocaleDLL, merge_locales

# One DLL
loc = LocaleDLL("XFilest.dll")
print(loc.language)         # 0x040C
print(loc.strings[1])       # 'Qui est le gros bonnet de D.C.?'
print(loc.summary())        # JSON-ready

# All four merged
merged = merge_locales(["XFilesc.dll", "XFilese.dll",
                        "XFiless.dll", "XFilest.dll"])
# merged["categories"]["game_text"]["1"] == "Qui est le gros bonnet de D.C.?"
```

## Examples — what's inside each DLL

### Credits (`XFilesc.dll`)

Cast names and behind-the-scenes credits:

> Avec DAVID DUCHOVNY
> GILLIAN ANDERSON
> CHRIS CARTER
> Musique MARK SNOW
> Programmation PETE ISENSEE

### Errors (`XFilese.dll`)

System-requirements and error messages:

> Pour une utilisation optimale du jeu X-Files, votre écran et …
> Le jeu X-Files requiert une résolution d'écran et de carte graphique de 640x480 minimum.
> Le jeu X-Files requiert DirectX 5.0 (ou plus)…

### Save titles (`XFiless.dll`)

Short labels used as save-slot titles or scene names:

> les extraterrestres vous observent
> tous contre moi
> anxiété

### Game text (`XFilest.dll`) — the bulk

In-game dialogue choices, PDA hints, UI labels:

> Qui est le gros bonnet de D.C.?
> Sur quelles affaires travailles-tu?

This is the file you'll merge with `decision_tree.json` (from the HDB
decoder) to resolve `text_id` references into human-readable dialogue.

## Resolving text_id from the HDB

The HDB stores dialogue references as numeric `text_id` integers. To
join them with their text:

```python
import json
from hdb_extract.extractors.loc_extract import LocaleDLL

# Load the game-text DLL
loc = LocaleDLL("XFilest.dll").strings

# Load the decision tree (from `python -m hdb_extract extract`)
with open("out/json/decision_tree.json") as f:
    tree = json.load(f)

# Resolve text_ids in conversation choices
for conv in tree["conversations"]:
    for node in conv["nodes"]:
        for choice in node.get("choices", []):
            tid = choice.get("text_id")
            if tid in loc:
                choice["text_resolved"] = loc[tid]
```

## Status in this decoder

- ✅ All four DLLs decoded byte-direct (1 336 strings total in FR).
- ✅ UTF-16LE decoding preserves French accents (`é`, `à`, `ç`, etc.).
- ✅ ID-to-text mapping faithful (sparse IDs supported).
- 🟡 Cross-reference with `decision_tree.json` is manual in v0.2 (see
  code example above); auto-merge planned for v0.3.

## Status in other releases

The decoder is locale-agnostic: it reads any `XFiles*.dll` regardless
of declared language. Retail releases known to exist:

| Release  | Language ID | Notes                          |
|----------|-------------|--------------------------------|
| French   | `0x040C`    | This is the release we tested  |
| English  | `0x0409`    | Reportedly identical structure |
| Spanish  | `0x040A`    | Reportedly identical structure |

Open a `format-finding` issue if you test against a different release
and the decoder fails — we'd love to add it to the test matrix.
