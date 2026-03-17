---
id: support-return-wildcard
level: task
title: "Support RETURN * wildcard"
short_code: "GQLITE-T-0123"
created_at: 2026-03-17T13:37:47.299840+00:00
updated_at: 2026-03-17T14:20:26.208943+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#feature"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# Support RETURN * wildcard

## Objective

Support `RETURN *` and `WITH *` to return all bound variables without listing them explicitly.

## Backlog Item Details

### Type
- [x] Feature - New functionality or enhancement

### Priority
- [x] P1 - High (important for user experience)

### Business Justification
- **User Value**: `RETURN *` is one of the most commonly used Cypher shortcuts. Every tutorial uses it. Its absence surprises users immediately.
- **Effort Estimate**: M — requires tracking bound variables through parse/transform

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `MATCH (n) RETURN *` returns all matched nodes
- [ ] `MATCH (n)-[r]->(m) RETURN *` returns n, r, m
- [ ] `WITH *` passes all variables to next clause
- [ ] Works with OPTIONAL MATCH
- [ ] Functional tests added

## Coverage Matrix Reference
- `docs/cypher-coverage-matrix.md` Section 3: `RETURN *`, `WITH *`

## Status Updates

### Implementation Complete
- **Grammar**: Added `RETURN *`, `RETURN DISTINCT *`, `WITH *` rules in `cypher_gram.y`
- **AST**: Added `return_all` flag to `cypher_return`, `pass_all` flag to `cypher_with`
- **Executor**: `RETURN *` expanded into explicit items after MATCH transform registers variables (in `executor_match.c`)
- **Transform**: `RETURN *` and `WITH *` expansion via `transform_var_count/at` iteration in `transform_return.c` and `transform_with.c`
- **Verified**: `MATCH (n) RETURN *`, `MATCH (a)-[r]->(b) RETURN *`, `RETURN * LIMIT N` all work
- **Tests**: 849 unit, 226 Python pass