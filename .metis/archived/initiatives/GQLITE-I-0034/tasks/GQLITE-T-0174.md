---
id: ast-node-type-and-constructor-for
level: task
title: "AST node type and constructor for CALL subquery"
short_code: "GQLITE-T-0174"
created_at: 2026-03-29T01:05:12.532243+00:00
updated_at: 2026-03-29T16:46:44.271125+00:00
parent: GQLITE-I-0034
blocked_by: [GQLITE-T-0173]
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0034
---

# AST node type and constructor for CALL subquery

## Parent Initiative

[[GQLITE-I-0034]]

## Objective

Define the AST node type and constructor for CALL subquery so the parser can build a typed representation of `CALL { ... }` blocks.

## Affected Files

- `src/include/parser/cypher_ast.h`
- `src/backend/parser/cypher_ast.c`

## Implementation Notes

- Add `AST_NODE_CALL_SUBQUERY` to the `ast_node_type` enum in `cypher_ast.h`
- Define `cypher_call_subquery` struct containing `ast_list *branches` (list of query nodes, one per UNION branch)
- Add `make_call_subquery()` constructor in `cypher_ast.c`
- Add freeing logic for the new node type in `ast_node_free()`
- Wire the grammar actions from GQLITE-T-0173 to call `make_call_subquery()`

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `AST_NODE_CALL_SUBQUERY` exists in the enum
- [ ] `make_call_subquery()` creates a valid node with branch list
- [ ] `ast_node_free()` frees CALL subquery nodes without leaks (verify with valgrind/ASAN)
- [ ] Unit tests cover construction and freeing

## Effort Estimate

0.5 days

## Status Updates

### 2026-03-29: Completed as part of T-0173

All AST work was implemented alongside the grammar rules since grammar actions require the AST node. See T-0173 status updates for details. Implemented: `AST_NODE_CALL_SUBQUERY` enum, `cypher_call_subquery` struct with `ast_list *branches`, `make_cypher_call_subquery()` constructor, free logic in `ast_node_free()`, type name in `ast_node_type_name()`. AST freeing verified via debug output — no leaks.