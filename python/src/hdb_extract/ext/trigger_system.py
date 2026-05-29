"""Official VirtualCinema trigger system grammar (W20.4).

Sourced from `the research datasets` which itself
is sourced from the VCAuthor manual + validated against the format analysis from
format-analysis (W9.3b + W16 + W19).

This module provides the **grammar** of the trigger system : the universe
of possible Events, Actions, Operators, Scopes, Variable types. The actual
per-trigger Conditions/Actions/Targets payloads are packed in HDB and
need the G1 wire-format crack to be byte-decoded.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any


# ---------------------------------------------------------------------------
# 9 Event Types
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class EventType:
    name: str
    description: str
    supported_on: tuple[str, ...]


EVENT_TYPES: tuple[EventType, ...] = (
    EventType(
        name="MouseClick",
        description="Player clicks on hotspot",
        supported_on=("Navigation", "Character", "Explorable",
                      "InterfaceItem", "Conversation", "Icon"),
    ),
    EventType(
        name="RightClick",
        description="Player right-clicks (Cmd-Click on Mac)",
        supported_on=("Navigation", "Character", "Explorable",
                      "InterfaceItem", "Conversation", "Icon"),
    ),
    EventType(
        name="KeyPress",
        description="Player presses a key, focused object receives it",
        supported_on=("InterfaceItem",),
    ),
    EventType(
        name="RollIn",
        description="Cursor enters hotspot rect",
        supported_on=("Navigation", "Character", "Explorable",
                      "InterfaceItem", "Conversation", "Icon"),
    ),
    EventType(
        name="RollOff",
        description="Cursor leaves hotspot rect",
        supported_on=("Navigation", "Character", "Explorable",
                      "InterfaceItem", "Conversation", "Icon"),
    ),
    EventType(
        name="Drop",
        description="Player drops a dragged Idea Icon on this object",
        supported_on=("Character",),
    ),
    EventType(
        name="TimerExpiration",
        description="One of 25 game timers fires",
        supported_on=("Title", "Node", "Location", "ViewPoint", "View",
                      "Asset", "Navigation", "Character", "Explorable",
                      "InterfaceItem", "Conversation", "Icon"),
    ),
    EventType(
        name="ObjectActivation",
        description="Object transitions into active state",
        supported_on=("Title", "Node", "Location", "ViewPoint", "View",
                      "Asset", "Navigation", "Character", "Explorable",
                      "InterfaceItem", "Conversation", "IdeaResponse", "Icon"),
    ),
    EventType(
        name="ObjectDeactivation",
        description="Object transitions out of active state",
        supported_on=("Title", "Node", "Location", "ViewPoint", "View",
                      "Asset", "Navigation", "Character", "Explorable",
                      "InterfaceItem", "Conversation", "Icon"),
    ),
)


# ---------------------------------------------------------------------------
# 8 Action Types (the "body" of each Action)
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class ActionType:
    name: str
    description: str
    params: tuple[str, ...]


ACTION_TYPES: tuple[ActionType, ...] = (
    ActionType(
        name="Statement",
        description="Variable assignment (lhs op rhs)",
        params=("lhs_var", "operator", "rhs_expr"),
    ),
    ActionType(
        name="Asset",
        description="Play/Display/Stop/Unshow an asset",
        params=("asset_path", "operation"),  # operation = Play/Display(once),
                                              # Play/Display(loop), Stop/Unshow
    ),
    ActionType(
        name="Timer",
        description="Start or stop one of 25 timers",
        params=("timer_id", "operation", "duration"),
    ),
    ActionType(
        name="Enable",
        description="Turn an object on or off (toggle Initially Enabled/Disabled)",
        params=("target_object", "state"),
    ),
    ActionType(
        name="SetView",
        description="Jump to a different View (cross-node allowed)",
        params=("view_path",),
    ),
    ActionType(
        name="Interface",
        description="Display/Unshow an Interface Layout",
        params=("layout_id", "operation"),
    ),
    ActionType(
        name="UDF",
        description="Call a C++ User Defined Function (203 registered in X-Files)",
        params=("function_name", "params"),
    ),
    ActionType(
        name="3DSound",
        description="Play a sound with x,y position for 3D placement",
        params=("asset_path", "x", "y", "operation"),
    ),
)


# ---------------------------------------------------------------------------
# Operators
# ---------------------------------------------------------------------------

EXPRESSION_OPERATORS: tuple[dict, ...] = (
    {"op": "=", "name": "Equal", "kind": "comparison"},
    {"op": "!=", "name": "NotEqual", "kind": "comparison"},
    {"op": ">", "name": "GreaterThan", "kind": "comparison"},
    {"op": "<", "name": "LessThan", "kind": "comparison"},
    {"op": ">=", "name": "GreaterOrEqual", "kind": "comparison"},
    {"op": "<=", "name": "LessOrEqual", "kind": "comparison"},
    {"op": "and", "name": "And", "kind": "boolean"},
    {"op": "or", "name": "Or", "kind": "boolean"},
)


STATEMENT_OPERATORS: tuple[dict, ...] = (
    {"op": "=", "name": "Assign"},
    {"op": "+", "name": "Increment"},   # unary +1
    {"op": "--", "name": "Decrement"},  # unary -1
    {"op": "+=", "name": "AddAssign"},
    {"op": "-=", "name": "SubAssign"},
    {"op": "*=", "name": "MulAssign"},
    {"op": "/=", "name": "DivAssign"},
    {"op": "%=", "name": "ModAssign"},
)


# ---------------------------------------------------------------------------
# Variable scope hierarchy and types
# ---------------------------------------------------------------------------

VARIABLE_SCOPES: tuple[str, ...] = (
    "Title",      # global, visible to all objects
    "Node",       # visible to Locations, ViewPoints, Views, hotspots
    "Location",   # visible to ViewPoints, Views, hotspots
    "ViewPoint",  # visible to Views, hotspots
    "View",       # visible to hotspots
    "Object",     # only self (Hotspot, Asset, Icon, ...)
    "Local",      # scratch — scope = current ActionList
)


VARIABLE_TYPES: tuple[dict, ...] = (
    {"prefix": "b", "type": "Boolean", "cpp": "bool"},
    {"prefix": "i", "type": "Integer", "cpp": "int32_t"},
    {"prefix": "n", "type": "UnsignedInteger", "cpp": "uint32_t"},
    {"prefix": "c", "type": "Character", "cpp": "uint8_t"},
    {"prefix": "l", "type": "Long", "cpp": "int32_t"},
    {"prefix": "s", "type": "String", "cpp": "char*"},
    {"prefix": "sc", "type": "StringConst", "cpp": "const char*"},
    {"prefix": "S", "type": "Struct", "cpp": "struct"},
    # Each type has a `const` variant (read-only).
    # IFL_*, IL_*, NODE_* prefixes are scope qualifiers stacked on top.
)


# ---------------------------------------------------------------------------
# Architecture description
# ---------------------------------------------------------------------------

TRIGGER_ARCHITECTURE = {
    "diagram": (
        "Object (Title|Node|Location|View|Hotspot|...) \n"
        "└── Triggers (multiple per object) \n"
        "    └── Trigger \n"
        "        ├── Event (1 of 9 types) \n"
        "        └── Action List (ordered, multiple actions) \n"
        "            └── Action \n"
        "                ├── Expression (optional condition) \n"
        "                │   └── LHS op RHS \n"
        "                └── Action Body (1 of 8 types) "
    ),
    "vc_trigger_layout": {
        "conditions": "8 slots × 4B (VCObject* pointers to Condition records)",
        "actions": "12 slots × 4B (VCObject* pointers to Action records)",
        "targets": "4 slots × 1B (scope-code or scene-id bytes)",
    },
    "official_source": "VCAuthor manual W20.4",
    "decomp_validation": "W9.3b + W16 + W19 + W21.3",
}


# ---------------------------------------------------------------------------
# Concrete example from VCAuthor Figure 18 (Comity Inn Node 2)
# ---------------------------------------------------------------------------

EXAMPLE_COMITY_INN_NODE2 = {
    "context": "Object Activation triggers for Comity Inn Node 2",
    "source": "VCAuthor manual Figure 18 (W20.4)",
    "actions": [
        {
            "condition": {"lhs": "Title::iCurrNode", "op": "=", "rhs": "2"},
            "body": {"type": "Asset",
                     "asset_path": "Video:Comity:Entrance",
                     "operation": "Play/Display(once)"},
        },
        {
            "condition": {"lhs": "Title::NODE_bSkinnerIsInSeattle",
                          "op": "=", "rhs": "Title::bTRUE"},
            "body": {"type": "Enable",
                     "target": "Asset: Navs:LoopsR:Comity Inn:3E"},
        },
        {
            "condition": {"lhs": "Title::NODE_bSkinnerIsInSeattle",
                          "op": "=", "rhs": "Title::bTRUE"},
            "body": {"type": "Enable",
                     "target": "CharView: Skinner"},
        },
        {
            "condition": {"lhs": "Title::iDayNight", "op": ">=", "rhs": "11"},
            "body": {"type": "UDF",
                     "name": "SetFLightProps",
                     "params": [2, "I", "M"]},
        },
        {
            "condition": {"lhs": "Title::iDayNight", "op": ">=", "rhs": "11"},
            "body": {"type": "UDF",
                     "name": "SetLBoxTint",
                     "params": [50, 50, 50]},
        },
        {
            "condition": {"lhs": "Title::iDayNight", "op": "<=", "rhs": "10"},
            "body": {"type": "UDF",
                     "name": "SetFLightProps",
                     "params": [1, "I", "M"]},
        },
        {
            "condition": {"lhs": "Title::iCurrNode (+10 for links)",
                          "op": ">=", "rhs": "3"},
            "body": {"type": "Enable", "state": "Disable",
                     "target": "Asset: Navs:LoopsR:Comity Inn:068 - COMNAV3F"},
        },
        {
            "condition": {"lhs": "Title::iCurrNode (+10 for links)",
                          "op": ">=", "rhs": "3"},
            "body": {"type": "Enable", "state": "Disable",
                     "target": "CharView: Motel Clerk"},
        },
        {
            "condition": {"lhs": "Title::bN2HadEncounterWithNSA",
                          "op": "=", "rhs": "Title::bTRUE"},
            "body": {"type": "UDF", "name": "DoActionList",
                     "params": [80161, 80163]},
        },
        {
            "condition": {"lhs": "Title::cAIComityWhereAreWe",
                          "op": "=", "rhs": "C"},
            "body": {"type": "UDF", "name": "SetAIList",
                     "params": [59567, 59569]},
        },
        {
            "condition": {"lhs": "Title::cAIComityWhereAreWe",
                          "op": "=", "rhs": "N"},
            "body": {"type": "UDF", "name": "ClearAIList", "params": []},
        },
        {
            "condition": {"lhs": "Title::cAIComityWhereAreWe",
                          "op": "=", "rhs": "D"},
            "body": {"type": "UDF", "name": "ClearAIList", "params": []},
        },
        {
            "condition": {"lhs": "Title::bAIGeneral_IsItTimeToSleep",
                          "op": "=", "rhs": "Title::bTRUE"},
            "body": {"type": "UDF", "name": "SetAIList",
                     "params": [93293, 93294]},
        },
    ],
}


def to_json_dict() -> dict[str, Any]:
    """Return the full grammar as a JSON-serializable dict."""
    return {
        "_source": (
            "VirtualCinema/NeoLogic trigger system, "
            "documented in the research datasets (W20.4)"
        ),
        "architecture": TRIGGER_ARCHITECTURE,
        "event_types": [{
            "name": e.name,
            "description": e.description,
            "supported_on": list(e.supported_on),
        } for e in EVENT_TYPES],
        "action_types": [{
            "name": a.name,
            "description": a.description,
            "params": list(a.params),
        } for a in ACTION_TYPES],
        "expression_operators": list(EXPRESSION_OPERATORS),
        "statement_operators": list(STATEMENT_OPERATORS),
        "variable_scopes": list(VARIABLE_SCOPES),
        "variable_types": list(VARIABLE_TYPES),
        "example_real_trigger": EXAMPLE_COMITY_INN_NODE2,
        "counts": {
            "event_types": len(EVENT_TYPES),
            "action_types": len(ACTION_TYPES),
            "expression_operators": len(EXPRESSION_OPERATORS),
            "statement_operators": len(STATEMENT_OPERATORS),
            "variable_scopes": len(VARIABLE_SCOPES),
            "variable_types": len(VARIABLE_TYPES),
        },
    }
