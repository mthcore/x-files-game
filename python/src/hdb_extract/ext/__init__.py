"""Loaders for the auxiliary format-analysis ground-truth datasets.

These CSV files are the product of byte-direct format-analysis work.
They contain the decision tree, scene metadata, triggers, conversations,
emails, and endings — all of which are needed to navigate the game's
logic flow.

This module re-exposes them as Python data structures for use by the
hdb_extract decision_tree extractor.
"""
from hdb_extract.ext.decomp_intel import (
    load_actions,
    load_conversations,
    load_email_tree,
    load_endings_map,
    load_gam_variables,
    load_gam_variables_semantics,
    load_hotspots,
    load_scene_encyclopedia,
    load_triggers,
    load_variables_full,
)
from hdb_extract.ext.scene_assets import load_scene_asset_map
from hdb_extract.ext.trigger_system import to_json_dict as trigger_system_json

__all__ = [
    "load_actions",
    "load_conversations",
    "load_email_tree",
    "load_endings_map",
    "load_gam_variables",
    "load_gam_variables_semantics",
    "load_hotspots",
    "load_scene_asset_map",
    "load_scene_encyclopedia",
    "load_triggers",
    "load_variables_full",
    "trigger_system_json",
]
