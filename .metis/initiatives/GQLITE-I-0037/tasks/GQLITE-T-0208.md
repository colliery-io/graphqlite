---
id: tck-03-angreal-test-tck-task-with
level: task
title: "TCK-03: angreal test tck task with structured JSON output"
short_code: "GQLITE-T-0208"
created_at: 2026-05-13T12:51:02.075434+00:00
updated_at: 2026-05-13T13:31:16.491260+00:00
parent: GQLITE-I-0037
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0037
---

# TCK-03: angreal test tck task with structured JSON output

## Parent Initiative

[[GQLITE-I-0037]]

## Objective

Wire the TCK runner into the project's task orchestrator so conformance runs are a first-class operation, and emit machine-readable results downstream tooling (triage, CI gate, coverage matrix) can consume.

## Acceptance Criteria

## Acceptance Criteria

- [x] `angreal test tck` runs the full TCK corpus against the default backend (extension) and writes `build/tck-results.json`.
- [x] JSON schema: one record per scenario with fields `{feature_file, scenario_name, status, backend, duration_ms, diagnostic, expected, actual}` — verified by sample output.
- [x] Task supports `--backend extension|python|rust|all`, `--filter <substring>`, `--limit <N>`, `--debug`, `--verbose`.
- [x] Task prints summary at the end: total / pass / fail / error / skipped + top-10 failing feature files.
- [x] Exit code is 0 if the harness completed (regardless of scenario failures); non-zero only on harness errors.
- [x] `ToolDescription` with `risk_level: safe` attached.

## Implementation Notes

### Technical Approach
JSON output is the contract for downstream tasks. Keep schema stable once published. Backend selection plumbing must already accept `python` / `rust` even before those adapters land (TCK-04) — they should fail with a clear `backend not implemented` error until then.

### Dependencies
Depends on [[GQLITE-T-0207]].

## Status Updates

### 2026-05-13 — Completed

Added `test_tck` to `.angreal/task_test.py` (between `test_functional` and `test_constraints` for locality). Discoverable via `angreal tree` as `test tck`.

Implementation:
- Ensures extension is built (`ensure_extension_built()`).
- Picks a python that has `sqlite3.enable_load_extension` (Apple's `/usr/bin/python3` does not). Lookup order: `$GQLITE_TCK_PYTHON`, then `python3.13` / `.12` / `.11` / `python3` on PATH. Probes each with a tiny snippet before using it.
- Shells out to `python -m tests.tck` with the flags threaded through, `PYTHONPATH` set to the repo root.
- Exit code = harness exit code; harness returns 0 on completion regardless of pass/fail (regression gate is TCK-12's job).

Smoke run via `angreal test tck --filter Match1 --limit 5`:
```
TCK: 5 scenarios — pass=0 fail=5 error=0 skipped=0
Top failing feature files:
     5  clauses/match/Match1.feature
```
JSON produced at `build/tck-results.json` with the documented schema.

All acceptance criteria met.