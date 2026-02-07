---
id: better-diagnostics-query-validation
level: task
title: "Better diagnostics + query validation"
short_code: "GQLITE-T-0099"
created_at: 2026-02-07T02:09:58.717879+00:00
updated_at: 2026-02-07T02:09:58.717879+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/backlog"
  - "#feature"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Better diagnostics + query validation

**GitHub Issue**: [#16](https://github.com/colliery-io/graphqlite/issues/16)

## Objective

Make GraphQLite errors actionable with precise parse errors (line/column, expected tokens) and provide a `validate(query)` API that returns structured diagnostics without executing the query.

## Backlog Item Details

### Type
- [x] Feature - New functionality or enhancement

### Priority
- [ ] P2 - Medium (nice to have)

### Business Justification
- **User Value**: Developers can pinpoint exactly where and why a query fails instead of guessing from generic messages
- **Business Value**: Reduces debugging time; enables tooling (IDE integration, linters) built on the validate API
- **Effort Estimate**: M - Parser error plumbing + new validation API surface

## Acceptance Criteria

- [ ] Parse errors include line number, column number, and expected tokens
- [ ] `validate(query)` returns structured diagnostics without running the query
- [ ] Errors include `dialect_version` and supported feature list for client adaptation
- [ ] Tests cover error messages for all categories of invalid syntax

## Implementation Notes

### Technical Approach
- Expose parser errors through the Rust API as a structured type (line, col, message, expected tokens)
- Add a `validate()` function that runs the parser/transformer pipeline without execution
- Include dialect version metadata in error responses

### Dependencies
- Benefits from GQLITE-T-0096 (dialect parity) being done first, so error messages cover new syntax

### Risk Considerations
- Bison error recovery can be tricky — may need custom error productions for good messages
- Line/column tracking requires careful handling through the lexer

## Status Updates

*To be added during implementation*