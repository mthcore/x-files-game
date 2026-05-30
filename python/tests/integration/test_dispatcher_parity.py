"""Python dispatcher ↔ C++ engine dispatcher parity.

The two implementations of the same logic (`extractors/dispatcher.py` and
`cpp/src/engine/xfiles_engine.cpp`) must produce identical playthrough
results on the same `game_definition.json`. Mismatch = either side drifted.
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

import pytest

from hdb_extract.extractors.dispatcher import dispatch_playthrough


_OSS_ROOT = Path(__file__).resolve().parents[2].parent
_GAME_DEF = _OSS_ROOT / "examples" / "outputs" / "game_definition.json"
if sys.platform.startswith("win"):
    _ENGINE = _OSS_ROOT / "cpp" / "build" / "Release" / "xfiles_engine.exe"
else:
    _ENGINE = _OSS_ROOT / "cpp" / "build" / "xfiles_engine"


@pytest.mark.skipif(
    not _GAME_DEF.is_file(),
    reason="requires examples/outputs/game_definition.json",
)
def test_python_dispatcher_smoke() -> None:
    gd = json.loads(_GAME_DEF.read_text(encoding="utf-8"))
    rep = dispatch_playthrough(gd)
    assert rep["steps_dispatched"] >= 1
    assert rep["triggers_dispatched"] >= 1
    assert rep["effects_uninterpreted"] == 0, (
        "all byte-direct effect_summaries should match the grammar; "
        f"saw {rep['effects_uninterpreted']} uninterpreted"
    )
    assert len(rep["timeline"]) == rep["steps_dispatched"]


@pytest.mark.skipif(
    not _ENGINE.is_file() or not _GAME_DEF.is_file(),
    reason="requires the compiled engine + game_definition.json",
)
def test_python_cpp_dispatcher_parity(tmp_path: Path) -> None:
    out_json = tmp_path / "cpp.json"
    proc = subprocess.run(
        [str(_ENGINE),
         "--validate-flow", str(_GAME_DEF),
         "--json-out", str(out_json)],
        capture_output=True, text=True,
    )
    assert out_json.is_file(), proc.stderr

    cpp = json.loads(out_json.read_text(encoding="utf-8"))["dispatcher"]
    py = dispatch_playthrough(json.loads(_GAME_DEF.read_text(encoding="utf-8")))

    # The two implementations must agree on the headline counts.
    assert cpp["steps_dispatched"] == py["steps_dispatched"]
    assert cpp["triggers_dispatched"] == py["triggers_dispatched"]
    assert cpp["effects_applied"] == py["effects_applied"]
    assert cpp["effects_uninterpreted"] == py["effects_uninterpreted"]

    # Final variable maps must be set-equal.
    assert set(cpp["variables"].keys()) == set(py["variables"].keys())
    for k, v in py["variables"].items():
        assert cpp["variables"][k] == v, f"value mismatch on {k}"
