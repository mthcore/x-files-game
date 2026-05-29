"""Integration test for conversations.py wired to MasterHDBResolver
extension layer.

Covers :
  - The extractor stats counters
    ``{records_total, dll_resolved, handles_resolved, undetermined_total}``
    are internally coherent (no negative numbers, totals add up).
  - At least one record carries a handle-resolved slot
    (``target_offset / target_class_id``) byte-direct, proving the wiring
    actually consumes the extension layer.
  - Every undetermined entry carries a ``reason`` field — honesty rule.
  - The wiring is additive : previously byte-direct DLL slots still
    resolve to text after the change.
  - Optional injectable resolver: callers can pre-build and pass a
    resolver; the extractor must not rebuild a second one.
"""
from __future__ import annotations

import os
from pathlib import Path

import pytest

from hdb_extract.container.container import HdbContainer
from hdb_extract.btree.master_resolver import MARKER_MAGIC, MasterHDBResolver
from hdb_extract.extractors.conversations import (
    _classify_offset,
    extract_conversations,
)


HDB_PATH = Path(os.environ.get("XFILES_HDB", "XFILES.HDB"))
LOC_JSON = Path("G:/TheXFiles/order_recovery/loc_XFilest.json")


def _load_strings() -> dict[int, str]:
    import json
    if not LOC_JSON.exists():
        return {}
    d = json.load(open(LOC_JSON, encoding="utf-8"))
    return {int(k): v for k, v in d.get("strings", {}).items()}


@pytest.fixture(scope="module")
def container() -> HdbContainer:
    if not HDB_PATH.exists():
        pytest.skip("XFILES.HDB not available")
    return HdbContainer.from_path(HDB_PATH)


@pytest.fixture(scope="module")
def dll_strings() -> dict[int, str]:
    if not LOC_JSON.exists():
        pytest.skip("loc_XFilest.json not available")
    return _load_strings()


@pytest.fixture(scope="module")
def result(container: HdbContainer,
           dll_strings: dict[int, str]) -> dict:
    return extract_conversations(container, dll_strings)


# ---------------------------------------------------------------------------
# Pure tests (no HDB needed).
# ---------------------------------------------------------------------------

def test_classify_offset_valid_marker() -> None:
    """Synthesise a 16-byte buffer starting with a valid marker."""
    # Marker pattern: 0xC0..0xCF, sub-tag, 00 00 00 01, class_id BE u16.
    raw = bytes([0xC2, 0x00, 0, 0, 0, 1, 0x00, 0x31]) + bytes(8)
    info = _classify_offset(raw, 0)
    assert info is not None
    assert info["class_id"] == 0x31
    assert info["class_name"] == "VCConversation"


def test_classify_offset_wrong_magic() -> None:
    """A non-marker byte must be rejected."""
    raw = bytes([0xBF, 0x00, 0, 0, 0, 1, 0x00, 0x31]) + bytes(8)
    assert _classify_offset(raw, 0) is None


def test_classify_offset_wrong_version() -> None:
    """Marker byte ok but version bytes wrong -> reject."""
    raw = bytes([0xC2, 0x00, 0, 0, 0, 2, 0x00, 0x31]) + bytes(8)
    assert _classify_offset(raw, 0) is None


def test_classify_offset_past_eof() -> None:
    """Offsets beyond EOF must return None, never raise."""
    raw = bytes([0xC2, 0x00, 0, 0, 0, 1, 0x00, 0x31])
    # offset that would read past the buffer
    assert _classify_offset(raw, 1) is None
    assert _classify_offset(raw, 100) is None
    assert _classify_offset(raw, -1) is None


# ---------------------------------------------------------------------------
# HDB-driven tests (skipped when XFILES.HDB is not present).
# ---------------------------------------------------------------------------

def test_stats_coherent(result: dict) -> None:
    """All counters are non-negative and the totals add up."""
    stats = result["stats"]
    for key in ("records_total", "dll_resolved", "handles_resolved",
                "handles_resolved_extended", "handles_resolved_page_aligned",
                "handles_resolved_direct_offset",
                "handles_unresolved", "undetermined_total",
                "with_resolved_text", "with_resolved_handle",
                "graph_edges", "graph_nodes",
                "conversation_records", "list_containers",
                "list_container_entries"):
        assert key in stats, f"missing stats key {key!r}"
        assert stats[key] >= 0, f"{key} is negative: {stats[key]}"

    # handles_resolved is the sum of the source-tagged counters.
    assert (stats["handles_resolved"]
            == stats["handles_resolved_extended"]
            + stats["handles_resolved_page_aligned"]
            + stats["handles_resolved_direct_offset"])

    # records_total = conversation_records + list_containers + entries
    assert (stats["records_total"]
            == stats["conversation_records"]
            + stats["list_containers"]
            + stats["list_container_entries"])


def test_at_least_one_handle_resolved(result: dict, container: HdbContainer) -> None:
    """At least one record must carry a handle target proving the
    extension wiring actually fires. Honest fallback : if none surface,
    the test reports the actual zero rather than guessing — but the
    handle_range_analysis.json recon proved 12 handle hits exist on the
    shipped HDB, so this is a hard expectation when XFILES.HDB is real.
    """
    raw = container.raw
    handles = []
    for rec in result["records"]:
        for slot_name in ("prompt", "response_1", "response_2",
                          "conversation_id", "topic",
                          "next_conversation_id"):
            slot = rec.get(slot_name)
            if not isinstance(slot, dict):
                continue
            off = slot.get("target_offset")
            if off is None:
                continue
            # Every advertised target must really be a marker, byte-direct.
            assert 0xC0 <= raw[off] <= 0xCF, (
                f"slot {slot_name} of record at 0x{rec['record_offset']:x} "
                f"claims target_offset 0x{off:x} but byte is "
                f"0x{raw[off]:02x}"
            )
            assert raw[off + 2:off + 6] == MARKER_MAGIC
            assert slot.get("target_class_id") is not None
            assert slot.get("target_class_name") is not None
            assert slot.get("resolver_source") in (
                "extended", "page-aligned", "direct-offset")
            assert slot["certainty"] == "byte-direct"
            handles.append((rec["record_offset"], slot_name,
                            slot["target_class_id"], slot["resolver_source"]))

    assert handles, (
        "No record carried target_offset — the extension wiring did not "
        "produce any handle resolutions. Check the recon "
        "handle_range_analysis.json which expects ~12 hits."
    )


def test_every_undetermined_has_reason(result: dict) -> None:
    """Every undetermined slot in every record must carry a `reason`."""
    offending: list[str] = []
    for rec in result["records"]:
        for slot_name, slot in rec.items():
            if not isinstance(slot, dict):
                continue
            if slot.get("certainty") != "undetermined":
                continue
            if not slot.get("reason"):
                offending.append(
                    f"record 0x{rec['record_offset']:x} slot {slot_name}"
                )
    assert not offending, (
        f"Undetermined slot(s) without a reason : {offending[:5]}"
    )


def test_dll_resolutions_preserved(result: dict) -> None:
    """The wiring must be additive — every record that still carries a
    text-resolved DLL slot keeps `certainty=byte-direct` and a non-empty
    text."""
    for rec in result["records"]:
        for slot_name in ("prompt", "response_1", "response_2"):
            slot = rec.get(slot_name)
            if not isinstance(slot, dict):
                continue
            if slot.get("text"):
                assert slot["certainty"] == "byte-direct"
                # text resolution always sets string_id == raw_u32.
                assert slot.get("string_id") == slot.get("raw_u32")


def test_resolver_extension_stat_present(result: dict) -> None:
    """When the extractor builds its own resolver, the extension info
    must be exposed in stats for traceability."""
    stats = result["stats"]
    assert "resolver_extension_added" in stats
    assert stats["resolver_extension_added"] >= 30_000


def test_cross_conversation_edges_marked(result: dict) -> None:
    """Handle edges to class-0x31 targets carry kind='cross_conversation'.
    The test passes vacuously when no such edge exists on this HDB."""
    for edge in result["graph_edges"]:
        if edge.get("via") != "handle_target":
            continue
        if edge.get("target_class_id") == 0x31:
            assert edge.get("kind") == "cross_conversation", (
                f"edge {edge} targets class 0x31 but kind missing"
            )


def test_injectable_resolver_used(container: HdbContainer,
                                  dll_strings: dict[int, str]) -> None:
    """A pre-built resolver must be consumed as-is (no second build).
    The acceptance test : with a resolver that has NOT been extended,
    handle resolutions are zero. With an extended resolver, they are
    non-zero. Same container, same DLL strings — the difference proves
    the extractor honoured the injected resolver."""
    bare = MasterHDBResolver(container)  # no extend_with_*
    res_bare = extract_conversations(container, dll_strings, resolver=bare)

    extended = MasterHDBResolver(container)
    extended.extend_with_neoidlist_entries(scope="global")
    res_ext = extract_conversations(container, dll_strings, resolver=extended)

    # The bare resolver only knows page-aligned IDs; its extended counter
    # must be zero by construction.
    assert res_bare["stats"]["handles_resolved_extended"] == 0
    # The pre-extended resolver must surface at least one extended hit.
    assert res_ext["stats"]["handles_resolved_extended"] >= 1
    # And `resolver_extension_added` must NOT be in stats when the caller
    # passed their own resolver (we only record it for the lazy build).
    assert "resolver_extension_added" not in res_bare["stats"]
    assert "resolver_extension_added" not in res_ext["stats"]
