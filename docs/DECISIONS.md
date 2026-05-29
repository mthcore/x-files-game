# Decisions — the story-flow graph

The most valuable artifact in `XFILES.HDB`, from a fan-port perspective,
is the **decision tree** — the full directed graph of:

- Scenes and the transitions between them.
- Player choices and their consequences.
- Triggers and the variable updates they perform.
- Dialogue branches and their gating conditions.

This is exactly what AGRIPPA (the prior web-port effort) could not
extract and therefore had to reconstruct manually. With this project,
the entire graph drops out of one CLI command.

## What we ship

[`../examples/outputs/decision_tree.json`](../examples/outputs/decision_tree.json)
(~1.6 MB) is a self-contained JSON snapshot of the X-Files decision
graph. It mirrors the in-game state machine and can be loaded into:

- A graph visualizer (Graphviz, vis.js, cytoscape).
- An external mod tool to author new branches.
- A re-implementation in any modern engine (Godot, Bevy, raylib, ...).

## Structure

```jsonc
{
  "version": "0.1.0",
  "source_hdb_sha256": "...",
  "scenes": [
    {
      "scene_id": 1,
      "name": "Office_Mulder",
      "entry_triggers": [12, 14],
      "exit_triggers": [13],
      "hotspots": [
        {"id": 17, "x": 120, "y": 340, "w": 160, "h": 70,
         "click_action": 102, "hover_action": 0}
      ],
      "conversations": [3, 7]
    },
    ...
  ],
  "conversations": [
    {
      "conv_id": 3,
      "name": "Mulder_intro",
      "entry_node": 30,
      "nodes": [
        {"id": 30, "speaker": "Mulder", "audio": "M_001.wav",
         "next": 31, "gates": []},
        {"id": 31, "speaker": "<player>", "choices": [
          {"text_id": 4001, "next": 32, "vars_set": {"sympathy": 1}},
          {"text_id": 4002, "next": 33, "vars_set": {"sympathy": -1}}
        ]}
      ]
    },
    ...
  ],
  "triggers": [
    {"id": 12, "event": "EVT_ENTER", "conditions": [...],
     "actions": [{"op": "ACT_PLAY_VIDEO", "args": [501]}],
     "targets": [1]},
    ...
  ],
  "variables": [
    {"id": 7, "name": "case_progress", "type": "int", "initial": 0},
    ...
  ],
  "udfs": [
    {"id": 12, "name": "udf_check_pda_email", "args": ["int"]},
    ...
  ]
}
```

(The exact JSON shape is what
[`python/src/hdb_extract/extractors/decision_tree.py`](../python/src/hdb_extract/extractors/decision_tree.py)
emits; consult the source for the authoritative schema.)

## Email subgraph

E-mails sent to Mulder/Scully's in-game PDA form a separate
sub-graph. They are captured as:

- `VCEmail` (class_id `0x5A`) — base email record.
- `VCEmailRead` (class_id `0x5B`) — read-marker.
- `VCEmailPending` (class_id `0x5C`) — scheduled-for-future-delivery.

In the JSON output above, emails appear under their own `emails: [...]`
top-level key (run `extract` to produce; or see the curated email tree
in the upstream research tree).

## Visualizing

```bash
# Quick stats
jq '.scenes | length, .conversations | length, .triggers | length' \
   examples/outputs/decision_tree.json

# Graph (DOT) — feed into Graphviz
python -c '
import json, sys
g = json.load(open("examples/outputs/decision_tree.json"))
print("digraph X { rankdir=LR;")
for c in g["conversations"]:
    for n in c["nodes"]:
        for nxt in [n.get("next")] + [ch["next"] for ch in n.get("choices", [])]:
            if nxt: print(f"  n{n[\"id\"]} -> n{nxt};")
print("}")' | dot -Tpng -o decisions.png
```

## Limitations

- **Variable initial values** are observed from the HDB; the game may
  update them at install time or via the launcher. We use the as-shipped
  values.
- **Localization**: the X-Files release shipped in English only.
  `text_id` references resolve through a separate string table; the
  decision_tree JSON exposes the ids, not the strings.
- **Save-state interaction**: triggers gated on `VCGameState` fields
  fire deterministically only with the initial state. To replay a
  specific saved game, load the save file separately (HDB describes
  the *game definition*, not the *current state*).

## See also

- [TRIGGERS.md](TRIGGERS.md) — trigger grammar and event types.
- [DIALOGUES.md](DIALOGUES.md) — conversation node structure.
- [HOTSPOTS.md](HOTSPOTS.md) — input event sources.
- [`../examples/outputs/decision_tree.json`](../examples/outputs/decision_tree.json)
  — sample decision graph from `XFILES.HDB`.
