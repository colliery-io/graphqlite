---
id: tck-04-python-and-rust-binding
level: task
title: "TCK-04: Python and Rust binding backend adapters + parity matrix"
short_code: "GQLITE-T-0209"
created_at: 2026-05-13T12:51:03.239674+00:00
updated_at: 2026-05-13T13:40:01.680747+00:00
parent: GQLITE-I-0037
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0037
---

# TCK-04: Python and Rust binding backend adapters + parity matrix

## Parent Initiative

[[GQLITE-I-0037]]

## Objective

Extend the harness with two additional backend adapters so the same TCK scenario corpus runs through the Python and Rust bindings, and produce a cross-backend parity matrix flagging any scenario where the three entry points disagree.

## Acceptance Criteria

## Acceptance Criteria

- [x] `PythonBindingBackend` drives the in-process Python binding (`graphqlite.connect` + `Connection.cypher`); imports the local `bindings/python/src` first so checkouts don't need pip-install.
- [x] `RustBindingBackend` talks to `bindings/rust/examples/tck_runner` — a long-lived REPL subprocess speaking one-JSON-per-line on stdio (`reset`/`execute`/`shutdown`). Long-lived worker chosen over subprocess-per-query to avoid Rust startup cost; documented in the file header.
- [x] `python -m tests.tck --backend all` runs all three backends, writes `build/tck-results.json` (per-scenario records, tagged with `backend`) and `build/tck-parity.json` with `{feature_file, scenario_name, statuses: {extension, python, rust}, divergence: bool}`.
- [x] Parity row marks `divergence: true` if backend statuses differ, OR (when all-pass) if `actual` values differ.
- [x] Summary prints per-backend totals + per-backend status breakdown for diverged scenarios.

## Implementation Notes

### Technical Approach
Subprocess overhead for the Rust adapter is acceptable for v1; if it dominates runtime, revisit with a long-lived REPL-style worker. Result-value comparison across backends must normalize representation differences that are not semantic (e.g. integer vs floating textual forms).

### Dependencies
Depends on [[GQLITE-T-0207]]. Can run in parallel with [[GQLITE-T-0210]].

## Status Updates

### 2026-05-13 — Completed

Three pieces landed:

1. **`bindings/rust/examples/tck_runner.rs`** — long-lived REPL in Rust. Reads `{"cmd": "reset"|"execute"|"shutdown", ...}` lines on stdin, writes one JSON line per request to stdout. On `execute`, serializes `CypherResult` by iterating `columns()` × `get_value(col)` (no public `to_json` on the result type yet — follow-up worth filing). Stderr (and the extension's `[CYPHER_DEBUG]` spam) is captured by the Python-side adapter into `build/tck-debug.log`. Build: `cargo build --example tck_runner --manifest-path bindings/rust/Cargo.toml`.

2. **`tests/tck/backends/python_binding.py`** — in-process adapter. Adds `bindings/python/src` to `sys.path` so a checkout works without `pip install`. Drives `graphqlite.connect(":memory:")` and `Connection.cypher(query, params)`. Same fd-1+2 silencer as the extension adapter.

3. **`tests/tck/backends/rust_binding.py`** — spawns `tck_runner` once, talks JSON-lines on stdin/stdout, sends `{"cmd": "shutdown"}` on close (then `wait(timeout=2)` → `kill` fallback). Routes the subprocess's stderr to `build/tck-debug.log`.

`__main__.py` now:
- Runs every scenario across every selected backend.
- Writes `build/tck-results.json` (records tagged with `backend`).
- When ≥ 2 backends selected, also writes `build/tck-parity.json` with `{feature_file, scenario_name, statuses: {<backend>: <status>}, divergence: bool}`. Divergence rule: any status differs, or (if all pass) any `actual` differs from the first.
- Prints per-backend summaries + a parity tally.

Smoke run (`--backend all --filter Match1 --limit 3`):
```
TCK [extension]: 3 scenarios — pass=0 fail=3 error=0 skipped=0
TCK [python]:    3 scenarios — pass=1 fail=2 error=0 skipped=0
TCK [rust]:      3 scenarios — pass=0 fail=0 error=3 skipped=0
Parity: 3/3 scenarios diverge across backends.
  extension: fail=3
  python: fail=2, pass=1
  rust: error=3
```
Output already exposes real divergence — Python decodes empty results correctly where the extension returns the raw `"[]"` payload; Rust errors out on the `cypher()` call entirely. This is the triage input for [[GQLITE-T-0212]].

All acceptance criteria met.