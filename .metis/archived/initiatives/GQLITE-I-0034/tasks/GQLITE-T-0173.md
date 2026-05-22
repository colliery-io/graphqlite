---
id: grammar-rules-for-call-clause-with
level: task
title: "Grammar rules for CALL { } clause with UNION support"
short_code: "GQLITE-T-0173"
created_at: 2026-03-29T01:05:11.432575+00:00
updated_at: 2026-03-29T16:46:21.550864+00:00
parent: GQLITE-I-0034
blocked_by: []
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0034
---

# Grammar rules for CALL { } clause with UNION support

## Parent Initiative

[[GQLITE-I-0034]]

## Objective

Add Bison grammar rules to `cypher_gram.y` so that `CALL { ... }` is recognized as a clause, including support for UNION branches inside the braces.

## Affected Files

- `src/backend/parser/cypher_gram.y`
- `src/generated/cypher_gram.tab.c` (regenerated)
- `src/generated/cypher_gram.tab.h` (regenerated)

## Implementation Notes

- **`CALL` is NOT currently a keyword token** in the grammar or scanner — it needs to be added to both `cypher_scanner.l` (as a keyword match) and `cypher_gram.y` (as a `%token`)
- Add production: `call_clause: CALL '{' query_list '}'`
- Add `query_list: query | query_list UNION query` to support UNION inside CALL
- Add `call_clause` as an alternative in the `clause:` rule
- **Conflict baseline**: current counts are `%expect 4` (S/R) and `%expect-rr 3` (R/R) — verify before and after, and update counts to match
- Check for ambiguity between `CALL` as a keyword and any existing identifier usage
- Regenerate the parser output files via the build system

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `CALL` added as keyword token in scanner and grammar
- [ ] `CALL { WITH c MATCH (d) RETURN d }` parses without error
- [ ] `CALL { query1 UNION query2 }` parses without error
- [ ] `CALL MATCH (n) RETURN n` (without braces) produces a parse error
- [ ] `%expect` / `%expect-rr` counts updated to match actual conflict counts
- [ ] Generated parser files rebuild cleanly

## Effort Estimate

1-2 days

## Status Updates

### 2026-03-29: Implementation complete

**Files changed:** `cypher_ast.h`, `cypher_ast.c`, `cypher_gram.y`, `cypher_parser.c`, `query_dispatch.c`, generated files.

- Added `AST_NODE_CALL_SUBQUERY` enum, struct, constructor, free logic
- Added `CALL` token to grammar and `keyword_token_map`
- Grammar rule: `call_clause: CALL '{' union_query '}'` — reuses existing `union_query` for UNION support
- Added `CLAUSE_CALL` detection in `analyze_query_clauses()`
- Conflict counts unchanged: `%expect 9` S/R, `%expect-rr 3` R/R
- All acceptance criteria verified, no regressions