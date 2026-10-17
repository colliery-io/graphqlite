"""Unit tests for the runner's test.* procedure emulation."""

from __future__ import annotations

from tests.tck.runner import (
    _ProcedureFixture,
    _State,
    _arg_matches_type,
    _maybe_call_procedure,
    _rewrite_embedded_yield_calls,
)


def test_argument_types():
    assert _arg_matches_type(1, "INTEGER?")
    assert not _arg_matches_type(True, "INTEGER?")
    assert _arg_matches_type(42, "FLOAT?")
    assert not _arg_matches_type("x", "FLOAT")
    assert _arg_matches_type(None, "STRING?")
    assert not _arg_matches_type(None, "STRING")
    assert _arg_matches_type(False, "BOOLEAN")


def test_wrong_argument_type_is_a_syntax_error():
    state = _State()
    state.procedures["test.my.proc"] = _ProcedureFixture(
        name="test.my.proc", arg_names=["in"], arg_types=["INTEGER?"], yield_names=["out"])
    r = _maybe_call_procedure("CALL test.my.proc(true) YIELD out RETURN out", state)
    assert r is not None and r.error == "SyntaxError"


def test_embedded_call_becomes_unwind():
    state = _State()
    state.procedures["test.labels"] = _ProcedureFixture(
        name="test.labels", yield_names=["label"], rows=[["A"], ["B"]])
    q = "CALL test.labels() YIELD label WITH count(*) AS c CALL test.labels() YIELD label AS l RETURN *"
    assert _rewrite_embedded_yield_calls(q, state) == (
        "UNWIND ['A', 'B'] AS label WITH count(*) AS c UNWIND ['A', 'B'] AS l RETURN *")
