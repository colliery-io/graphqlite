---
id: support-bulk-property-set-with-map
level: task
title: "Support bulk property SET with map literals (SET n = {map}, SET n += {map})"
short_code: "GQLITE-T-0107"
created_at: 2026-02-07T19:45:19.044324+00:00
updated_at: 2026-02-07T20:30:41.381879+00:00
parent: 
blocked_by: []
archived: true

tags:
  - "#task"
  - "#feature"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# Support bulk property SET with map literals (SET n = {map}, SET n += {map})

## Objective

Implement the openCypher bulk property SET syntax: `SET n = {map}` (replace all properties) and `SET n += {map}` (merge properties). This allows setting multiple properties on a node or edge in a single operation using a map literal, rather than one-by-one with `SET n.foo = 1, n.bar = 2`.

**GitHub Issue**: [#19](https://github.com/colliery-io/graphqlite/issues/19) (item 3)

## Backlog Item Details

### Type
- [x] Feature - New functionality or enhancement  

### Priority
- [x] P2 - Medium (nice to have)

### Business Justification
- **User Value**: Standard Cypher syntax supported by Neo4j and Memgraph; reduces verbosity for multi-property updates
- **Effort Estimate**: M

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `SET n = {foo: 1, bar: 2}` replaces all properties on node `n` with the given map
- [ ] `SET n += {foo: 1, bar: 2}` merges the map into existing properties (adds/updates keys, preserves others)
- [ ] Works for both nodes and edges (`SET r = {map}`, `SET r += {map}`)
- [ ] Works with map literals containing mixed types (string, int, real, bool, nested JSON)
- [ ] Works with parameterized maps (`SET n = $props`)
- [ ] Grammar changes maintain existing conflict counts (`%expect 4` S/R, `%expect-rr 3` R/R)
- [ ] Unit tests pass (770+)
- [ ] Functional tests cover both `=` and `+=` forms

## Implementation Notes

### Files to Modify (7 files)

| File | Change |
|------|--------|
| `src/backend/parser/cypher_gram.y` | Add `PLUS_EQ` rules to `set_item` |
| `src/include/parser/cypher_ast.h` | Add `is_bulk_set` + `is_merge` flags to `cypher_set_item` |
| `src/backend/parser/cypher_ast.c` | Update constructor + debug print |
| `src/backend/transform/transform_set.c` | New `transform_bulk_set_item()` function |
| `src/backend/executor/executor_set.c` | Bulk SET execution loop |
| `src/backend/executor/cypher_schema.c` | New `cypher_schema_delete_all_node_properties()` / edge variant |
| `src/include/executor/cypher_schema.h` | Function declarations |

### Step 1: Grammar + AST (~30 min)

**Grammar** (`cypher_gram.y`):
- `PLUS_EQ` token already exists in scanner (`cypher_scanner.l:147`) — no scanner changes needed
- Add two new alternatives to the `set_item` rule:
  - `IDENTIFIER '=' expr` with `is_bulk_set=true, is_merge=false` (when expr is a map/param)
  - `IDENTIFIER PLUS_EQ expr` with `is_bulk_set=true, is_merge=true`
- Detection strategy: since `IDENTIFIER '=' expr` overlaps with `expr '=' expr` (an identifier is a valid expr), the cleanest approach is to keep the existing `expr '=' expr` rule and **detect bulk SET at transform time** by checking if the LHS is a bare identifier (not a property access). This avoids grammar conflicts entirely.
- Alternatively, could add `PLUS_EQ` as a new rule: `expr PLUS_EQ expr` — only the `PLUS_EQ` variant truly needs a grammar change since `+=` is new syntax.

**AST** (`cypher_ast.h`):
- Add to `cypher_set_item`:
  ```c
  bool is_merge;  /* true for +=, false for = */
  ```
- The `is_bulk_set` flag can be inferred at transform time (LHS is identifier, not property access), so only `is_merge` needs to be in the struct.

### Step 2: Transform layer (~2-3 hrs)

**`transform_set.c`** — new `transform_bulk_set_item()`:
- Detect bulk SET: `item->property->type == AST_NODE_IDENTIFIER` (bare var, not `AST_NODE_PROPERTY`)
- Get variable alias + node_id binding from `ctx->var_ctx`

For `SET n = {map}` (replace mode):
1. Generate DELETE from all 5 property tables:
   ```sql
   DELETE FROM node_props_text WHERE node_id = <id>;
   DELETE FROM node_props_int WHERE node_id = <id>;
   DELETE FROM node_props_real WHERE node_id = <id>;
   DELETE FROM node_props_bool WHERE node_id = <id>;
   DELETE FROM node_props_json WHERE node_id = <id>;
   ```
2. For each key-value pair in the map literal, determine value type and generate:
   ```sql
   INSERT OR REPLACE INTO node_props_<type> (node_id, key_id, value)
   SELECT <id>, (SELECT id FROM property_keys WHERE key = '<key>'), <value>
   ```

For `SET n += {map}` (merge mode):
- Skip deletion step, only do the INSERT OR REPLACE for each pair

Edge support: same logic but `edge_props_*` tables and `edge_id`.

### Step 3: Executor layer (~1.5-2 hrs)

**`executor_set.c`**:
- Detect bulk SET: `item->property->type == AST_NODE_IDENTIFIER`
- For replace mode: call new `cypher_schema_delete_all_node_properties(mgr, node_id)`
- Iterate `cypher_map->pairs`, infer type per value, call `cypher_schema_set_node_property()` for each
- Handle edge variant similarly

**`cypher_schema.c`** — new functions:
```c
int cypher_schema_delete_all_node_properties(cypher_schema_manager *mgr, int node_id);
int cypher_schema_delete_all_edge_properties(cypher_schema_manager *mgr, int edge_id);
```
Each runs DELETE on all 5 typed property tables for the given entity ID.

### Dependencies
- JSON property support (GQLITE-I-0030) — completed
- `PLUS_EQ` token — already in scanner

### Risk Considerations
- **Grammar conflicts**: Low risk if we reuse `expr '=' expr` and detect bulk SET at transform time. Only `PLUS_EQ` needs a new grammar rule.
- **Type inference**: Each map value must be routed to the correct property table. Reuse existing logic from `generate_property_update()` in `transform_set.c`.
- **Atomicity**: The replace mode (DELETE all + INSERT each) is multi-statement. If one INSERT fails, prior deletes are not rolled back within a single `cypher()` call. Acceptable for now — matches existing multi-statement SET behavior.

## Test Plan

### Functional Tests (`tests/functional/34_bulk_set.sql`)

**Section 1: Basic SET n = {map}**
- Create node with properties, then `SET n = {new: "props"}` — verify old properties removed, new ones set
- `SET n = {a: 1, b: "two", c: 3.14, d: true}` — verify mixed types stored correctly
- `SET n = {}` — verify all properties cleared (empty map = delete all)

**Section 2: Basic SET n += {map}**
- Create node with `{a: 1, b: 2}`, then `SET n += {c: 3}` — verify `a`, `b` preserved, `c` added
- `SET n += {a: 10}` — verify `a` updated to 10, `b` unchanged
- `SET n += {a: 10, d: 4}` — verify update + add in same operation

**Section 3: Replace vs Merge semantics**
- Create node with `{x: 1, y: 2, z: 3}`
- `SET n = {x: 10}` — verify only `x` remains (y, z deleted)
- Re-create, `SET n += {x: 10}` — verify x=10, y=2, z=3 all present

**Section 4: Edge bulk SET**
- `MATCH ()-[r]->() SET r = {weight: 5, label: "new"}` — verify edge properties replaced
- `MATCH ()-[r]->() SET r += {extra: true}` — verify merge on edge

**Section 5: JSON/nested values in bulk SET**
- `SET n = {name: "Alice", meta: {role: "admin"}, tags: [1,2,3]}` — verify scalar + JSON coexistence
- `SET n += {meta: {role: "user"}}` — verify JSON property updated

**Section 6: Parameterized bulk SET**
- `SET n = $props` where `$props` is a map parameter — verify properties set
- `SET n += $props` — verify merge with parameter

**Section 7: WHERE + RETURN integration**
- `MATCH (n:Foo) WHERE n.name = "bar" SET n = {name: "bar", updated: true} RETURN n` — full pipeline
- `MATCH (n) SET n += {visited: true} RETURN properties(n)` — verify properties() reflects merge

**Section 8: Edge cases**
- SET on non-existent variable — verify error
- `SET n = {map}` where n is unbound — verify error
- Multiple bulk SETs in one query: `SET n = {a: 1}, m += {b: 2}`

### Unit Tests
- AST: verify `make_cypher_set_item()` creates correct node with `is_merge` flag
- Transform: verify SQL output for both `=` and `+=` modes
- Existing SET tests: verify no regressions (property-by-property SET still works)

## Status Updates

### Implementation Complete
All changes implemented and tested:

**Files modified (7):**
1. `src/include/parser/cypher_ast.h` — Added `is_merge` field to `cypher_set_item`
2. `src/backend/parser/cypher_ast.c` — Updated `make_cypher_set_item()` to accept `is_merge`
3. `src/backend/parser/cypher_gram.y` — Added `expr PLUS_EQ expr` rule to `set_item`; updated all `make_cypher_set_item` calls
4. `src/backend/transform/transform_set.c` — Added `generate_bulk_property_update()` for SQL generation; detects bulk SET when LHS is bare identifier
5. `src/backend/executor/executor_set.c` — Added bulk SET handling in `execute_set_operations()` with map iteration and type routing
6. `src/backend/executor/cypher_schema.c` — Added `cypher_schema_delete_all_node_properties()` and `cypher_schema_delete_all_edge_properties()`
7. `src/include/executor/cypher_schema.h` — Declarations for new functions

**New test file:**
- `tests/functional/34_bulk_set.sql` — 8 sections, all passing

**Test results:**
- Grammar conflicts: 4 S/R + 3 R/R (unchanged, matches %expect)
- Unit tests: 770/770 pass
- Functional tests: all pass (including new 34_bulk_set.sql)

**Acceptance criteria status:**
- [x] `SET n = {foo: 1, bar: 2}` replaces all properties
- [x] `SET n += {foo: 1, bar: 2}` merges properties
- [x] Works for edges (`SET r = {map}`, `SET r += {map}`)
- [x] Mixed types (string, int, real, bool, nested JSON)
- [ ] Parameterized maps — deferred (returns clear error message)
- [x] Grammar conflicts unchanged
- [x] 770+ unit tests pass
- [x] Functional tests cover both forms