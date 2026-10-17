"""Unit tests for the strict checks of the runner (GQLITE-T-0429)."""

from __future__ import annotations

import pytest

from tests.tck._extension_worker import _classify
from tests.tck.backends.base import QueryResult
from tests.tck.runner import (
    _Mismatch,
    _ProcedureFixture,
    _State,
    _check_side_effects,
    _compare_result_table,
    _diff_side_effects,
    _maybe_call_procedure,
)


def test_classify_reads_the_class_the_engine_names():
    assert _classify('{"error":"TypeError: InvalidArgumentValue: x","code":"EXECUTION_ERROR"}') == "TypeError"
    assert _classify('{"error":"Line 1, Col 3: syntax error","code":"PARSE_ERROR"}') == "SyntaxError"
    # no guessing from words in the message
    assert _classify('{"error":"type mismatch in syntax","code":"EXECUTION_ERROR"}') == "GraphQLiteError"


def test_side_effects_are_a_graph_state_diff():
    before = {"nodes": [1], "rels": [], "labels": ["A"], "props": [["node", 1, "k", "int", 1]]}
    after = {"nodes": [1, 2], "rels": [5], "labels": ["A", "B"],
             "props": [["node", 1, "k", "int", 2]]}
    d = _diff_side_effects(before, after)
    assert d == {"+nodes": 1, "-nodes": 0, "+relationships": 1, "-relationships": 0,
                 "+labels": 1, "-labels": 0, "+properties": 1, "-properties": 1}
    state = _State()
    state.side_effects = d
    _check_side_effects(state, {"+nodes": 1, "+relationships": 1, "+labels": 1,
                                "+properties": 1, "-properties": 1})
    with pytest.raises(_Mismatch):
        _check_side_effects(state, {})


def test_columns_must_match_by_name():
    r = QueryResult(headers=["x"], rows=[[1]])
    with pytest.raises(_Mismatch):
        _compare_result_table(r, [["y"], ["1"]], ordered=True)
    _compare_result_table(r, [["x"], ["1"]], ordered=True)


def test_registered_procedure_errors():
    state = _State()
    state.procedures["test.my.proc"] = _ProcedureFixture(
        name="test.my.proc", arg_names=["in"], arg_types=["INTEGER?"], yield_names=["out"])
    r = _maybe_call_procedure("CALL test.my.proc(1, 2)", state)
    assert r.error == "SyntaxError" and "InvalidNumberOfArguments" in r.error_message
    r = _maybe_call_procedure("CALL test.my.proc YIELD out RETURN out", state)
    assert r.error == "SyntaxError" and "InvalidArgumentPassingMode" in r.error_message
    r = _maybe_call_procedure("CALL test.my.proc", state)
    assert r.error == "ParameterMissing"
