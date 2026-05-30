"""Cross-validate the engine's headless dispatcher output against the
known GAM variable namespace.

The dispatcher fires each step's triggers and parses their byte-direct
``effect_summary`` (``set <var> = true|false|<int>``) to mutate a
variable-state map. Every mutated variable should correspond to a real
``XFILES.GAM`` variable name. We tolerate a small percentage of
truncated names (the trigger_id label is read from a fixed-width buffer
in the on-disk record), but require the bulk to overlap with the GAM
namespace as a sanity check.

Requires:

  * ``XFILES_HDB`` env var pointing at the game file (for the validator);
  * the compiled ``xfiles_engine.exe`` in
    ``cpp/build/Release/xfiles_engine.exe`` (built via the CMake target).
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

import pytest


_OSS_ROOT = Path(__file__).resolve().parents[2].parent
_GAME_DEF = _OSS_ROOT / "examples" / "outputs" / "game_definition.json"
_GAM_VARS = _OSS_ROOT / "examples" / "outputs" / "gam_state_variables.json"
if sys.platform.startswith("win"):
    _ENGINE = _OSS_ROOT / "cpp" / "build" / "Release" / "xfiles_engine.exe"
else:
    _ENGINE = _OSS_ROOT / "cpp" / "build" / "xfiles_engine"


@pytest.mark.skipif(
    not _ENGINE.is_file() or not _GAME_DEF.is_file() or not _GAM_VARS.is_file(),
    reason="requires the compiled engine + game_definition.json + gam vars",
)
def test_dispatcher_overlap_with_gam(tmp_path: Path) -> None:
    out_json = tmp_path / "v.json"
    proc = subprocess.run(
        [str(_ENGINE),
         "--validate-flow", str(_GAME_DEF),
         "--json-out", str(out_json)],
        capture_output=True, text=True,
    )
    # Validator may return non-zero if any FAIL — we still want to inspect.
    assert out_json.is_file(), proc.stderr

    rep = json.loads(out_json.read_text(encoding="utf-8"))
    mutated = set(rep["dispatcher"]["variables"].keys())
    assert mutated, "dispatcher produced no mutations"

    gam = json.loads(_GAM_VARS.read_text(encoding="utf-8"))
    names: set[str] = set()
    for scope_vars in gam.get("by_scope", {}).values():
        if isinstance(scope_vars, list):
            for v in scope_vars:
                if isinstance(v, dict) and isinstance(v.get("name"), str):
                    names.add(v["name"])
    assert names, "no GAM variable names loaded"

    overlap = mutated & names
    pct = len(overlap) / len(mutated)
    assert pct >= 0.85, (
        f"only {len(overlap)}/{len(mutated)} dispatched vars are in the "
        f"GAM namespace ({pct:.1%}); missing examples: "
        f"{sorted(mutated - names)[:5]}"
    )
