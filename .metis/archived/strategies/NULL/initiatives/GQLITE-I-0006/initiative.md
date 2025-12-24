---
id: implement-exists-pattern-syntax
level: initiative
title: "Implement EXISTS Pattern Syntax"
short_code: "GQLITE-I-0006"
created_at: 2025-12-20T02:00:48.235117+00:00
updated_at: 2025-12-22T02:04:24.460731+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: true

tags:
  - "#initiative"
  - "#phase/completed"


exit_criteria_met: false
estimated_complexity: M
strategy_id: NULL
initiative_id: implement-exists-pattern-syntax
---

# Implement EXISTS Pattern Syntax Initiative

## Context

The grammar currently only supports `EXISTS(expr)` for property existence checking but lacks support for `EXISTS((pattern))` to check relationship existence. This is a core OpenCypher v9 feature that violates specification compliance. Users cannot check if relationships exist without creating complex workarounds.

## Goals & Non-Goals

**Goals:**
- Implement `EXISTS((pattern))` syntax for relationship existence checking
- Support pattern existence queries like `WHERE EXISTS((n)-[:KNOWS]->())`
- Extend existing `EXISTS(expr)` functionality without breaking changes
- Generate efficient SQL for pattern existence subqueries
- Achieve OpenCypher v9 compliance for EXISTS functionality

**Non-Goals:**
- Optimizing complex nested EXISTS patterns (separate performance initiative)
- Supporting non-standard EXISTS extensions
- Implementing EXISTS for list expressions (different feature)

## Detailed Design

1. **Grammar Changes** (cypher_gram.y):
   - Modify function_call rule to accept `EXISTS '(' '(' pattern ')' ')'`
   - Add pattern parsing within EXISTS context
   - Create AST node type for EXISTS patterns

2. **AST Structure**:
   ```c
   typedef struct {
       cypher_astnode_type_t type;
       cypher_astnode_t *pattern;  // The pattern to check
       bool is_pattern_exists;     // true for pattern, false for expr
   } cypher_ast_exists_t;
   ```

3. **SQL Generation**:
   - `EXISTS((n)-[:KNOWS]->())` becomes `EXISTS(SELECT 1 FROM relationships WHERE ...)`
   - Maintain proper join conditions with outer query variables

## Alternatives Considered

1. **Separate EXISTS_PATTERN function**: Create new function instead of extending EXISTS - rejected as it deviates from OpenCypher standard
2. **Macro expansion**: Transform EXISTS patterns to equivalent MATCH queries - rejected as it complicates query planning

## Implementation Plan

1. **Grammar Extension**: Modify function_call rule in cypher_gram.y
2. **AST Integration**: Define cypher_ast_exists_t structure
3. **Transform Layer Implementation**: Extend transform_return.c to handle EXISTS patterns
4. **Testing & Validation**: Test basic and complex pattern existence queries

## Testing Strategy

1. Basic pattern existence: `WHERE EXISTS((n)-[:KNOWS]->())`
2. Property-filtered patterns: `WHERE EXISTS((n)-[:KNOWS {since: 2020}]->())`
3. Complex patterns: `WHERE EXISTS((n)-[:KNOWS]->()-[:WORKS_AT]->())`
4. Negation: `WHERE NOT EXISTS((n)-[:KNOWS]->())`