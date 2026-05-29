"""Validate the trigger system grammar (9 events + 8 actions + 8 ops)."""
from hdb_extract.ext.trigger_system import (
    ACTION_TYPES,
    EVENT_TYPES,
    EXPRESSION_OPERATORS,
    STATEMENT_OPERATORS,
    VARIABLE_SCOPES,
    VARIABLE_TYPES,
    to_json_dict,
)


def test_event_types_count():
    assert len(EVENT_TYPES) == 9
    names = {e.name for e in EVENT_TYPES}
    assert "MouseClick" in names
    assert "TimerExpiration" in names
    assert "ObjectActivation" in names


def test_action_types_count():
    assert len(ACTION_TYPES) == 8
    names = {a.name for a in ACTION_TYPES}
    assert "Statement" in names
    assert "UDF" in names
    assert "Asset" in names


def test_expression_operators_count():
    assert len(EXPRESSION_OPERATORS) == 8
    ops = {o["op"] for o in EXPRESSION_OPERATORS}
    assert {"=", "!=", ">", "<", ">=", "<=", "and", "or"} == ops


def test_statement_operators_count():
    assert len(STATEMENT_OPERATORS) == 8


def test_variable_scopes():
    assert "Title" in VARIABLE_SCOPES
    assert "Local" in VARIABLE_SCOPES


def test_variable_types_count():
    assert len(VARIABLE_TYPES) >= 8


def test_to_json_dict_shape():
    d = to_json_dict()
    assert d["counts"]["event_types"] == 9
    assert d["counts"]["action_types"] == 8
    assert d["counts"]["expression_operators"] == 8
    assert d["example_real_trigger"]["context"]
    assert len(d["example_real_trigger"]["actions"]) >= 10
