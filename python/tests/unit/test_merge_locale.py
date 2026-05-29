"""Tests for the locale merger."""
from __future__ import annotations

import json

from hdb_extract.extractors.merge_locale import _walk_and_resolve


def test_resolves_text_id_at_top_level():
    tree = {"text_id": 1, "other": "x"}
    stats = {"resolved": 0, "unresolved": 0}
    _walk_and_resolve(tree, {1: "Hello"}, stats)
    assert tree["text_id"] == 1
    assert tree["text_text"] == "Hello"
    assert stats == {"resolved": 1, "unresolved": 0}


def test_resolves_nested():
    tree = {"conversations": [{"node_id": 5, "choices": [
        {"text_id": 10}, {"text_id": 11}
    ]}]}
    lookup = {10: "Yes", 11: "No"}
    stats = {"resolved": 0, "unresolved": 0}
    _walk_and_resolve(tree, lookup, stats)
    choices = tree["conversations"][0]["choices"]
    assert choices[0]["text_text"] == "Yes"
    assert choices[1]["text_text"] == "No"
    assert stats["resolved"] == 2


def test_unresolved_ids_counted():
    tree = {"text_id": 99}  # not in lookup
    stats = {"resolved": 0, "unresolved": 0}
    _walk_and_resolve(tree, {1: "x"}, stats)
    assert "text_text" not in tree
    assert stats == {"resolved": 0, "unresolved": 1}


def test_non_int_ids_ignored():
    """A key ending in _id whose value is not an int is not resolved."""
    tree = {"page_id": "abc"}
    stats = {"resolved": 0, "unresolved": 0}
    _walk_and_resolve(tree, {1: "x"}, stats)
    assert "page_text" not in tree
    assert stats == {"resolved": 0, "unresolved": 0}


def test_handles_multiple_id_kinds():
    tree = {"text_id": 1, "subject_id": 2, "label_id": 3}
    lookup = {1: "Hello", 2: "Subject", 3: "Label"}
    stats = {"resolved": 0, "unresolved": 0}
    _walk_and_resolve(tree, lookup, stats)
    assert tree["text_text"] == "Hello"
    assert tree["subject_text"] == "Subject"
    assert tree["label_text"] == "Label"
    assert stats["resolved"] == 3


def test_merge_and_write_end_to_end(tmp_path, monkeypatch):
    from hdb_extract.extractors import merge_locale

    # Stub LocaleDLL so the test doesn't need a real PE.
    class FakeLoc:
        def __init__(self, path):
            self.strings = {1: "hello", 2: "world"}

    monkeypatch.setattr(merge_locale, "LocaleDLL", FakeLoc)

    src = tmp_path / "tree.json"
    src.write_text(json.dumps({"items": [{"text_id": 1}, {"text_id": 2}]}),
                   encoding="utf-8")
    out = tmp_path / "merged.json"
    stats = merge_locale.merge_and_write(src, ["fake.dll"], out)

    merged = json.loads(out.read_text(encoding="utf-8"))
    assert merged["items"][0]["text_text"] == "hello"
    assert merged["items"][1]["text_text"] == "world"
    assert stats["resolved"] == 2
