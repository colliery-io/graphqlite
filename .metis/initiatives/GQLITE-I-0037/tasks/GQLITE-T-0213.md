---
id: tck-08-null-and-three-valued-logic
level: task
title: "TCK-08: Null and three-valued-logic correctness audit"
short_code: "GQLITE-T-0213"
created_at: 2026-05-13T12:51:07.785555+00:00
updated_at: 2026-05-13T12:51:07.785555+00:00
parent: GQLITE-I-0037
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0037
---

# TCK-08: Null and three-valued-logic correctness audit

## Parent Initiative

[[GQLITE-I-0037]]

## Objective

Audit GraphQLite's behavior in areas where openCypher's three-valued logic and null-propagation rules are subtle and the TCK provides incomplete coverage. File bugs for any deviation from spec.

## Acceptance Criteria

- [ ] Spec sections covered: null propagation through arithmetic, string, and list operators; comparison with null (returns null, not false); `IS NULL` / `IS NOT NULL`; null handling in `WHERE`; null handling inside aggregations (skipped for most aggregates, counted as 0 by `count(...)`, not skipped by `collect(...)`); short-circuit evaluation in `AND`/`OR`/`XOR` per Kleene logic.
- [ ] Each rule has a hand-written probe query in `tests/tck/audits/null_logic.sql` (or equivalent harness format) with expected result.
- [ ] Audit report at `docs/tck/audit-null-logic-<date>.md` lists each probe, expected, actual, pass/fail, and links to filed bugs.
- [ ] Bugs filed for all failures with concrete reproductions.

## Implementation Notes

### Technical Approach
Reference: openCypher 9 spec sections on Boolean operators, comparison, and aggregation. Cross-check against Neo4j's behavior only where openCypher spec is genuinely silent.

### Dependencies
Independent of TCK-06/07; can run in parallel after [[GQLITE-T-0210]].

## Status Updates

*To be added during implementation*
