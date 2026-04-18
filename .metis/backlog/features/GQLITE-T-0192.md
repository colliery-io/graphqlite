---
id: better-parse-diagnostics-validate
level: task
title: "Better parse diagnostics + validate(query) API (issue #16)"
short_code: "GQLITE-T-0192"
created_at: 2026-04-18T16:54:28.358601+00:00
updated_at: 2026-04-18T16:54:28.358601+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/backlog"
  - "#feature"


exit_criteria_met: false
initiative_id: NULL
---

# Better parse diagnostics + validate(query) API (issue #16)

**GitHub Issue**: [#16](https://github.com/colliery-io/graphqlite/issues/16)

## Objective

Improve parser/transform diagnostics to be actionable (line/column, expected tokens, structured error type) and expose a `validate(query)` API so clients can pre-check Cypher without executing it.

## Backlog Item Details

### Type
- [x] Feature / Enhancement

### Priority
- [x] P2 - Medium

### Business Justification
- **User Value**: Current errors like "Failed to transform MATCH clause" give no location info and no hint about what syntax was unsupported. Developers resort to binary-search debugging.
- **Business Value**: Better errors reduce issue volume and make GraphQLite a more credible Neo4j alternative. Pairs naturally with capability metadata (GQLITE-T-0100) for client-side adaptation.
- **Effort Estimate**: M - requires threading structured error data from Bison/Flex and the transform layer through the C extension to bindings.

## Acceptance Criteria

- [ ] Parse errors include line, column, and (where possible) the offending token and an expected-tokens hint.
- [ ] Transform errors identify the Cypher clause/expression responsible and a reason string.
- [ ] `validate(query)` API returns structured diagnostics without executing the query.
- [ ] Structured error type exposed to the Rust API and surfaced through Python/SQLite bindings.
- [ ] `dialect_version` and supported-feature list reported alongside diagnostics (or via capabilities API — see GQLITE-T-0100).
- [ ] Regression tests covering error messages for common invalid syntax (labels after REL, unsupported operators, etc.).

## Implementation Notes

### Technical Approach
- Bison: switch from default `yyerror` to locations tracking (`%locations`), capture `YYLTYPE` ranges on error, and surface them through `cypher_parser_result`.
- Flex: enable `%option yylineno` and propagate column tracking.
- Transform: replace stringly-typed `"Failed to transform ..."` with a structured error enum (clause, reason, span).
- Extension surface: serialize structured errors as JSON (already the response format) with stable keys: `line`, `column`, `token`, `expected`, `stage` (parse|transform|exec).
- New SQL function `SELECT cypher_validate('query')` returning the diagnostic JSON without side effects. Python: `db.validate(query) -> Diagnostic`.

### Dependencies
- Synergistic with GQLITE-T-0100 (capabilities) — diagnostics should include dialect version.

### Risk Considerations
- Bison error recovery is fragile; changing locations/error handling may require re-tuning `%expect` counts.
- Must preserve current error JSON as a stable subset so existing consumers don't break.

## Status Updates

- 2026-04-18: Ticket created from issue #16.
