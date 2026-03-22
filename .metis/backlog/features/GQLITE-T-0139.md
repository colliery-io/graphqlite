---
id: existential-subquery-syntax-exists
level: task
title: "Existential subquery syntax: EXISTS { MATCH ... WHERE ... }"
short_code: "GQLITE-T-0139"
created_at: 2026-03-22T00:45:57.656842+00:00
updated_at: 2026-03-22T00:45:57.656842+00:00
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

# Existential subquery syntax: EXISTS { MATCH ... WHERE ... }

## Objective

Implement full existential subquery syntax `EXISTS { MATCH ... WHERE ... }` per CIP2015-05-13-EXISTS. Currently only `EXISTS((pattern))` and `EXISTS(property)` are supported. The full subquery form allows filtering within the existence check.

## Background

**Reported from Clotho integration (v0.3.7).** Query:
```cypher
MATCH (t) WHERE t.entity_type = 'Transcript'
AND NOT EXISTS { MATCH (e)-[:EXTRACTED_FROM]->(t) }
RETURN t.title
```
Fails with: `syntax error, unexpected '{'`

**Note:** The reporter also used a double-WHERE variant (`WHERE ... WHERE NOT EXISTS ...`) which is invalid Cypher regardless. The single-WHERE + AND form is the correct syntax.

**Workarounds that already work:**
- `EXISTS((t)<-[:EXTRACTED_FROM]-())` — simple pattern form
- `NOT (t)<-[:EXTRACTED_FROM]-()` — pattern predicate (added in v0.3.10)

**Spec basis:** CIP2015-05-13-EXISTS defines three levels:
1. `EXISTS(property)` — property existence (supported)
2. `EXISTS((pattern))` — pattern existence (supported)
3. `EXISTS { MATCH pattern [WHERE filter] }` — full subquery (NOT supported)

## Acceptance Criteria

- [ ] `EXISTS { MATCH (a)-[:REL]->(b) }` parses and evaluates as existence check
- [ ] `NOT EXISTS { MATCH (a)-[:REL]->(b) }` works for non-existence
- [ ] `EXISTS { MATCH (a)-[:REL]->(b) WHERE b.prop = value }` supports filtering within the subquery
- [ ] Variables from outer scope are correctly correlated (e.g., `t` from outer MATCH)
- [ ] Unit tests and functional SQL tests added
- [ ] Existing `EXISTS((pattern))` and `EXISTS(property)` behavior unaffected

## Implementation Notes

### Technical Approach

**Grammar (`cypher_gram.y`):** Add production for `EXISTS '{' clause_list '}'` or a specialized `EXISTS '{' MATCH pattern_list where_opt '}'` rule. The braces `{` `}` already tokenize correctly (used for map literals). The main challenge is disambiguating `EXISTS { ... }` from `EXISTS(...)`.

**AST:** May need a new AST node type (`AST_NODE_EXISTS_SUBQUERY`) or extend `cypher_exists_expr` with a subquery variant that holds a full `MATCH` + optional `WHERE`.

**Transform:** The subquery needs its own variable scope that can reference outer variables. Generate a correlated `EXISTS (SELECT 1 FROM ... WHERE ...)` SQL subquery, similar to the pattern form but with additional WHERE conditions from the subquery body.

### Dependencies
- Pattern predicate support (GQLITE-T-0138) — completed
- CALL subquery (GQLITE-T-0133) may share scoping infrastructure

### Risk Considerations
- Variable scoping: inner variables must not leak to outer scope, but outer variables must be accessible in the subquery
- Grammar conflicts with `EXISTS(...)` form — need careful disambiguation of `{` vs `(`

## Status Updates

*To be added during implementation*