"""Container layer : header parse + roundtrip."""
import os
import pytest
from pathlib import Path

from hdb_extract.container import HEADER_SIZE, HdbContainer, parse_header

HDB_PATH = Path(os.environ.get("XFILES_HDB", "XFILES.HDB"))


@pytest.fixture(scope="module")
def hdb() -> HdbContainer:
    if not HDB_PATH.exists():
        pytest.skip("XFILES.HDB not available")
    return HdbContainer.from_path(HDB_PATH)


def test_header_constants(hdb: HdbContainer):
    assert hdb.header.version == 5
    assert hdb.header.record_count == 64_896
    assert hdb.header.class_count == 39
    assert hdb.header.page_size == 256
    assert hdb.header.root_or_size == 5_375_656
    assert hdb.header.max_or_limit == 262_144


def test_page_count(hdb: HdbContainer):
    assert hdb.page_count == 23_726


def test_trailer_is_zero_176(hdb: HdbContainer):
    assert hdb.trailer.size == 176
    assert hdb.trailer.is_zero_padding


def test_page_distribution(hdb: HdbContainer):
    dist = hdb.page_type_distribution()
    assert dist[0x00] == 14_286  # empty
    assert dist[0xC2] == 989     # btree_internal
    assert dist[0xC0] == 754     # btree_freed
    assert dist[0xC3] == 202     # btree_leaf
    assert dist[0xD2] == 187     # btree_internal_alt


def test_roundtrip_byte_identical(hdb: HdbContainer):
    assert hdb.roundtrip_ok(), "container parse → reemit must be byte-identical"


def test_header_parse_too_small():
    with pytest.raises(ValueError):
        parse_header(b"\x00" * (HEADER_SIZE - 1))
