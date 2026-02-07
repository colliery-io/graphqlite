---
id: native-json-map-list-properties
level: task
title: "Native JSON/map/list properties + nested access"
short_code: "GQLITE-T-0097"
created_at: 2026-02-07T02:09:57.039169+00:00
updated_at: 2026-02-07T02:09:57.039169+00:00
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

# Native JSON/map/list properties + nested access

**GitHub Issue**: [#14](https://github.com/colliery-io/graphqlite/issues/14)

## Objective

Support map and list literals as property values in SET and CREATE, store them as JSON internally, and enable nested access against JSON values in queries.

## Backlog Item Details

### Type
- [x] Feature - New functionality or enhancement

### Priority
- [ ] P1 - High (important for user experience)

### Business Justification
- **User Value**: Users can store structured data (maps, lists) as properties without manual JSON serialization/flattening
- **Business Value**: Closes a major gap vs Memgraph; enables richer data modeling use cases
- **Effort Estimate**: XL - Touches parser, storage layer, query execution, and JSON1 integration

## Acceptance Criteria

- [ ] `SET n.metadata = {name:'x', labels:{a:'b'}}` stores the value as JSON
- [ ] `RETURN n.metadata.name` returns `x` via nested access
- [ ] `IN` and comparison operators work on nested JSON scalar values
- [ ] Map and list literals accepted in both SET and CREATE
- [ ] JSON helper functions available as fallback: `json_get(n.metadata, 'name')`
- [ ] Existing scalar property behavior is unchanged

## Implementation Notes

### Technical Approach

#### Architecture Overview

The current property storage uses an EAV (Entity-Attribute-Value) model with four typed tables per entity kind: `node_props_int`, `node_props_text`, `node_props_real`, `node_props_bool` (and matching `edge_props_*` tables). JSON/map/list values will be stored in a **new fifth property table** (`node_props_json` / `edge_props_json`) that stores the value as a JSON TEXT column, leveraging SQLite JSON1 functions for extraction and manipulation. SQLite 3.47.2 (vendored) has JSON1 built-in.

**Key architectural decision**: Add dedicated JSON property tables rather than overloading `_text` tables. This preserves existing behavior, avoids ambiguity in `COALESCE` chains (a JSON string `'{"a":1}'` would collide with text), and allows future JSON-specific indexing.

#### Files to Modify (in implementation order)

**Phase 1: Schema Layer - JSON Property Tables**

1. **`src/include/executor/cypher_schema.h`**
   - Add `PROP_TYPE_JSON` to the `property_type` enum (value 4)
   - Add DDL constants: `CYPHER_SCHEMA_DDL_NODE_PROPS_JSON`, `CYPHER_SCHEMA_DDL_EDGE_PROPS_JSON`
   - Add index constants: `CYPHER_SCHEMA_INDEX_NODE_PROPS_JSON`, `CYPHER_SCHEMA_INDEX_EDGE_PROPS_JSON`

2. **`src/backend/executor/cypher_schema.c`**
   - Add DDL for `node_props_json` and `edge_props_json` tables:
     ```sql
     CREATE TABLE IF NOT EXISTS node_props_json (
       node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
       key_id INTEGER NOT NULL REFERENCES property_keys(id),
       value TEXT NOT NULL CHECK (json_valid(value)),
       PRIMARY KEY (node_id, key_id)
     );
     ```
   - Add table creation and index creation calls in `cypher_schema_create_tables()` and `cypher_schema_create_indexes()`
   - Add `PROP_TYPE_JSON` case to `cypher_schema_set_node_property()` and `cypher_schema_set_edge_property()` - binds value as TEXT
   - Add `node_props_json` and `edge_props_json` to cleanup loops in property set/delete functions
   - Update `cypher_schema_property_type_name()` to return `"JSON"` for `PROP_TYPE_JSON`

**Phase 2: Parser - Nested Property Access**

3. **`src/backend/parser/cypher_gram.y`**
   - Add grammar rule for chained dot access. Currently only `IDENTIFIER '.' IDENTIFIER` is supported. Add a recursive rule:
     ```yacc
     | expr '.' IDENTIFIER
         {
             $$ = (ast_node*)make_property($1, $3, @3.first_line);
             free($3);
         }
     ```
     This allows `n.metadata.name` to parse as `make_property(make_property(n, "metadata"), "name")` - a nested `AST_NODE_PROPERTY` with another `AST_NODE_PROPERTY` as its `expr`.
   - IMPORTANT: This rule must be added carefully to avoid ambiguity with the existing `IDENTIFIER '.' IDENTIFIER` rule. The existing rule should remain for backward compatibility with simpler parsing paths; the new rule handles the chained case via `expr '.' IDENTIFIER`.
   - Expect count (`%expect`) may need adjustment if new shift/reduce conflicts arise from the grammar extension.

**Phase 3: Transform Layer - Nested Property Access & JSON Storage**

4. **`src/backend/transform/transform_expr_ops.c`** (core change)
   - Modify `transform_property_access()` to detect nested property access (when `prop->expr->type == AST_NODE_PROPERTY` instead of `AST_NODE_IDENTIFIER`).
   - For nested access like `n.metadata.name`:
     - Walk the chain to find the root identifier (the variable name) and collect the property path segments
     - For the root property (e.g., `metadata`), generate the existing EAV COALESCE lookup but **also include** the JSON table: add `node_props_json` to the COALESCE chain
     - For nested access beyond the first level, wrap with `json_extract()`:
       ```sql
       json_extract(
         (SELECT npj.value FROM node_props_json npj
          JOIN property_keys pk ON npj.key_id = pk.id
          WHERE npj.node_id = n0.id AND pk.key = 'metadata'),
         '$.name'
       )
       ```
     - Support arbitrary depth: `n.a.b.c` becomes `json_extract(<lookup for a>, '$.b.c')`
   - For single-level property access (`n.prop`), add the JSON table as a new arm in the existing COALESCE chain so JSON-valued properties are returned correctly:
     ```sql
     COALESCE(
       (SELECT npt.value FROM node_props_text npt ...),
       (SELECT npi.value FROM node_props_int npi ...),
       (SELECT npr.value FROM node_props_real npr ...),
       (SELECT npb.value FROM node_props_bool npb ...),
       (SELECT npj.value FROM node_props_json npj ...)  -- NEW
     )
     ```

5. **`src/backend/transform/transform_set.c`**
   - Modify `generate_property_update()` to detect when `value_expr` is `AST_NODE_MAP` or `AST_NODE_LIST`
   - When the value is a map/list, target `node_props_json` (or `edge_props_json`) instead of the scalar tables
   - The value expression is already transformed to `json_object(...)` or `json_array(...)` by `transform_expression()` in `transform_return.c`, so the INSERT value will be valid JSON
   - Also handle edge properties (currently `generate_property_update` only references `node_props_*` - needs to check if variable is an edge and use `edge_props_*` accordingly)

**Phase 4: Executor Layer - JSON Property Handling**

6. **`src/backend/executor/executor_set.c`**
   - In `execute_set_operations()`, add handling for `AST_NODE_MAP` and `AST_NODE_LIST` value expressions
   - When the value expr is a map: serialize the AST map to a JSON string, then call `cypher_schema_set_node_property()` with `PROP_TYPE_JSON`
   - Implement a helper function `ast_map_to_json_string()` that recursively walks an `AST_NODE_MAP` / `AST_NODE_LIST` tree and produces a JSON string using the existing `json_builder` (`src/backend/executor/json_builder.c`)
   - Handle nested maps: `{name:'x', labels:{a:'b'}}` should produce `{"name":"x","labels":{"a":"b"}}`

7. **`src/backend/executor/executor_create.c`**
   - In `execute_path_pattern_with_variables()`, when processing node/edge properties, detect map/list values and store as `PROP_TYPE_JSON`
   - This is the same pattern as executor_set.c: when `pair->value->type == AST_NODE_MAP || pair->value->type == AST_NODE_LIST`, serialize to JSON and use `PROP_TYPE_JSON`

**Phase 5: JSON Helper Functions**

8. **`src/backend/transform/transform_func_dispatch.c`**
   - Register new function handlers in the dispatch table:
     - `{"json_get", transform_json_get_function}` - explicit `json_get(expr, path)` fallback
     - `{"json_set", transform_json_set_function}` - optional, for `json_set(expr, path, value)`
     - `{"json_keys", transform_json_keys_function}` - return keys of a JSON object
     - `{"json_type", transform_json_type_function}` - return JSON type of a value

9. **New file: `src/backend/transform/transform_func_json.c`**
   - Implement `transform_json_get_function()`: translates `json_get(n.metadata, 'name')` to `json_extract(<property-lookup>, '$.name')`
   - Implement other JSON helper functions as thin wrappers around SQLite JSON1 functions
   - `json_get(expr, path)` -> `json_extract(expr, '$.' || path)` or `json_extract(expr, path)` if path starts with `$`

10. **`src/include/transform/transform_functions.h`**
    - Add declarations for the new JSON function handlers

**Phase 6: Comparison Operators on JSON Values**

11. **`src/backend/transform/transform_expr_ops.c`** (additional change)
    - For `IN` operator with a JSON-valued right side, use `json_each()` (already implemented for non-literal lists at line ~140)
    - For comparison operators (`=`, `<>`, `<`, `>`, etc.) where one side is a nested JSON access, the `json_extract()` output already returns the scalar value with the correct SQLite affinity, so no additional work is needed

#### JSON Storage Design

- **Storage**: Map/list values stored as JSON TEXT in `node_props_json.value` / `edge_props_json.value` with `json_valid()` CHECK constraint
- **Retrieval (single-level)**: `n.metadata` returns the raw JSON string from `node_props_json`
- **Retrieval (nested)**: `n.metadata.name` uses `json_extract(value, '$.name')` which returns the scalar value with correct affinity
- **Deep nesting**: `n.a.b.c[0].d` combines dot-chaining and subscript: `json_extract(value, '$.b.c[0].d')`
- **Subscript on JSON**: Already handled by existing `AST_NODE_SUBSCRIPT` -> `json_extract()` code in transform_return.c (line 661-691). This naturally composes: `n.metadata.items[0]` becomes `json_extract(json_extract(metadata_lookup, '$.items'), '$[0]')` or ideally optimized to `json_extract(metadata_lookup, '$.items[0]')`

#### Order of Changes

1. Schema layer (tables, DDL, property_type enum) - no existing behavior changes
2. Parser (nested dot access grammar) - backward compatible extension
3. Transform property access (add JSON to COALESCE, nested access via json_extract) - extends existing behavior
4. Transform SET (detect map/list values, route to JSON table) - extends existing behavior  
5. Executor SET/CREATE (serialize AST maps to JSON, store with PROP_TYPE_JSON) - extends existing behavior
6. JSON helper functions (json_get, etc.) - purely additive
7. Tests

#### Testing Approach

- **New test file: `tests/test_executor_json_properties.c`**
  - Test `CREATE (n:Node {metadata: {name:'x', labels:{a:'b'}}})` stores JSON
  - Test `MATCH (n) RETURN n.metadata` returns JSON string
  - Test `MATCH (n) RETURN n.metadata.name` returns `'x'` via nested access
  - Test `MATCH (n) WHERE n.metadata.name = 'x' RETURN n` works in WHERE
  - Test `SET n.metadata = {key: 'val'}` stores JSON via SET
  - Test `SET n.tags = [1, 2, 3]` stores JSON array
  - Test `RETURN n.tags[0]` returns first element via subscript
  - Test `json_get(n.metadata, 'name')` function fallback
  - Test `n.metadata.labels.a` deep nesting returns `'b'`
  - Test comparison operators: `WHERE n.metadata.count > 5`
  - Test IN operator: `WHERE 'x' IN n.tags`
  - Test mixed: scalar properties continue to work unchanged
  - Test edge JSON properties: `SET r.config = {timeout: 30}`

- **Extend existing tests**:
  - `tests/test_executor_set.c` - add map/list value cases
  - `tests/test_executor_expressions.c` - add nested property access cases
  - `tests/test_parser.c` - add nested dot access parsing tests
  - `tests/test_transform_set.c` - add JSON property SQL generation tests

### Dependencies
- The parser grammar change (nested `expr '.' IDENTIFIER`) must be carefully integrated to avoid ambiguity with existing grammar rules. This is the riskiest change.
- No dependency on GQLITE-T-0096 (bracket access) for the core feature. The existing `AST_NODE_SUBSCRIPT` already handles `expr[index]` via `json_extract`, so `n.metadata.items[0]` will work once nested dot access is in place.

### Risk Considerations
- **Grammar conflicts**: Adding `expr '.' IDENTIFIER` to the grammar may introduce shift/reduce conflicts with existing rules. The GLR parser (`%glr-parser`) should handle this, but conflicts need to be analyzed and the `%expect` counts updated.
- **JSON1 availability**: SQLite 3.47.2 (vendored) has JSON1 built-in. The `json_valid()` CHECK constraint serves as a runtime guard. For external SQLite builds without JSON1, a graceful error should be emitted when JSON properties are used.
- **Performance**: `json_extract()` in WHERE clauses will be slower than scalar lookups because JSON parsing happens per-row. For the initial implementation this is acceptable; a future optimization could add generated columns or expression indexes.
- **Backward compatibility**: The new JSON property tables are additive. Existing scalar properties are unaffected. The COALESCE chain in property access simply gains one more arm.

## Status Updates

*To be added during implementation*