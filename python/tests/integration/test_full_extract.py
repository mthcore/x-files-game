"""End-to-end : extract real XFILES.HDB and validate counts vs format-analysis ground truth."""
import os
import pytest
from pathlib import Path

from hdb_extract.container.container import HdbContainer
from hdb_extract.extractors import (
    dump_classes,
    dump_neoidlist,
    dump_production_paths,
)

HDB_PATH = Path(os.environ.get("XFILES_HDB", "XFILES.HDB"))


@pytest.fixture(scope="module")
def hdb() -> HdbContainer:
    if not HDB_PATH.exists():
        pytest.skip("XFILES.HDB not available")
    return HdbContainer.from_path(HDB_PATH)


def test_extract_neoidlist(hdb):
    """We must find more than 13,000 CNeoIDList records (native says 15,606
    visible markers but some are at edge offsets)."""
    nl = dump_neoidlist(hdb)
    assert nl["total"] >= 13_000
    assert nl["total"] <= 16_000
    # All ID records claim class_id 0x0b (CNeoIDList).
    for rec in nl["ids"][:100]:
        assert rec["class_id"] == 0x0b


def test_extract_production_paths(hdb):
    """We must find ~2,432 production paths (native ground truth)."""
    paths = dump_production_paths(hdb)
    assert paths["total"] >= 2_400, f"too few paths : {paths['total']}"
    assert paths["total"] <= 2_500, f"suspicious extras : {paths['total']}"


def test_extract_production_paths_scenes(hdb):
    """Key scenes from native ground truth must be present."""
    paths = dump_production_paths(hdb)
    scenes = {p["scene"] for p in paths["paths"]}
    expected = {
        "End Games", "Field Office", "Wong", "Warehouse", "Apt", "Coroner",
        "Secret Base", "Rail Yard", "Tarakan", "Hospital", "Emails",
    }
    missing = expected - scenes
    assert not missing, f"missing scenes : {missing}"


def test_extract_production_paths_assets(hdb):
    """The asset extension distribution must match native expectations."""
    paths = dump_production_paths(hdb)
    by_ext: dict[str, int] = {}
    for p in paths["paths"]:
        by_ext[p["asset_ext"]] = by_ext.get(p["asset_ext"], 0) + 1
    # native ground truth : xmv 1778, xtx 475, amv 99, mus 61, pff 19
    assert by_ext.get("xmv", 0) >= 1_700
    assert by_ext.get("xtx", 0) >= 470 and by_ext.get("xtx", 0) <= 480
    assert by_ext.get("pff", 0) >= 18 and by_ext.get("pff", 0) <= 20


def test_classes_documented():
    cls = dump_classes()
    assert cls["total_classes"] == 51
    # All have at least the base init op.
    for c in cls["classes"]:
        assert c["n_ops"] >= 1


def test_container_invariants(hdb):
    """Header invariants for XFILES.HDB v5."""
    assert hdb.header.version == 5
    assert hdb.header.record_count == 64_896
    assert hdb.header.page_size == 256
    assert hdb.trailer.size == 176
    assert hdb.trailer.is_zero_padding
    assert hdb.roundtrip_ok()
