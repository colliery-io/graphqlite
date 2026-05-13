---
id: tck-11-error-taxonomy-audit-vs
level: task
title: "TCK-11: Error taxonomy audit vs openCypher spec"
short_code: "GQLITE-T-0216"
created_at: 2026-05-13T12:51:10.866771+00:00
updated_at: 2026-05-13T12:51:10.866771+00:00
parent: GQLITE-I-0037
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0037
---

# TCK-11: Error taxonomy audit vs openCypher spec

## Parent Initiative

[[GQLITE-I-0037]]

## Objective

Audit the error classes GraphQLite raises against the openCypher error taxonomy. The spec defines categories like `SyntaxError`, `TypeError`, `EntityNotFound`, `ConstraintViolation`, `ArgumentError`; user-facing reliability depends on these being correct and stable.

## Acceptance Criteria

- [ ] Inventory: list every error path the extension can produce (grep the source), and map each to an openCypher category (or 'GraphQLite-specific' where no spec category applies).
- [ ] Probes: trigger each spec error category with a minimal query; record what GraphQLite emits (category name, message, sqlite error code).
- [ ] Audit report at `docs/tck/audit-errors-<date>.md` documents expected category, actual, and a pass/fail.
- [ ] Bugs filed for: errors that crash instead of returning a clean error; errors mis-categorized; errors with unhelpful messages on common user mistakes.

## Implementation Notes

### Technical Approach
This is partly a discovery task — we may not have an error taxonomy at all today. If so, the audit's primary output is a *proposal* for one, filed as a separate spec document under this initiative, plus bugs for the worst offenders.

### Dependencies
Independent of TCK-06/07; can run in parallel after [[GQLITE-T-0210]].

## Status Updates

*To be added during implementation*
