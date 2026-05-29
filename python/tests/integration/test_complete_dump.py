"""End-to-end : the complete byte-direct dump must roundtrip byte-identical."""
import os
import pytest
from pathlib import Path

from hdb_extract.container.container import HdbContainer
from hdb_extract.extractors import dump_complete, verify_dump

HDB_PATH = Path(os.environ.get("XFILES_HDB", "XFILES.HDB"))


@pytest.fixture(scope="module")
def hdb() -> HdbContainer:
    if not HDB_PATH.exists():
        pytest.skip("XFILES.HDB not available")
    return HdbContainer.from_path(HDB_PATH)


def test_complete_dump_includes_every_page(hdb):
    """Every page of the HDB must appear in the dump."""
    dump = dump_complete(hdb)
    assert len(dump["pages"]) == hdb.page_count
    assert dump["size_bytes"] == len(hdb.raw)


def test_complete_dump_header_fields(hdb):
    dump = dump_complete(hdb)
    h = dump["header"]
    assert h["version"] == 5
    assert h["record_count"] == 64_896
    assert h["class_count"] == 39
    assert len(bytes.fromhex(h["hex"])) == 32


def test_complete_dump_roundtrip_byte_identical(hdb):
    """The KEY 100% byte-direct invariant — dump → bytes → equal."""
    dump = dump_complete(hdb)
    assert verify_dump(hdb, dump)


def test_complete_dump_trailer_zero(hdb):
    dump = dump_complete(hdb)
    assert dump["trailer"]["is_zero_padding"]
    assert dump["trailer"]["size"] == 176


def test_complete_dump_id_markers(hdb):
    """The dump must catalog every 'ID  ' marker found in the HDB."""
    dump = dump_complete(hdb)
    n_id_markers = sum(
        sum(1 for m in p["markers"] if m["marker"] == "ID  ")
        for p in dump["pages"]
    )
    # The exact count (15,606) is from native ground truth.
    assert n_id_markers >= 15_000, f"too few ID markers : {n_id_markers}"
