---
id: tck-05-baseline-tck-run-and
level: task
title: "TCK-05: Baseline TCK run and categorized conformance report"
short_code: "GQLITE-T-0210"
created_at: 2026-05-13T12:51:04.365979+00:00
updated_at: 2026-05-13T12:51:04.365979+00:00
parent: GQLITE-I-0037
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0037
---

# TCK-05: Baseline TCK run and categorized conformance report

## Parent Initiative

[[GQLITE-I-0037]]

## Objective

Run the full TCK against the extension backend, publish the first official conformance snapshot for GraphQLite, and group results by Cypher language area so triage can be parallelized.

## Acceptance Criteria

- [ ] Full TCK run executed against the current `main` build of the extension.
- [ ] Report committed at `docs/tck/baseline-<date>.md` containing: overall pass/fail/error/skipped counts and percentages, breakdown by top-level feature directory (clauses, expressions, functions, types, ...), and the top-20 most-failing feature files.
- [ ] Report links each failure cluster to its TCK feature file path so reviewers can drill in.
- [ ] Raw `tck-results.json` archived alongside the report.
- [ ] Baseline pass count is recorded in a machine-readable file (e.g. `tests/tck/baseline.json`) for CI gating in TCK-12.

## Implementation Notes

### Technical Approach
This is the foundational artifact for the rest of the initiative. Do not edit pass/fail data by hand — always regenerate from the JSON. If certain scenarios are non-deterministic, file them under a 'flaky' bucket separately from real failures.

### Dependencies
Depends on [[GQLITE-T-0208]].

## Status Updates

*To be added during implementation*
