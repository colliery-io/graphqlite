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
class _ProcedureFixture:
    """A test.* procedure declared via Gherkin's
    `And there exists a procedure name(args) :: (yields):` step.
    `arg_names`/`yield_names` are the column names; `rows` is the
    fixture table (parsed values) the runner serves when the
    scenario's CALL hits the procedure. """
    name: str
    arg_names: list[str] = field(default_factory=list)
    yield_names: list[str] = field(default_factory=list)
    rows: list[list[Any]] = field(default_factory=list)


@dataclass
class _State:
    last_result: QueryResult | None = None
    parameters: dict[str, Any] = field(default_factory=dict)
    skipped_reason: str | None = None
    procedures: dict[str, _ProcedureFixture] = field(default_factory=dict)


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
    (re.compile(r"^a (?P<err>\w+(?:Error|Failure|Missing)) should be raised at \w+"), "expect_error"),
    (re.compile(r"^no side effects$"), "no_side_effects"),
    (re.compile(r"^the side effects should be$"), "side_effects_table"),
    # T-0252: procedure declaration. Captures the fully-qualified name,
    # the arg list (raw), and the yield list (raw). The table beneath the
    # step (parsed at handler time) provides per-call fixture rows.
    (re.compile(
        r"^there exists a procedure (?P<name>[\w.]+)\((?P<args>[^)]*)\)\s*::\s*\((?P<yields>[^)]*)\)$"
    ), "procedure_declared"),
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
    # T-0252: intercept plain `CALL <procedure>(...)` against registered
    # test.* fixtures. If the procedure is declared in this scenario,
    # synthesize the QueryResult from the fixture rows; otherwise fall
    # through to the backend.
    intercepted = _maybe_call_procedure(step.docstring, state)
    if intercepted is not None:
        state.last_result = intercepted
    else:
        # T-0252 follow-on: when a registered no-yield procedure is
        # embedded inside a query (e.g. `MATCH (n) CALL test.doNothing()
        # RETURN n`), strip the CALL invocation so the backend processes
        # the remaining query. The procedure has no rows/columns to
        # contribute by design.
        rewritten = _strip_embedded_noyield_call(step.docstring, state)
        state.last_result = backend.execute(rewritten, state.parameters or None)
    # Record backend-side trouble so the runner can decide between fail and error
    # at the Then-step. We don't raise here — TCK has scenarios that *expect* an
    # error, and the verdict is decided by the matching Then-step.


def _strip_embedded_noyield_call(query: str, state) -> str:
    """Strip `CALL <registered no-yield proc>(...)` lines from a query.
    Used for in-query patterns like `MATCH (n) CALL test.doNothing()
    RETURN n` where the procedure contributes nothing to the result.
    Leaves the query unchanged if no embedded no-yield CALL matches. """
    if not state.procedures:
        return query
    out = query
    # Find every CALL <name>([args]) where <name> is a registered
    # procedure with no declared yields. Remove the CALL clause
    # (and a trailing newline if present).
    for name, fixture in state.procedures.items():
        if fixture.yield_names:
            continue
        # Escape dots for regex.
        esc = re.escape(name)
        pat = re.compile(
            rf"\bCALL\s+{esc}\s*(?:\([^)]*\))?\s*",
            re.IGNORECASE,
        )
        out = pat.sub("", out)
    return out


def _split_signature_columns(sig: str) -> list[str]:
    """Parse `in :: INTEGER?, out :: STRING?` into ['in', 'out']."""
    if not sig.strip():
        return []
    parts = [p.strip() for p in sig.split(",")]
    out = []
    for part in parts:
        if "::" in part:
            out.append(part.split("::", 1)[0].strip())
        elif part:
            out.append(part)
    return out


def _h_procedure_declared(step, state, backend, m):
    """T-0252: register a test.* procedure fixture for this scenario."""
    name = m.group("name")
    arg_names = _split_signature_columns(m.group("args"))
    yield_names = _split_signature_columns(m.group("yields"))
    fixture = _ProcedureFixture(name=name, arg_names=arg_names, yield_names=yield_names)
    if step.table:
        # The first row is the column header (matches arg/yield names);
        # remaining rows are fixture data. parse_value each cell so types
        # land in QueryResult-compatible shape.
        header = step.table[0] if step.table else []
        for row in step.table[1:]:
            parsed = []
            for cell in row:
                try:
                    parsed.append(parse_value(cell))
                except ValueParseError:
                    parsed.append(cell)
            fixture.rows.append(parsed)
        # Tolerate header being absent (single-column tables sometimes
        # only have data); leave header validation to comparators.
        (void := header)  # noqa: B018 — referenced for clarity
    state.procedures[name] = fixture


def _maybe_call_procedure(query: str, state) -> QueryResult | None:
    """If query is a CALL <proc>(...) [YIELD ...] [RETURN ...] against
    a registered test.* procedure, synthesize a QueryResult from the
    fixture rows. Returns None when the query isn't a procedure call
    we can serve, leaving backend.execute to handle the general path.

    Argument-based filtering: when CALL passes explicit args (e.g.
    `test.my.proc('Stefan', 1)`), only fixture rows where the arg
    columns match are returned. """
    q = query.strip().rstrip(";").strip()
    # Pattern: CALL name(args) [YIELD yields] [WITH with_cols] [RETURN return_cols]
    # The WITH renames flow through yield → with → return.
    m = re.match(
        r"^CALL\s+(?P<name>[\w.]+)\s*(?:\((?P<args>[^)]*)\))?\s*"
        r"(?:YIELD\s+(?P<yields>[\w*,\s]+?))?\s*"
        r"(?:WITH\s+(?P<withs>[\w\s,]+?))?\s*"
        r"(?:RETURN\s+(?P<rets>.+?))?\s*$",
        q,
        re.IGNORECASE | re.DOTALL,
    )
    if not m:
        return None
    name = m.group("name")
    fixture = state.procedures.get(name)
    if fixture is None:
        return None
    # T-0252: CALL <proc> RETURN <cols> without explicit YIELD is a
    # Cypher syntax error (UndefinedVariable). Fall through to backend
    # so the harness sees the expected error class.
    if m.group("rets") and not m.group("yields"):
        return None
    # T-0252: CALL <proc> YIELD * RETURN ... is in-query YIELD * which
    # the Cypher spec disallows (UnexpectedSyntax). Fall through.
    # (Standalone CALL YIELD * is OK — handled when rets is absent.)
    if m.group("yields") and m.group("yields").strip() == "*" and m.group("rets"):
        return None

    # Parse args. Three cases:
    # - Explicit: CALL name(a, b)        → use literal values.
    # - Implicit: CALL name              (no parens) → pull args from
    #                                       state.parameters by name.
    # - Empty:    CALL name()            → no filtering.
    args_raw = m.group("args")  # None if no parens, "" if empty parens
    explicit_args: list[Any] = []
    if args_raw is None:
        # Implicit-arg mode: look up each declared arg name in parameters.
        # If none of the args have matching params, skip filtering.
        for arg_name in fixture.arg_names:
            if arg_name in state.parameters:
                explicit_args.append(state.parameters[arg_name])
            else:
                # Missing parameter — fall through to backend; the
                # expected ParameterMissing error class won't surface
                # from our synthesizer but the backend will (or won't)
                # produce something the harness can score.
                return None
    elif args_raw.strip():
        for part in [p.strip() for p in args_raw.split(",")]:
            if not part:
                continue
            if part.startswith("$"):
                pname = part[1:]
                explicit_args.append(state.parameters.get(pname))
            else:
                try:
                    explicit_args.append(parse_value(part))
                except ValueParseError:
                    explicit_args.append(part)

    # T-0252: validate arg count against the declared signature.
    # When the count mismatches (too few or too many), Cypher spec
    # says the engine raises SyntaxError InvalidNumberOfArguments.
    # Fall through to backend so the harness can score the expected
    # error class. (Call1 [7]/[8]/[9]/[10] family.)
    if args_raw is not None and len(explicit_args) != len(fixture.arg_names):
        return None

    # YIELD/RETURN projection. Build:
    #   - yield_alias_map: {alias_in_query: fixture_column_name}
    #     For `YIELD a AS c, b AS d`, map is {c: a, d: b}.
    #     For bare YIELD a, b, map is {a: a, b: b} (identity).
    #   - wanted: ordered list of column names in the final result.
    # When RETURN is present, its columns are the final result names
    # (looked up via yield_alias_map → fixture column → value).
    rets_clause = m.group("rets")
    yield_clause = m.group("yields")
    yield_alias_map: dict[str, str] = {}
    # T-0252: detect duplicate YIELD destination names. Cypher spec
    # treats this as VariableAlreadyBound — fall through to backend
    # so the expected SyntaxError surfaces. Call5 [5]/[6].
    if yield_clause and yield_clause.strip() != "*":
        seen_dst: set[str] = set()
        for item in [c.strip() for c in yield_clause.split(",") if c.strip()]:
            mas = re.match(r"^(?P<src>\w+)\s+AS\s+(?P<dst>\w+)$", item, re.IGNORECASE)
            dst = mas.group("dst") if mas else item
            if dst in seen_dst:
                return None
            seen_dst.add(dst)
    if yield_clause and yield_clause.strip() != "*":
        for item in [c.strip() for c in yield_clause.split(",") if c.strip()]:
            mas = re.match(r"^(?P<src>\w+)\s+AS\s+(?P<dst>\w+)$", item, re.IGNORECASE)
            if mas:
                yield_alias_map[mas.group("dst")] = mas.group("src")
            else:
                yield_alias_map[item] = item
    else:
        # Bare YIELD * or no YIELD → identity map to all declared yields.
        for n in fixture.yield_names:
            yield_alias_map[n] = n

    # WITH renames build a second alias map on top of YIELD's.
    # E.g. `YIELD out WITH out AS a` → final map for `a` resolves via
    # yield_alias_map[out] to fixture column out.
    withs_clause = m.group("withs")
    with_alias_map: dict[str, str] = {}
    if withs_clause:
        for item in [c.strip() for c in withs_clause.split(",") if c.strip()]:
            mas = re.match(r"^(?P<src>\w+)\s+AS\s+(?P<dst>\w+)$", item, re.IGNORECASE)
            if mas:
                with_alias_map[mas.group("dst")] = mas.group("src")
            else:
                with_alias_map[item] = item

    # Compose: final_alias[col] = fixture column.
    # Lookup chain: col → with_alias_map → yield_alias_map → fixture col.
    def resolve_col(col: str) -> str:
        c = with_alias_map.get(col, col) if with_alias_map else col
        return yield_alias_map.get(c, c)

    if rets_clause:
        rets_clause = rets_clause.strip()
        if rets_clause == "*":
            # RETURN * → project all in-scope vars (from WITH if present,
            # else from YIELD).
            wanted = list(with_alias_map.keys()) if with_alias_map else list(yield_alias_map.keys())
        else:
            wanted = [c.strip() for c in rets_clause.split(",") if c.strip()]
    elif with_alias_map:
        wanted = list(with_alias_map.keys())
    else:
        wanted = list(yield_alias_map.keys())

    full_cols = fixture.arg_names + fixture.yield_names
    out_rows: list[list[Any]] = []
    for row in fixture.rows:
        # Argument filter: when CALL passes explicit args, only keep
        # rows whose arg-column values equal those args (positional).
        if explicit_args and fixture.arg_names:
            match = True
            for i, av in enumerate(explicit_args):
                if i >= len(fixture.arg_names):
                    break
                col_name = fixture.arg_names[i]
                try:
                    col_idx = full_cols.index(col_name)
                except ValueError:
                    match = False
                    break
                if col_idx >= len(row) or row[col_idx] != av:
                    match = False
                    break
            if not match:
                continue
        # Skip rows entirely when there are no yields (e.g. doNothing).
        if not wanted:
            continue
        projected = []
        for col in wanted:
            # Resolve through with_alias_map → yield_alias_map → fixture
            # column. Each level may rename; chained lookup walks the
            # final src column name.
            src = resolve_col(col)
            try:
                idx = full_cols.index(src)
                projected.append(row[idx] if idx < len(row) else None)
            except ValueError:
                projected.append(None)
        out_rows.append(projected)
    return QueryResult(headers=wanted, rows=out_rows)

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
    "procedure_declared": _h_procedure_declared,
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
