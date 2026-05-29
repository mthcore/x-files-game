# Dialogues — conversation system

Conversations in X-Files are branching dialogue trees with multiple
endings, gated by player choices and game-state variables. The full
conversation graph is persisted in `XFILES.HDB` via:

- `VCConversation` (class_id `0x31`) — a single conversation/dialogue.
- `VCConversationList` (class_id `0x32`) — collection of conversations.
- `VCConversationHistory` (class_id `0x56`) — record of past
  player choices.

## VCConversation byte-direct grammar

A `VCConversation` carries seven tagged fields (single-byte tags `e..k`),
each a big-endian dword (`k` is a byte), version-gated:

| Tag | Field | Notes |
|----|-------|-------|
| `e` | `conversation_id` | |
| `f` | `speaker_topic_id` | who/what the line is about |
| `g` | `prompt_string_id` | → localization-DLL string (version ≥ 3) |
| `h` | `response_id_1` | → localization-DLL string |
| `i` | `response_id_2` | → localization-DLL string (version ≥ 3) |
| `j` | `next_conversation_id` | branch target |
| `k` | `flags` | visited / unlocked (version ≥ 4) |

The on-disk field order is `e, f, h, j` then (v≥3) `g, i` then (v≥4) `k`.

**Dialogue text → localization DLL.** The `prompt`/`response` ids resolve to the
dialogue strings in `XFilest.dll` (RT_STRING, UTF-16LE), extracted with
`hdb-extract loc`. All 1025 lines are in
[`examples/outputs/dialogue_lines.json`](../examples/outputs/dialogue_lines.json)
— e.g. id 19 = *"Nous recherchons des informations sur deux agents du FBI qui ont
logé ici."* (the motel-clerk question).

## Resolution status (honest)

- **Dialogue text** — fully recovered: 1025 lines (DLL), id-indexed.
- **Field grammar** — recovered (table above).
- **Per-conversation walk** — 45 records dumped byte-direct from
  `XFILES.HDB` (28 conversation payloads + 1 list container + 16 list
  entries) into
  [`examples/outputs/conversations.json`](../examples/outputs/conversations.json).
  Per record we emit every grammar slot (`conversation_id`, `topic`,
  `prompt`, `response_1`, `response_2`, `next`, `flags`) with an explicit
  `certainty` marker: `byte-direct` when the dword was read straight off
  the on-disk payload, `undetermined` otherwise (with a `reason` —
  e.g. `field-not-present-in-payload`,
  `u32-not-in-dll-string-range`,
  `u32-not-in-dll-string-range-and-not-in-extended-index`).
- **Resolved-text count** — 21 of the 45 records currently resolve to a
  real `XFilest.dll` string (out of 1025 strings available). The
  remainder keep their raw u32 and a documented `undetermined` reason —
  no guessing.
- **Handle resolution (page-aligned + extended).** Each grammar slot whose
  u32 is *not* a DLL string id is consulted against the
  `MasterHDBResolver` extended map (page-aligned `CNeoPersist` markers
  plus the `CNeoIDList` / `CNeoIDIndex` pair tables, see
  [NEOPERSIST.md](NEOPERSIST.md)). When the lookup hits, the slot gains:

  ```text
  target_offset:      uint32   // byte-direct absolute file offset
  target_class_id:    uint16   // CNeoPersist class id at that offset
  target_class_name:  string   // CLASSES.md row name
  resolver_source:    "page-aligned" | "extended"
  ```

  with `certainty: "byte-direct"`. When it does not hit, the slot
  carries `handle_status: "handle-not-in-extended-index"` and keeps its
  raw u32. Current run: 62/86 candidate handles resolve byte-direct (46
  via page-aligned markers, 16 via the extended index), 24 stay
  unresolved.
- **Branch wiring.** `graph_edges` is byte-direct on both ends — `from`
  is a record offset, `to` is the resolved `target_offset` — and now
  carries the 16 list-container entries plus the cross-conversation
  hops obtained by resolving `next_conversation_id` / `response_*`
  through the extended index (`kind ∈ {list_entry,
  cross_conversation, ...}`, `via ∈ {next, response_1, response_2,
  list_entry}`, `resolver_source ∈ {page-aligned, extended}`). 78 edges
  are exported for this run.

### Honest boundary

- The conversation *instances* still reference each other through the
  NeoPersist ID-index layer (`VCConversationList` 0x32 + ID-index; see
  [FORMAT.md](FORMAT.md), [NEOPERSIST.md](NEOPERSIST.md)). Slots that
  resolve through `extended` consume the same index-pair tables
  byte-direct; slots that don't resolve are reported with their raw
  value and an explicit `handle_status`. Nothing is invented — every
  edge in `graph_edges` has a byte-direct `target_offset`.

The extractor honesty contract is part of the JSON header
(`_about` / per-field `certainty` / `reason`); downstream tools can
filter on it without re-implementing the rules.

A typical X-Files conversation has 5-30 nodes. Each node represents
either an NPC line, a player choice, or a branching decision point.

## Conversation node layout

Conversation nodes are stored as separate `VCAction` records (yes —
the same `VCAction` class is reused; see [TRIGGERS.md](TRIGGERS.md)).
Each node has:

- A unique node id (within the conversation scope).
- The asset id of the FMV / audio line.
- The character speaking (`VCCharacter` reference).
- Optional next-node id (or set of next-node candidates).
- Optional condition expression (gates the node on game state).

## Character roster

The `VCCharacter` records (class_id `0x2C`) identify the speakers.
Examples observed in the data:

| char_id | Symbolic name | Role |
|---------|---------------|------|
| —       | Mulder        | Player character (FBI agent) |
| —       | Scully        | Player character (FBI agent, alternate POV) |
| —       | Skinner       | FBI Assistant Director |
| —       | Shanks        | FBI Seattle field-office boss (Willmore's superior) |
| —       | Willmore      | Seattle ASAC |
| (many more — see VCCharacter inventory) |

The full character roster is in the extracted
`out/json/decoded_records.json` after running `extract` on a HDB.

## Dialogue history

`VCConversationHistory` is a save-state-aware record that tracks which
conversation nodes the player has already heard. It's used to:
- Prevent dialogue repetition.
- Unlock conditional branches based on past choices.
- Drive the PDA's "Cases" or "Notes" view.

## Extracting dialogues

```bash
python -m hdb_extract extract /path/to/XFILES.HDB --out=out/

# View one conversation:
jq '.[] | select(.["__class__"] == "VCConversation") | .' \
  out/json/decoded_records.json | head -60

# Dialogue tree (semantic):
cat out/json/decision_tree.json | jq '.conversations[0]'
```

The `decision_tree.json` extractor (~1.6 MB) consolidates conversations,
choices, and gating conditions into a single navigable graph — see
[DECISIONS.md](DECISIONS.md).

## See also

- [DECISIONS.md](DECISIONS.md) — the broader story-flow graph.
- [TRIGGERS.md](TRIGGERS.md) — how dialogue side effects (variable
  updates, scene changes) are realized.
- [`../examples/outputs/decision_tree.json`](../examples/outputs/decision_tree.json)
  — a 1.6 MB snapshot of the full dialogue graph.
