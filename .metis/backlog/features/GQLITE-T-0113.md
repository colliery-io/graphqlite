---
id: undirected-match-syntax-a-b-not
level: task
title: "Undirected match syntax (a)--(b) not supported"
short_code: "GQLITE-T-0113"
created_at: 2026-03-17T01:30:28.977154+00:00
updated_at: 2026-03-17T02:15:49.837795+00:00
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

# Undirected match syntax (a)--(b) not supported

## Objective

Add support for undirected match syntax `(a)--(b)` in the Cypher parser. This is standard Cypher syntax for matching any relationship in either direction. Currently fails with "syntax error, unexpected '(', expecting '['".

## Backlog Item Details

### Type
- [x] Feature - New functionality or enhancement  

### Priority
- [x] P2 - Medium (nice to have)

### Business Justification
- **User Value**: Every "get neighbors" query currently requires a UNION of both directions, doubling query complexity
- **Effort Estimate**: M — requires parser grammar changes in `cypher_gram.y`

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `MATCH (a {id: '...'})--(b) RETURN b` parses and executes correctly
- [ ] Undirected match returns neighbors from both directions
- [ ] Works with relationship type filters: `(a)-[:TYPE]-(b)`
- [ ] Works with variable binding: `(a)-[r]-(b)`
- [ ] Functional tests added

## Implementation Notes

### Technical Approach
Add grammar rule in `cypher_gram.y` for the `--` pattern (two hyphens with no arrow). Transform should generate SQL that queries both edge directions (similar to how `get_neighbors` works internally with `UNION`).

### Dependencies
Parser-level change — touches `cypher_gram.y` and potentially `transform_match.c`.

## Status Updates

### Implementation Complete
- **Scanner fix**: Removed `-` from `operator` regex in `cypher_scanner.l` so `--` tokenizes as two separate `-` chars instead of one operator token
- **Parser rules**: Added bare `--`, `-->`, `<--` to `rel_pattern` in `cypher_gram.y`
- **Transform fix**: Changed undirected match in `transform_match.c` to query BOTH directions with OR condition (was treating undirected as forward-only)
- **Grammar conflicts**: Unchanged at 4 S/R, 3 R/R
- **Tests**: All 849 unit tests pass, functional test added (`100_undirected_match_test.sql`), scanner test updated
- **Regression**: Subtraction expressions verified working