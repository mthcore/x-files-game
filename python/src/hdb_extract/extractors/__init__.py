"""Per-category extractors.

Each extractor inspects HDB content and produces a JSON-serializable
structure for a specific facet (flows, scenes, hotspots, etc.).

For MVP, extractors that depend on the **packed wire format** (G1) are
stubbed — they output the records they can identify by surface markers
(CNeoIDList ID→offset entries, production-path patterns, etc.) and note
what's still pending in their output.
"""
from hdb_extract.extractors.classes_dump import dump_classes
from hdb_extract.extractors.complete_dump import dump_complete, verify_dump
from hdb_extract.extractors.decision_tree import build_decision_tree
from hdb_extract.extractors.decoded_records import extract_decoded_records
from hdb_extract.extractors.neoidlist_dump import dump_neoidlist
from hdb_extract.extractors.page_index import dump_page_index
from hdb_extract.extractors.production_paths import dump_production_paths
from hdb_extract.extractors.scene_json import build_scene_json, write_scenes_json
from hdb_extract.extractors.scene_neologic import write_scene_nl_files
from hdb_extract.extractors.scene_readable import write_scenes_md
from hdb_extract.extractors.scene_storyboard import write_storyboards
from hdb_extract.extractors.strings_dump import dump_strings
from hdb_extract.extractors.triggers_full import build_triggers_full, enrich_trigger
from hdb_extract.extractors.typed_records import extract_typed_records
from hdb_extract.btree.leaf_resolver import build_resolved_catalog
from hdb_extract.extractors.xt_dump import dump_xt_scripts, organize_xt_by_category

__all__ = [
    "build_decision_tree",
    "build_scene_json",
    "build_triggers_full",
    "dump_classes",
    "dump_complete",
    "dump_neoidlist",
    "dump_page_index",
    "dump_production_paths",
    "dump_strings",
    "dump_xt_scripts",
    "build_resolved_catalog",
    "enrich_trigger",
    "extract_decoded_records",
    "extract_typed_records",
    "organize_xt_by_category",
    "verify_dump",
    "write_scene_nl_files",
    "write_scenes_json",
    "write_scenes_md",
    "write_storyboards",
]
