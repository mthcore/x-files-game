"""Validate XT scripts extraction + classification."""
import os
import pytest
from pathlib import Path

from hdb_extract.extractors.xt_dump import dump_xt_scripts

XT_DIR = Path(os.environ.get("XT_DIR", "XT"))


@pytest.fixture(scope="module")
def xt_dump_data() -> dict:
    if not XT_DIR.exists():
        pytest.skip("XT dir not available")
    return dump_xt_scripts()


def test_total_count(xt_dump_data):
    # native doc says 474 XT files.
    assert xt_dump_data["total"] >= 470
    assert xt_dump_data["total"] <= 480


def test_categories_present(xt_dump_data):
    cats = xt_dump_data["by_category"]
    assert "email" in cats
    assert "journal" in cats
    assert "help" in cats
    assert "media" in cats
    assert "other" in cats


def test_email_count(xt_dump_data):
    # 345 emails per narrative_content.md (or ~350 with fuzzy boundaries).
    assert xt_dump_data["by_category"]["email"] >= 340
    assert xt_dump_data["by_category"]["email"] <= 360


def test_journal_count(xt_dump_data):
    # 42 Willmore diary entries.
    assert xt_dump_data["by_category"]["journal"] == 42


def test_help_count(xt_dump_data):
    assert xt_dump_data["by_category"]["help"] == 10


def test_script_has_preview(xt_dump_data):
    scripts = xt_dump_data["scripts"]
    assert any(s["preview"] for s in scripts[:50])


def test_script_shape(xt_dump_data):
    s = xt_dump_data["scripts"][0]
    assert "file_id" in s
    assert "filename" in s
    assert "category" in s
    assert "preview" in s
