---
id: m13-boolean-result-json-rendering
level: task
title: "M13: Boolean-result JSON rendering refactor — type-tag UDF results (unblocks ~6 TCK; depends on M4/M7)"
short_code: "GQLITE-T-0298"
created_at: 2026-05-19T14:50:06.306916+00:00
updated_at: 2026-05-19T14:50:06.306916+00:00
parent: GQLITE-I-0040
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0040
---

# M13: Boolean-result JSON rendering refactor — type-tag UDF results (unblocks ~6 TCK; depends on M4/M7)

## Parent Initiative

[[GQLITE-I-0040]]

## Objective

Today the extension's JSON formatter heuristically treats any TEXT cell with value `true` / `false` as a JSON boolean (unquoted). That collides with legitimately string values (`toString(true)` previously returned the bare bool; we worked around it with a fragile `'"true"'` quoting trick). After M4/M7 the helper UDFs live in `runtime/`; this task adds a proper type tag to UDF results (or migrates affected UDFs to return SQLite's native bool storage), then removes the JSON-formatter heuristic. Unblocks ~6 TCK scenarios where booleans in nested lists or aggregates render as integers.

## Acceptance Criteria

- [ ] No `strcmp(val, "true") == 0` heuristic remains in extension.c JSON output.
- [ ] `toString(true)` returns the string "true" without the `'"…"'` trick.
- [ ] Precedence3 [4]/[5], TypeConversion4 [2]/[3]/[4]/[5] pass.
- [ ] No regressions in scenarios that produce booleans from comparisons / IN / NOT / AND / OR.

## Status Updates

*To be added during implementation*
