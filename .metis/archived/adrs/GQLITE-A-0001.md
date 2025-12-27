---
id: 001-keep-append-sql-for-expression
level: adr
title: "Keep append_sql for expression transformation"
number: 1
short_code: "GQLITE-A-0001"
created_at: 2025-12-27T17:12:02.977994+00:00
updated_at: 2025-12-27T17:12:02.977994+00:00
decision_date: 
decision_maker: 
parent: 
archived: true

tags:
  - "#adr"
  - "#phase/draft"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# ADR-1: Keep append_sql for expression transformation

## Context

During the Unified SQL Builder initiative (GQLITE-I-0025), we migrated READ query clauses (MATCH, WITH, UNWIND, RETURN) to use the structured `sql_builder` API. This raised the question of whether expression transformation should also be migrated.

Expression transformation currently uses `append_sql()` with 399 calls across:
- `transform_expr_ops.c` - 78 calls (binary ops, comparisons)
- `transform_return.c` - 74 calls (return expressions)
- `transform_expr_predicate.c` - 41 calls (IS NULL, LIKE, IN)
- `transform_func_*.c` - ~170 calls (function implementations)

## Decision

**Keep `append_sql()` for expression transformation.** Do not migrate expressions to the unified builder API.

## Rationale

Expression transformation is fundamentally different from clause assembly:

1. **Linear string building**: Expressions are built left-to-right as SQL fragments. There's no clause reordering needed (unlike SELECT/FROM/WHERE which must be assembled in specific order).

2. **No deferred assembly**: Expression output is immediate - `1 + 2` becomes `1 + 2`. There's no need to buffer parts for later assembly.

3. **Nested recursion**: Expressions recurse deeply (`transform_expression` → `transform_binary_op` → `transform_expression`). Each level appends to the same buffer, which `append_sql()` handles naturally.

4. **Different abstraction level**: The unified builder operates at clause level (SELECT, FROM, JOIN). Expressions are sub-clause components that get passed TO the builder via `sql_select(builder, expr_string, alias)`.

## Consequences

### Positive
- No unnecessary refactoring of working code
- Expression transformation remains simple and direct
- Clear separation: builder for clauses, append_sql for expressions

### Negative
- Two patterns coexist in codebase (acceptable given different purposes)

### Neutral
- 399 append_sql calls remain (appropriate usage)