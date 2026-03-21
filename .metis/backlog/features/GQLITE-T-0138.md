---
id: pattern-predicates-in-where-clause
level: task
title: "Pattern predicates in WHERE clause (bare relationship patterns as boolean expressions)"
short_code: "GQLITE-T-0138"
created_at: 2026-03-20T15:40:21.527463+00:00
updated_at: 2026-03-20T23:17:50.143661+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#feature"
  - "#phase/active"


exit_criteria_met: false
initiative_id: NULL
---

# Pattern predicates in WHERE clause (bare relationship patterns as boolean expressions)

## Objective

Support bare relationship patterns as boolean expressions in WHERE clauses, per the openCypher 9 specification (`<PatternPredicate> ::= <RelationshipsPattern>`). Currently GraphQLite requires the explicit `EXISTS(pattern)` form; the spec also allows the shorthand where a relationship pattern in boolean context is implicitly coerced to an existence check.

## Background

**Reported query:**
```cypher
MATCH (n {entity_type: 'Note'})
WHERE NOT (n)-[:BELONGS_TO]->() AND NOT (n)-[:RELATES_TO]->()
RETURN n.id, n.title
```
Fails with: `syntax error, unexpected ':'` at col 48 (the `:` in `-[:BELONGS_TO]->`).

**Workaround:** Wrap in `EXISTS()`:
```cypher
WHERE NOT EXISTS((n)-[:BELONGS_TO]->()) AND NOT EXISTS((n)-[:RELATES_TO]->())
```

**Spec basis:** openCypher 9 defines `PatternPredicate` — a `RelationshipsPattern` in a boolean site is semantically equivalent to `EXISTS { MATCH pattern RETURN 1 }`. Neo4j has supported this since at least v2.x. The pattern must contain at least one relationship to be valid.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [x] `WHERE (n)-[:REL]->()` parses and evaluates as existence check
- [x] `WHERE NOT (n)-[:REL]->()` parses and evaluates as non-existence check
- [x] Works with all relationship directions: `->`, `<-`, `-`
- [x] Works with typed and untyped relationships
- [x] Works combined with AND/OR/XOR and other predicates
- [x] Bare node pattern `(n)` without a relationship is NOT accepted as a pattern predicate
- [x] Unit tests and functional SQL tests added
- [x] Existing `EXISTS(pattern)` behavior unaffected

## Implementation Notes

### Technical Approach

**Grammar (`cypher_gram.y`):** Add a `pattern_predicate` production to the `expr` rule that matches a relationship pattern (node-rel-node) in expression context. The parser currently only reaches relationship patterns via the `simple_path` rule inside `MATCH`. A new rule would recognize `node_pattern rel_pattern node_pattern` within `expr` and produce an AST node (e.g., `AST_NODE_PATTERN_PREDICATE`).

**Transform:** In `transform_expression()`, handle `AST_NODE_PATTERN_PREDICATE` by reusing the same SQL generation path as `EXISTS(pattern)` — emit a `EXISTS (SELECT 1 FROM ...)` subquery.

**Key concern:** GLR conflicts. Adding relationship patterns to `expr` will likely introduce S/R or R/R conflicts since `(expr)` parenthesized expressions overlap with `(node_pattern)`. Careful precedence/disambiguation will be needed. Consider restricting the rule to require at least one `-[...]->` component to disambiguate.

### Dependencies
- None — the EXISTS(pattern) transform already exists and can be reused.

### Risk Considerations
- Grammar conflicts are the main risk. The GLR parser can handle ambiguity but conflict counts (`%expect`) may need updating and careful testing.

## Status Updates

### 2026-03-20: Implementation complete

**Files modified:**
- `src/backend/parser/cypher_gram.y` — Added two `expr` productions for pattern predicates (3-element and 5-element paths). Updated `%expect` from 4 to 9 S/R conflicts (all GLR-safe ambiguities from `(IDENTIFIER)` being parseable as both `(expr)` and `node_pattern`). R/R conflicts unchanged at 3.
- `src/backend/transform/transform_expr_predicate.c` — Fixed pre-existing bug where `EXISTS(pattern)` always assumed outgoing direction. Now respects `left_arrow`/`right_arrow` flags for incoming (`<-`) and undirected (`-`) relationship patterns.
- `src/generated/cypher_gram.tab.{c,h}` — Regenerated.
- `tests/functional/14_pattern_predicates.sql` — 17 test cases covering all directions, typed/untyped, NOT/AND/OR/XOR combinations, and equivalence with EXISTS().

**Approach:** Pattern predicates reuse the existing `make_exists_pattern_expr()` AST constructor and `transform_exists_expression()` SQL generation. No new AST node types needed — a bare pattern predicate is desugared to an EXISTS expression at parse time.

**Test results:** 5300 unit assertions pass (0 failures), all functional tests pass.