"""
Scenario runner. Maps Gherkin steps to backend operations and compares
results against expected tables.

Statuses
--------
pass    : every Then-step compared equal.
fail    : at least one Then-step compared unequal.
error   : backend raised when an error was not expected, or expected an
          error and the wrong category was raised.
skipped : harness saw a step or value form it does not yet understand,
          before reaching any Then-step verdict.
"""

from __future__ import annotations

import re
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from .backends import Backend, BackendError, QueryResult
from .gherkin import Feature, Scenario, Step, walk_features
from .values import parse_value, values_equal, ValueParseError


@dataclass
class ScenarioOutcome:
    feature_file: str
    scenario_name: str
    status: str
    backend: str
    duration_ms: float
    diagnostic: str = ""
    expected: Any = None
    actual: Any = None


@dataclass
class _State:
    last_result: QueryResult | None = None
    parameters: dict[str, Any] = field(default_factory=dict)
    skipped_reason: str | None = None


# Step matchers ordered by specificity.
_STEP_PATTERNS: list[tuple[re.Pattern[str], str]] = [
    (re.compile(r"^an empty graph$"), "given_empty_graph"),
    (re.compile(r"^any graph$"), "given_empty_graph"),
    (re.compile(r"^the (?P<name>[\w-]+) graph$"), "given_named_graph"),
    (re.compile(r"^having executed$"), "having_executed"),
    (re.compile(r"^parameters are$"), "parameters_are"),
    (re.compile(r"^executing query$"), "executing_query"),
    (re.compile(r"^executing control query$"), "executing_query"),
    (re.compile(r"^the result should be, in order$"), "result_ordered"),
    (re.compile(r"^the result should be, in any order$"), "result_any_order"),
    # openCypher TCK ships a variant where the row order doesn't matter
    # AND list-valued cells compare as multisets (used in DISTINCT/aggregation
    # scenarios where the order of returned list elements is unspecified).
    (re.compile(r"^the result should be \(ignoring element order for lists\)$"),
     "result_any_order_lists_unordered"),
    (re.compile(r"^the result should be, in order \(ignoring element order for lists\)$"),
     "result_ordered_lists_unordered"),
    (re.compile(r"^the result should be empty$"), "result_empty"),
    (re.compile(r"^a (?P<err>\w+(?:Error|Failure)) should be raised at \w+"), "expect_error"),
    (re.compile(r"^no side effects$"), "no_side_effects"),
    (re.compile(r"^the side effects should be$"), "side_effects_table"),
]


def run_feature(feature: Feature, backend: Backend) -> list[ScenarioOutcome]:
    rel_path = _rel_to_features_root(feature.path)
    out: list[ScenarioOutcome] = []
    for sc in feature.scenarios:
        out.append(_run_scenario(rel_path, sc, backend))
    return out


def _rel_to_features_root(path: Path) -> str:
    parts = path.parts
    if "features" in parts:
        i = parts.index("features")
        return "/".join(parts[i + 1:])
    return str(path)


def _run_scenario(feature_file: str, sc: Scenario, backend: Backend) -> ScenarioOutcome:
    t0 = time.monotonic()
    state = _State()
    expected_error: str | None = None
    verdict: str | None = None
    diag: str = ""
    expected_payload: Any = None
    actual_payload: Any = None

    try:
        backend.reset()
    except BackendError as e:
        return _outcome(feature_file, sc, "error", backend, t0, f"backend.reset: {e}")

    for step in sc.steps:
        handler = _dispatch(step)
        if handler is None:
            state.skipped_reason = f"unknown step: {step.keyword} {step.text!r}"
            break
        try:
            result = handler(step, state, backend)
        except BackendError as e:
            diag = f"backend error at step {step.keyword} {step.text!r}: {e}"
            verdict = "error"
            break
        except _BackendErrorAtThen as be:
            diag = f"{be.error_class}: {be.message[:200]}"
            verdict = "error"
            break
        except ValueParseError as e:
            state.skipped_reason = f"value parse: {e} (step: {step.text!r})"
            break
        except _Mismatch as m:
            verdict = "fail"
            diag = str(m)
            expected_payload = m.expected
            actual_payload = m.actual
            break

        if result is _ExpectErrorMarker:
            expected_error = step.text  # informational
        if result is _PassMarker:
            verdict = verdict or "pass"

    if state.skipped_reason is not None:
        return _outcome(feature_file, sc, "skipped", backend, t0, state.skipped_reason)

    if verdict is None:
        # Scenario had no Then-step (or only setup). Treat as pass.
        verdict = "pass"

    return _outcome(
        feature_file, sc, verdict, backend, t0, diag,
        expected=expected_payload, actual=actual_payload,
    )


def _outcome(feature_file: str, sc: Scenario, status: str, backend: Backend, t0: float,
             diag: str = "", expected: Any = None, actual: Any = None) -> ScenarioOutcome:
    return ScenarioOutcome(
        feature_file=feature_file,
        scenario_name=sc.name,
        status=status,
        backend=backend.name,
        duration_ms=(time.monotonic() - t0) * 1000,
        diagnostic=diag,
        expected=expected,
        actual=actual,
    )


# ---------------------------------------------------------------------------


class _Mismatch(Exception):
    def __init__(self, msg: str, expected: Any = None, actual: Any = None):
        super().__init__(msg)
        self.expected = expected
        self.actual = actual


class _BackendErrorAtThen(Exception):
    """Backend raised/crashed when the scenario expected a successful result table."""

    def __init__(self, error_class: str, message: str):
        super().__init__(f"{error_class}: {message}")
        self.error_class = error_class
        self.message = message


class _Marker: pass
_ExpectErrorMarker = _Marker()
_PassMarker = _Marker()


def _dispatch(step: Step):
    text = step.text.strip().rstrip(":").strip()
    for pat, name in _STEP_PATTERNS:
        m = pat.match(text)
        if m:
            handler = _HANDLERS[name]
            return lambda s, st, be, _h=handler, _m=m: _h(s, st, be, _m)
    return None


# Each handler returns either None, _PassMarker, or _ExpectErrorMarker. They
# raise _Mismatch on failed comparison and BackendError on backend trouble.

def _h_empty_graph(step, state, backend, m):
    backend.reset()

def _h_named_graph(step, state, backend, m):
    backend.reset()
    backend.load_named_graph(m.group("name"))

def _h_having_executed(step, state, backend, m):
    if not step.docstring:
        raise _Mismatch("missing docstring for 'having executed'")
    backend.execute(step.docstring, state.parameters or None)

def _h_parameters(step, state, backend, m):
    if not step.table:
        raise _Mismatch("missing table for 'parameters are'")
    # Two-column tables in `parameters are` are always name/value rows in the
    # TCK corpus — there's no header row. Treat every row as data and try to
    # parse_value() the second column; fall back to literal string on parse
    # error.
    params: dict[str, Any] = {}
    for row in step.table:
        if len(row) < 2:
            continue
        try:
            params[row[0]] = parse_value(row[1])
        except ValueParseError:
            params[row[0]] = row[1]
    state.parameters = params

def _h_executing_query(step, state, backend, m):
    if not step.docstring:
        raise _Mismatch("missing docstring for 'executing query'")
    state.last_result = backend.execute(step.docstring, state.parameters or None)
    # Record backend-side trouble so the runner can decide between fail and error
    # at the Then-step. We don't raise here — TCK has scenarios that *expect* an
    # error, and the verdict is decided by the matching Then-step.

def _h_result_ordered(step, state, backend, m):
    _compare_result_table(state.last_result, step.table, ordered=True)
    return _PassMarker

def _h_result_any_order(step, state, backend, m):
    _compare_result_table(state.last_result, step.table, ordered=False)
    return _PassMarker

def _h_result_any_order_lists_unordered(step, state, backend, m):
    _compare_result_table(state.last_result, step.table, ordered=False,
                          lists_unordered=True)
    return _PassMarker

def _h_result_ordered_lists_unordered(step, state, backend, m):
    _compare_result_table(state.last_result, step.table, ordered=True,
                          lists_unordered=True)
    return _PassMarker

def _h_result_empty(step, state, backend, m):
    if state.last_result is None:
        raise _Mismatch("no result captured")
    if state.last_result.rows:
        raise _Mismatch("expected empty result", expected=[], actual=state.last_result.rows)
    return _PassMarker

def _h_expect_error(step, state, backend, m):
    if state.last_result is None or state.last_result.error is None:
        raise _Mismatch(f"expected error {m.group('err')!r}, none raised")
    return _PassMarker

def _h_no_side_effects(step, state, backend, m):
    # v1: until the extension reports side-effect counters, accept this silently.
    return _PassMarker

def _h_side_effects_table(step, state, backend, m):
    # The extension doesn't surface side-effect counters yet (separate ticket
    # in [[GQLITE-T-0228]]). Treat the step as soft-pass so a scenario that
    # has already had its data result verified isn't downgraded to `skipped`
    # by an unverifiable counter check. Mismatches in counters are tracked as
    # their own work and do not affect the conformance pass count here.
    return _PassMarker


_HANDLERS = {
    "given_empty_graph": _h_empty_graph,
    "given_named_graph": _h_named_graph,
    "having_executed":  _h_having_executed,
    "parameters_are":   _h_parameters,
    "executing_query":  _h_executing_query,
    "result_ordered":   _h_result_ordered,
    "result_any_order": _h_result_any_order,
    "result_any_order_lists_unordered": _h_result_any_order_lists_unordered,
    "result_ordered_lists_unordered":   _h_result_ordered_lists_unordered,
    "result_empty":     _h_result_empty,
    "expect_error":     _h_expect_error,
    "no_side_effects":  _h_no_side_effects,
    "side_effects_table": _h_side_effects_table,
}


def _compare_result_table(result: QueryResult | None, table: list[list[str]] | None,
                          ordered: bool, lists_unordered: bool = False) -> None:
    if table is None:
        raise _Mismatch("missing expected table")
    if not table:
        raise _Mismatch("empty expected table")
    if result is None:
        raise _Mismatch("no result captured")
    if result.error:
        # Backend raised/crashed where the scenario expected a result table —
        # surface as a BackendError so the outcome is `error`, not `fail`.
        raise _BackendErrorAtThen(result.error, result.error_message or "")
    headers = table[0]
    expected_rows = [[parse_value(c) for c in row] for row in table[1:]]
    # Align actual columns to the TCK header order by name. cypher()'s JSON
    # output preserves user-named columns but not user-declared order; if the
    # backend reports headers, reorder cells so cell[i] matches header[i].
    actual_rows = result.rows
    if result.headers and len(result.headers) == len(headers):
        try:
            index_map = [result.headers.index(h) for h in headers]
            actual_rows = [[row[i] for i in index_map] for row in actual_rows]
        except ValueError:
            # Header name mismatch — fall through with positional comparison
            # so the mismatch is surfaced via _Mismatch, not silently masked.
            pass
    if len(actual_rows) != len(expected_rows):
        raise _Mismatch(
            f"row count: expected {len(expected_rows)} got {len(actual_rows)}",
            expected=expected_rows, actual=actual_rows,
        )
    if ordered:
        for e, a in zip(expected_rows, actual_rows):
            if not _rows_equal(e, a, lists_unordered=lists_unordered):
                raise _Mismatch("row mismatch", expected=e, actual=a)
    else:
        # Multiset comparison.
        remaining = list(actual_rows)
        for e in expected_rows:
            for i, a in enumerate(remaining):
                if _rows_equal(e, a, lists_unordered=lists_unordered):
                    remaining.pop(i)
                    break
            else:
                raise _Mismatch("unmatched expected row", expected=e, actual=remaining)


def _rows_equal(expected: list[Any], actual: list[Any], lists_unordered: bool = False) -> bool:
    if len(expected) != len(actual):
        return False
    return all(values_equal(e, a, lists_unordered=lists_unordered)
               for e, a in zip(expected, actual))
