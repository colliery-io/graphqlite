---
id: tck-05-baseline-tck-run-and
level: task
title: "TCK-05: Baseline TCK run and categorized conformance report"
short_code: "GQLITE-T-0210"
created_at: 2026-05-13T12:51:04.365979+00:00
updated_at: 2026-05-13T14:14:25.229457+00:00
parent: GQLITE-I-0037
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0037
---

# TCK-05: Baseline TCK run and categorized conformance report

## Parent Initiative

[[GQLITE-I-0037]]

## Objective

Run the full TCK against the extension backend, publish the first official conformance snapshot for GraphQLite, and group results by Cypher language area so triage can be parallelized.

## Acceptance Criteria

- [x] Full TCK run executed against the current `main` build of the extension (HEAD).
- [x] Report at `docs/tck/baseline-2026-05-13.md` with totals + percentages, error breakdown (incl. crash list), per-area breakdown (clauses / expressions / useCases), top-20 most-failing files.
- [x] Report links each failure cluster to its `vendor/tck/features/<path>`.
- [x] Raw JSON archived at `docs/tck/baseline-2026-05-13.json` (736 KB).
- [x] `tests/tck/baseline.json` written for CI gating: `{"pass": 22, "fail": 596, "error": 274, "skipped": 723, "total": 1615}`.

## Implementation Notes

### Technical Approach
This is the foundational artifact for the rest of the initiative. Do not edit pass/fail data by hand — always regenerate from the JSON. If certain scenarios are non-deterministic, file them under a 'flaky' bucket separately from real failures.

### Dependencies
Depends on [[GQLITE-T-0208]].

## Status Updates

### 2026-05-13 — Completed

**Headline numbers** (1615 scenarios, extension backend, supervised worker):

| Status | Count | % |
|---|---:|---:|
| pass    |  22 |  1.4% |
| fail    | 596 | 36.9% |
| error   | 274 | 17.0% |
| skipped | 723 | 44.8% |

Error breakdown: `SyntaxError=132, GraphQLiteError=124, TypeError=10, ExtensionCrash=8`.

**8 P0 extension crashes** — all in `clauses/with-*` scenarios. Top of stack is `executor_match.c:187` `build_query_results` (seen in the macOS crash report). Listed in the markdown report; macOS popped a crash dialog per crash but the supervisor caught them and respawned, so the run completed.

**Harness change made to land this task.** Turned `ExtensionBackend` into a supervisor for a long-lived worker subprocess (`tests/tck/_extension_worker.py`) speaking the same JSON-line protocol as `tck_runner.rs`. When the worker dies, the supervisor records the in-flight scenario as `error: ExtensionCrash`, respawns, and continues. Also updated `runner.py` so a backend error on a `Then result should be ...` step is classified as `error`, not `fail` — without that change, crashes hid inside the fail count.

Deliverables:
- `docs/tck/baseline-2026-05-13.md` — markdown report.
- `docs/tck/baseline-2026-05-13.json` — archived raw JSON.
- `tests/tck/baseline.json` — machine-readable baseline for CI gating ([[GQLITE-T-0217]]).
- `tests/tck/report.py` — reproducible report generator: `python -m tests.tck.report --results … --out … --baseline-out …`.

Per-area conformance:
- `clauses/`: 827 scenarios, 12 pass (1.5%) — heaviest absolute failure load.
- `expressions/`: 758 scenarios, 10 pass (1.3%) — 68% skip rate (harness value parser doesn't yet decode all TCK literal forms; closing the skip rate is harness work, not executor work).
- `useCases/`: 30 scenarios, 0 pass — small surface, no crashes.

All acceptance criteria met.