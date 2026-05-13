---
id: tck-02-minimal-python-tck-runner
level: task
title: "TCK-02: Minimal Python TCK runner with SQLite extension adapter"
short_code: "GQLITE-T-0207"
created_at: 2026-05-13T12:51:00.936543+00:00
updated_at: 2026-05-13T13:29:35.425280+00:00
parent: GQLITE-I-0037
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0037
---

# TCK-02: Minimal Python TCK runner with SQLite extension adapter

## Parent Initiative

[[GQLITE-I-0037]]

## Objective

Build a small Python harness that parses openCypher TCK `.feature` files (Gherkin), executes their scenarios against the GraphQLite SQLite extension via in-memory `sqlite3`, and compares actual results / errors to the expected ones declared in the feature steps.

## Acceptance Criteria

## Acceptance Criteria

- [x] Harness lives under `tests/tck/` (Python package), imports no JVM dependencies.
- [x] Implements step definitions for the core TCK step vocabulary: graph setup (`Given an empty graph`, `Given the <name> graph`, `And having executed`), parameterization (`And parameters are`), execution (`When executing query`), expected results (`Then the result should be, in order` / `, in any order` / `should be empty`), expected errors (`Then a ... should be raised at ...`), and side-effects (`And no side effects` accepted; `And the side effects should be` marked skipped until counters are surfaced).
- [x] Result comparison is order-aware for `should be, in order` and multiset-based otherwise; value comparison handles nodes, relationships, paths, lists, maps, nulls, NaN/Inf per TCK textual forms.
- [x] Backend adapter abstraction (`Backend` protocol in `backends/base.py`) with `ExtensionBackend` concrete implementation that opens in-memory SQLite, loads `build/graphqlite.dylib` via `enable_load_extension`, and routes queries through `SELECT cypher(?)`.
- [x] Unknown step types or unsupported value forms produce `skipped` status with a diagnostic — verified by harness design (skipped_reason set, scenario short-circuits).
- [x] Smoke test runs end-to-end in ~30ms (well under 5s); pytest suite (10 tests) green.

## Implementation Notes

### Technical Approach
Use a hand-rolled Gherkin parser sufficient for TCK (regex-based — TCK uses a fixed step vocabulary, full Cucumber is overkill). Do NOT depend on `behave`/`pytest-bdd` unless they add real leverage; the goal is a minimal stack we control. Compare values using openCypher's textual literal forms documented in the TCK README.

### Dependencies
Depends on [[GQLITE-T-0206]] (TCK files must be vendored).

## Status Updates

### 2026-05-13 — Completed

Harness landed under `tests/tck/`:

- `gherkin.py` — minimal `.feature` parser (Feature/Scenario/Background/steps/docstrings/tables). No `behave`/`pytest-bdd` dep.
- `values.py` — parser + comparator for openCypher textual values: scalars, lists, maps, nodes (`(:A:B {k: v})`), relationships (`[:TYPE {...}]`), paths (`<(...)-[...]->(...)>`), null/NaN/Inf. Comparator is structural (node identity not observable).
- `backends/base.py` — `Backend` protocol + `QueryResult` dataclass.
- `backends/extension.py` — `ExtensionBackend` driving `build/graphqlite.dylib` via `sqlite3.connect(':memory:')` + `cypher(?)`. Wraps every backend call in a context manager that redirects fd 1 and fd 2 to `build/tck-debug.log` so the extension's `[CYPHER_DEBUG]` chatter doesn't pollute the harness's stdout/JSON.
- `runner.py` — step dispatcher + result-table comparator (ordered & multiset).
- `__main__.py` — CLI (`python -m tests.tck`) with `--features`, `--filter`, `--backend extension|python|rust|all`, `--out`, `--debug`, `--limit`. Selecting `python` or `rust` raises `backend not implemented` cleanly (lands in TCK-04).
- `tests/tck/tests/` — pytest suite: gherkin parser, value parser/comparator, end-to-end smoke against the extension. 10/10 green in 0.02s.

**Debug-noise note.** The extension emits debug to *stdout* (not just stderr); the silencer redirects both fds. Connection close was also wrapped after the first pass leaked a few free-side debug lines. Follow-up worth filing: add a compile-time `-DGQLITE_QUIET` switch in the extension itself.

**Known v1 limitation (intentional, deferred to TCK-05/06).** The extension's `cypher()` currently returns a single TEXT payload (e.g. `"[]"` for empty match). The harness keeps this opaque in `result.rows` for now; comparison vs. expected tables fails until result-row decoding is implemented. This is by design — the harness *measures* the gap rather than papering over it; the gap becomes triage input in TCK-05/06. Smoke run shows: `TCK: 5 scenarios — pass=0 fail=5 error=0 skipped=0` against `clauses/match/Match1.feature`, every failure with a precise diagnostic.

All acceptance criteria met. Transitioning to completed.