# Official trigger system (W20.4)

Complete trigger system documented from the VCAuthor manual, validated against
our format analysis W9.3b + W16 + W19.

## Architecture

```text
Object (Title/Node/Location/View/Hotspot/...)
└── Triggers (multiple per object)
    └── Trigger
        ├── Event (1 of 9 types)
        └── Action List (ordered, multiple actions)
            └── Action
                ├── Expression (optional condition)
                │   └── LHS op RHS
                └── Action Body (1 of 8 types)
```

## 9 official event types

| Event | Description | Validation |
| ----- | ----------- | ---------- |
| **Mouse Click** | Player clicks on hotspot | ✓ |
| **Right Click** | Player right-clicks (Cmd-Click Mac) | ✓ |
| **Key Press** | Player presses key (focused object) | ✓ (KeyPress strings) |
| **Roll Into** | Cursor enters hotspot rect | ✓ |
| **Roll Off** | Cursor leaves hotspot rect | ✓ |
| **Drop** | Player drops dragged icon | ✓ (Drop string) |
| **Timer Expiration** | Timer N goes off | ✓ (Timer string) |
| **Object Activation** | Object enters active state | ✓ |
| **Object Deactivation** | Object leaves active state | ✓ |

### Event-Object Interaction Matrix

| VC Object | Click | Right | Key | RollIn | RollOff | Drop | Timer | Activ | Deact |
| --------- | ----- | ----- | --- | ------ | ------- | ---- | ----- | ----- | ----- |
| Title | | | | | | | ✓ | ✓ | ✓ |
| Node | | | | | | | ✓ | ✓ | ✓ |
| Location | | | | | | | ✓ | ✓ | ✓ |
| ViewPoint | | | | | | | ✓ | ✓ | ✓ |
| View | | | | | | | ✓ | ✓ | ✓ |
| Asset | | | | | | | ✓ | ✓ | ✓ |
| Navigation | ✓ | ✓ | | ✓ | ✓ | | ✓ | ✓ | ✓ |
| Character | ✓ | ✓ | | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Explorable | ✓ | ✓ | | ✓ | ✓ | | ✓ | ✓ | ✓ |
| InterfaceItem | ✓ | ✓ | ✓ | ✓ | ✓ | | ✓ | ✓ | ✓ |
| Conversation | ✓ | ✓ | | ✓ | ✓ | | ✓ | ✓ | ✓ |
| Idea Response | | | | | | | | ✓ | ? |
| Icon | ✓ | ✓ | | ✓ | ✓ | | ✓ | ✓ | ✓ |

(✓ = supported, blank = not supported per VCAuthor doc)

**Note** : `Character + Drop` is exclusive — this is how **Idea Icons
are drag-dropped onto Characters** to start a conversation.

## 8 official action types

### 1. Statement

Variable assignment :

```text
<Left-hand-side> <op> <Right-hand-side>
```

Operators :
- `=` Assign RHS to LHS
- `+` Increment LHS by 1
- `--` Decrement LHS by 1
- `+=` Add RHS to LHS
- `-=` Subtract RHS from LHS
- `*=` Multiply LHS by RHS
- `/=` Divide LHS by RHS
- `%=` Mod LHS by RHS

### 2. Asset

Manipulate an asset :
- **Preload** (not implemented in shipped engine)
- **Play/Display (once)** — play once or show static
- **Play/Display (loop)** — play in loop
- **Stop/Unshow** — stop / hide
- **Unload** (not implemented)

### 3. Timer

Start or stop a timer (25 timers max, shared across objects) :
- One-shot or periodic
- Timer ID 0..24
- Generates **Timer Expiration** event when fires

### 4. Enable

Turn an object on or off :
- Targets : any object with `Initially Enabled` or `Initially Disabled` status
- `Always Enabled` objects can't be Enable-acted

### 5. Set View

Jump to a different View (cross-node allowed) :
- More powerful than Navigation hotspots which are within-node only

### 6. Interface

Display or Unshow an Interface Layout :
- `Display Interface` = show all enabled items in layout
- `Unshow Interface` = hide all items

### 7. C++ Function

Call a User Defined Function (UDF) :
- Function name (case-sensitive)
- Parameters list

In X-Files, **203 scripts registered** (W16) — these
are the available UDFs. Examples :
- `WillmoreSleeps`, `RegDayNight`, `AlienBlast`, `SmAttack`, `FixJumpCut`,
  `SendEmail`, `SaveGameState`, `RandGen`, etc.

### 8. 3D Sound

Asset action with X,Y position coordinates for 3D placement.

## Expression operators (conditions)

An Action can be conditional :

```text
If <LHS> <op> <RHS> then <Action>
```

Operators :
- `=` Equal
- `!=` Not equal
- `>` Greater than
- `<` Less than
- `<=` Less than or equal
- `>=` Greater than or equal
- `and` Boolean AND
- `or` Boolean OR

## Variable scope hierarchy

```text
Title vars              → visible to ALL objects (global)
  └─> Node vars         → visible to Locations, ViewPoints, Views, hotspots
       └─> Location vars → visible to ViewPoints, Views, hotspots
            └─> ViewPoint vars → visible to Views, hotspots
                 └─> View vars → visible to hotspots
                      └─> Object vars (Hotspot/Asset/Icon...) → only self

Special : Local Variables → scope = current ActionList (scratch)
```

## Variable types

- **Boolean** (true/false) — `bXxx` Hungarian prefix
- **Integer** — `iXxx`
- **Character** (byte) — `cXxx`
- **String** — `sXxx` or `scXxx` (short const)
- Each type has a **const** variant (read-only)

Mapping of our 670 Hungarian vars (W9.4) :
- 362 `b*` → Boolean
- 115 `i*` → Integer
- 10 `n*` → unsigned Integer (probable Integer with hint)
- 35 `IFL_c*` → Character (byte) IFL scope
- 5 `IFL_sc*` → String const IFL scope
- 7 `l*` → Long (Integer 32-bit)
- 51 `S*` → Struct or Short
- 125 IFL_* + scope = scope-prefixed

## Example trigger from VCAuthor screenshot (Comity Inn Node 2)

Figure 18 of the VCAuthor manual shows the **Object Activation
triggers** of the Comity Inn location, with 14 conditional actions :

```text
If Title::iCurrNode (+10 for links) = 2 then Play/Display (once) Video:Comity:Entrance Action
If Title::NODE_bSkinnerIsInSeattle = Title:: bTRUE then Enable Asset: Navs:LoopsR:Comity Inn:3E…
If Title::NODE_bSkinnerIsInSeattle = Title:: bTRUE then Enable CharView: Skinner
If Title::NODE_bSkinnerIsInSeattle = Title:: bTRUE then Enable Asset: Navs:LoopsR:Comity Inn:056…
If Title::iDayNight >= 11 then SetFLightProps(2,I,M)
If Title::iDayNight >= 11 then SetLBoxTint(50,50,50)
If Title::iDayNight <= 10 then SetFLightProps(1,I,M)
If Title::iCurrNode (+10 for links) >= 3 then Disable Asset: Navs:LoopsR:Comity Inn:068 - COMNAV3F
If Title::iCurrNode (+10 for links) >= 3 then Disable CharView: Motel Clerk
If Title::bN2HadEncounterWithNSA = Title:: bTRUE then DoActionList(80161,80163)
If Title::cAIComityWhereAreWe (N=None, P=Phone, D=Done) = C then SetAIList(59567,59569)
If Title::cAIComityWhereAreWe = N then ClearAIList()
If Title::cAIComityWhereAreWe = D then ClearAIList()
If Title::bAIGeneral_IsItTimeToSleep = Title:: bTRUE then SetAIList(93293,93294)
```

**Key revelations from this screenshot** :

1. **`Title::iCurrNode`** = current node (official name for our concept)
2. **`Title::iDayNight`** = day/night cycle (our `iLastDayNight`)
3. **`Title::NODE_bSkinnerIsInSeattle`** = node-scope flag present in Title
4. **`bN2HadEncounterWithNSA`** = **Encounter With NSA** !
   Confirms Node 9 = "Escape NSA" hypothesis
5. **`cAIComityWhereAreWe`** = location tracker within Comity (N/P/D states)
6. **`SetFLightProps`, `SetLBoxTint`, `SetAIList`, `DoActionList`,
   `ClearAIList`** = UDF scripts called
7. **`bTRUE` constant** stored in Title (variable constant pattern)
8. **`iCurrNode (+10 for links)`** = special notation for cross-node refs

## Vars revealed by this trigger

| Variable | Type | Scope | Role |
| -------- | ---- | ----- | ---- |
| `iCurrNode` | int | Title | Current Node index 1-10 |
| `iDayNight` | int | Title | Time of day counter |
| `NODE_bSkinnerIsInSeattle` | bool | Node | Skinner presence |
| `bN2HadEncounterWithNSA` | bool | Title | NSA contact at Node 2 |
| `cAIComityWhereAreWe` | char (byte) | Title | Comity location N/P/D |
| `bAIGeneral_IsItTimeToSleep` | bool | Title | Sleep eligibility (W15.2) |
| `bTRUE` | const bool | Title | Reusable TRUE literal |

## UDF (User Defined Functions) examples

From the screenshot + our W16 scripts :

| UDF Name | Params | Role |
| -------- | ------ | ---- |
| `SetFLightProps(2,I,M)` | (level, intensity, mode) | Flashlight beam control |
| `SetLBoxTint(50,50,50)` | (R,G,B) | LetterBox tint color |
| `SetAIList(59567,59569)` | (asset1, asset2) | Set Artificial Intuition icon list |
| `ClearAIList()` | () | Clear AI suggestions |
| `DoActionList(80161,80163)` | (range start, end) | Execute a range of actions by ID |

Note: **AI Lists** in VCAuthor = **suggestions / hints** to the player via
the Artificial Intuition icon (W17.1 and VC Bible). The IDs 59567+ are
the assets/actions sub-list.

## Status W20.4

✓ 9 Events documented with object support matrix
✓ 8 Actions documented with complete syntax
✓ Expression operators + Statement operators listed
✓ Variable scope hierarchy clarified
✓ Variable types mapped to our 670 Hungarian vars
✓ Example real trigger from Comity Inn analysed
✓ UDF examples documented

**Key discovery** : `bN2HadEncounterWithNSA` confirms **NSA is
the major antagonist** of Node 9 "Escape NSA". The player has an encounter
at Node 2 (Initial Investigation) that sets up the climax.
