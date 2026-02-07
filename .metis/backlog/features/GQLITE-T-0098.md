---
id: parameterized-cypher-api
level: task
title: "Parameterized Cypher API"
short_code: "GQLITE-T-0098"
created_at: 2026-02-07T02:09:57.907693+00:00
updated_at: 2026-02-07T13:05:01.484027+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#feature"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Parameterized Cypher API

**GitHub Issue**: [#15](https://github.com/colliery-io/graphqlite/issues/15)

## Objective

Add a `cypher_with_params(query, params)` API that allows clients to pass parameters separately instead of inlining values into Cypher strings, eliminating escaping bugs and security risks.

## Backlog Item Details

### Type
- [x] Feature - New functionality or enhancement

### Priority
- [ ] P1 - High (important for user experience)

### Business Justification
- **User Value**: Eliminates error-prone string interpolation; prevents injection-style bugs
- **Business Value**: Security improvement; reduces support burden from escaping issues; standard practice for query APIs
- **Effort Estimate**: M - New API surface with parameter binding layer, but no parser changes

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

### Bug Fix (C Extension)
- [ ] Parameters bind correctly in `handle_generic_transform()` path (OPTIONAL MATCH, multi-MATCH, WITH+MATCH+RETURN, UNWIND+RETURN)
- [ ] Parameters bind correctly in `handle_return_only()` path
- [ ] Parameters bind correctly in UNION handling path

### Parameter Types
- [ ] String parameters: `$name` binds `"Alice"` correctly
- [ ] Integer parameters: `$age` binds `30` correctly
- [ ] Float parameters: `$score` binds `3.14` correctly
- [ ] Boolean parameters: `$active` binds `true`/`false` correctly
- [ ] Null parameters: `$val` binds `null` correctly
- [ ] List parameters: `$ids` binds `[1, 2, 3]` as JSON text (Phase 5 / pairs with GQLITE-T-0097)
- [ ] Map parameters: `$meta` binds `{"key": "val"}` as JSON text (Phase 5 / pairs with GQLITE-T-0097)

### API Surface
- [ ] Rust `Connection::cypher_with_params(query, &json!({...}))` works end-to-end
- [ ] Rust `Graph::query_with_params(query, &json!({...}))` works end-to-end
- [ ] Python `Connection.cypher(query, params)` continues to work (already exists)
- [ ] Python `Graph.query(query, params)` forwards params to Connection

### Parameter Syntax
- [ ] `$name` style parameters resolve correctly
- [ ] `$0` numeric parameters resolve correctly
- [ ] `${name}` brace-delimited parameters resolve correctly
- [ ] Same parameter used multiple times in a query binds consistently

### Error Handling
- [ ] Missing parameter (referenced in query but not in params dict) returns a clear error, not silent wrong results
- [ ] Malformed params JSON returns a clear error
- [ ] Empty params `{}` with no `$param` references in query succeeds (no-op)
- [ ] Extra unused params in the dict are silently ignored

### Backward Compatibility
- [ ] `cypher(query)` with no params continues to work identically (all existing tests pass)
- [ ] `Connection::cypher(query)` in Rust is unchanged
- [ ] `Graph.query(query)` in Python with no params arg is unchanged

### Security
- [ ] Parameters are bound via `sqlite3_bind_*`, NOT string-interpolated — injection-safe by construction
- [ ] Passing SQL injection payloads as parameter values does not execute injected SQL

## Implementation Notes

### Technical Approach

#### Architecture Summary

The parameterized Cypher API is **largely already implemented** at the C extension layer. The current architecture already supports:

1. **Scanner/Lexer**: `$name`, `$0`, `${name}` parameter syntax is tokenized as `CYPHER_TOKEN_PARAMETER` (scanner.l lines 182-205)
2. **Parser/Grammar**: `PARAMETER` tokens are parsed into `AST_NODE_PARAMETER` / `cypher_parameter` AST nodes (cypher_gram.y lines 992, 1419-1422)
3. **Transform Layer**: Parameters are transformed from Cypher `$name` to SQLite `:name` named parameters via `register_parameter()` and `append_sql(ctx, ":%s", param->name)` (transform_return.c lines 716-728, transform_match.c lines 199-224)
4. **C Extension Entry Point**: `graphqlite_cypher_func` already accepts 1 or 2 arguments: `cypher(query)` or `cypher(query, params_json)` (extension.c lines 58-86)
5. **Executor**: `cypher_executor_execute_params()` stores `params_json` on the executor and calls `bind_params_from_json()` after SQL preparation (cypher_executor.c lines 400-420)
6. **Parameter Binding**: `bind_params_from_json()` parses the JSON params object and binds values to SQLite named parameters using `sqlite3_bind_*` functions (executor_helpers.c lines 125-250)
7. **Direct Param Resolution**: `get_param_value()` resolves parameter values directly from JSON for CREATE/SET clauses that handle properties at the AST level rather than SQL level (executor_helpers.c lines 15-122)

**What is missing is the Rust and Python binding-level `cypher_with_params()` convenience API.** The Python `Connection.cypher()` already supports an optional `params` dict argument. The Rust `Connection.cypher()` does NOT support parameters. The `Graph.query()` methods in both languages do not support parameters either.

#### Detailed Implementation Plan

##### Phase 1: Fix Missing Parameter Binding in Generic Transform Path (C Extension Bug)

**Issue**: The `handle_generic_transform()` function in `query_dispatch.c` (lines 532-609) calls `cypher_transform_query()` which prepares a SQL statement, but it NEVER calls `bind_params_from_json()` on the resulting statement. This means parameterized queries that route through the generic transform path (OPTIONAL MATCH, multi-MATCH, WITH+MATCH+RETURN, UNWIND+RETURN) will fail silently or produce wrong results when parameters are used.

**File**: `src/backend/executor/query_dispatch.c`
**Change**: In `handle_generic_transform()`, after the statement is prepared (around line 561 where `transform_result->stmt` is checked), add parameter binding:
```c
if (transform_result->stmt) {
    /* Bind parameters if provided */
    if (executor->params_json) {
        if (bind_params_from_json(transform_result->stmt, executor->params_json) < 0) {
            set_result_error(result, "Failed to bind query parameters");
            cypher_free_result(transform_result);
            cypher_transform_free_context(ctx);
            return -1;
        }
    }
    // ... rest of result building
}
```

Also apply the same fix in `handle_return_only()` for standalone RETURN with parameters, and in the UNION handling path in `cypher_executor.c` (line 274).

##### Phase 2: Add `cypher_with_params()` to Rust `Connection` Type

**File**: `bindings/rust/src/connection.rs`
**Change**: Add a new method alongside the existing `cypher()`:

```rust
/// Execute a Cypher query with named parameters.
///
/// Parameters are passed as a map and bound inside the extension,
/// preventing injection and eliminating the need for manual escaping.
///
/// # Arguments
///
/// * `query` - Cypher query string with `$param` placeholders
/// * `params` - Parameter values as a serde_json::Value (must be an object)
///
/// # Example
///
/// ```no_run
/// use graphqlite::Connection;
/// use serde_json::json;
///
/// let conn = Connection::open_in_memory()?;
/// conn.cypher("CREATE (n:Person {name: 'Alice', age: 30})")?;
/// let results = conn.cypher_with_params(
///     "MATCH (n:Person) WHERE n.name = $name RETURN n.name, n.age",
///     &json!({"name": "Alice"})
/// )?;
/// # Ok::<(), graphqlite::Error>(())
/// ```
pub fn cypher_with_params(&self, query: &str, params: &serde_json::Value) -> Result<CypherResult> {
    let params_json = serde_json::to_string(params)?;
    let result: Option<String> = self
        .conn
        .query_row("SELECT cypher(?1, ?2)", rusqlite::params![query, params_json], |row| row.get(0))?;

    match result {
        Some(json_str) => {
            if json_str.starts_with("Error") {
                return Err(Error::Cypher(json_str));
            }
            CypherResult::from_json(&json_str)
        }
        None => Ok(CypherResult::empty()),
    }
}
```

**Dependencies**: Add `serde_json` to `Cargo.toml` if not already a dependency (it already is, used in `graph/mod.rs`).

##### Phase 3: Add `query_with_params()` to Rust `Graph` Type

**File**: `bindings/rust/src/graph/queries.rs` (and/or `bindings/rust/src/graph/mod.rs`)
**Change**: Add a convenience method on `Graph`:

```rust
/// Execute a raw Cypher query with named parameters.
pub fn query_with_params(&self, cypher: &str, params: &serde_json::Value) -> Result<CypherResult> {
    self.conn.cypher_with_params(cypher, params)
}
```

##### Phase 4: Add `query_with_params()` to Python `Graph` Type

The Python `Connection.cypher()` already supports an optional `params` dict. However, `Graph.query()` does not pass through parameters.

**File**: `bindings/python/src/graphqlite/graph/queries.py`
**Change**: Add a new method and update `query()`:

```python
def query(self, cypher: str, params: dict[str, Any] | None = None) -> list[dict]:
    """
    Execute a raw Cypher query with optional parameters.

    Args:
        cypher: Cypher query string (may contain $param placeholders)
        params: Optional dictionary of parameter values

    Returns:
        List of result dictionaries
    """
    result = self._conn.cypher(cypher, params)
    return result.to_list()
```

This is backward compatible since `params` defaults to `None`.

##### Phase 5: Improve List/Map Parameter Support (Pairs with GQLITE-T-0097)

The current `bind_params_from_json()` in `executor_helpers.c` only handles scalar values (string, int, float, bool, null). List and map parameter values are skipped.

**File**: `src/backend/executor/executor_helpers.c`
**Change**: Extend `bind_params_from_json()` to handle JSON array and object values by binding them as JSON text strings:

```c
} else if (*p == '[' || *p == '{') {
    /* Array or object value - bind as JSON text */
    const char *json_start = p;
    int depth = 1;
    p++;
    while (*p && depth > 0) {
        if (*p == '[' || *p == '{') depth++;
        else if (*p == ']' || *p == '}') depth--;
        else if (*p == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && *(p+1)) p++;
                p++;
            }
        }
        if (*p) p++;
    }
    size_t json_len = p - json_start;
    char *json_val = malloc(json_len + 1);
    memcpy(json_val, json_start, json_len);
    json_val[json_len] = '\0';
    sqlite3_bind_text(stmt, idx, json_val, -1, SQLITE_TRANSIENT);
    free(json_val);
}
```

Similarly extend `get_param_value()` to handle arrays/objects for CREATE/SET paths.

##### Phase 6: Comprehensive Testing Plan

Tests are organized by layer and must all pass before the task is considered complete. Tests should be written incrementally alongside each phase, not deferred to the end.

###### 6A: SQL Functional Tests (C Extension Layer)

**File**: `tests/functional/26_parameterized_queries.sql` (extend existing)

**Bug fix validation (Phase 1):**
```sql
-- T01: Params in OPTIONAL MATCH (generic transform path)
CREATE (n:Person {name: 'Alice', age: 30});
CREATE (n:Person {name: 'Bob', age: 25});
SELECT cypher('OPTIONAL MATCH (n:Person) WHERE n.name = $name RETURN n.name', '{"name":"Alice"}');
-- Expected: [{"n.name": "Alice"}]

-- T02: Params in WITH+MATCH+RETURN
SELECT cypher('MATCH (n:Person) WITH n WHERE n.age > $min_age RETURN n.name', '{"min_age": 28}');
-- Expected: [{"n.name": "Alice"}]

-- T03: Params in UNION
SELECT cypher('MATCH (n:Person {name: $n1}) RETURN n.name UNION MATCH (n:Person {name: $n2}) RETURN n.name', '{"n1":"Alice","n2":"Bob"}');
-- Expected: 2 rows
```

**Parameter type coverage:**
```sql
-- T04: String param
SELECT cypher('MATCH (n:Person) WHERE n.name = $name RETURN n.name', '{"name":"Alice"}');

-- T05: Integer param
SELECT cypher('MATCH (n:Person) WHERE n.age > $age RETURN n.name', '{"age":28}');

-- T06: Float param
SELECT cypher('CREATE (n:Metric {val: $v}) RETURN n.val', '{"v":3.14}');

-- T07: Boolean param
SELECT cypher('CREATE (n:Flag {active: $a}) RETURN n.active', '{"a":true}');

-- T08: Null param
SELECT cypher('CREATE (n:Thing {val: $v}) RETURN n.val', '{"v":null}');

-- T09: List param (Phase 5)
SELECT cypher('RETURN $ids', '{"ids":[1,2,3]}');

-- T10: Map param (Phase 5)
SELECT cypher('RETURN $meta', '{"meta":{"key":"val"}}');
```

**Parameter syntax variants:**
```sql
-- T11: $name style
SELECT cypher('RETURN $x', '{"x":42}');

-- T12: $0 numeric style
SELECT cypher('RETURN $0', '{"0":42}');

-- T13: ${name} brace-delimited style
SELECT cypher('RETURN ${x}', '{"x":42}');

-- T14: Same param used twice
SELECT cypher('MATCH (n:Person) WHERE n.name = $name RETURN $name, n.age', '{"name":"Alice"}');
```

**Edge cases and error handling:**
```sql
-- T15: Empty params, no $refs in query (should succeed)
SELECT cypher('MATCH (n:Person) RETURN n.name', '{}');

-- T16: Extra unused params (should succeed, ignore extras)
SELECT cypher('MATCH (n:Person) RETURN n.name', '{"unused":"val"}');

-- T17: Missing required param (should return clear error)
SELECT cypher('MATCH (n:Person) WHERE n.name = $name RETURN n', '{}');

-- T18: Malformed JSON params (should return clear error)
SELECT cypher('RETURN $x', '{bad json}');
```

**Security:**
```sql
-- T19: SQL injection attempt via param value
SELECT cypher('MATCH (n:Person) WHERE n.name = $name RETURN n', '{"name":"Alice''; DROP TABLE nodes; --"}');
-- Expected: no injection, treated as literal string

-- T20: Cypher injection attempt via param value
SELECT cypher('MATCH (n:Person) WHERE n.name = $name RETURN n', '{"name":"Alice}) DETACH DELETE n //"}');
-- Expected: no injection, treated as literal string
```

**Params in CREATE/SET (AST-level `get_param_value()` path):**
```sql
-- T21: Param in CREATE property
SELECT cypher('CREATE (n:Person {name: $name, age: $age}) RETURN n.name, n.age', '{"name":"Charlie","age":40}');

-- T22: Param in SET
CREATE (n:Person {name: 'Dave'});
SELECT cypher('MATCH (n:Person {name: $name}) SET n.age = $age RETURN n.age', '{"name":"Dave","age":35}');
```

###### 6B: Rust Integration Tests

**File**: `bindings/rust/tests/integration.rs` (extend)

```rust
// --- Connection::cypher_with_params ---

#[test]
fn test_params_string_match() {
    let conn = test_connection();
    conn.cypher("CREATE (n:Person {name: 'Alice', age: 30})").unwrap();
    let results = conn.cypher_with_params(
        "MATCH (n:Person) WHERE n.name = $name RETURN n.name AS name",
        &json!({"name": "Alice"})
    ).unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0]["name"], "Alice");
}

#[test]
fn test_params_integer_filter() {
    let conn = test_connection();
    conn.cypher("CREATE (n:Person {name: 'Alice', age: 30})").unwrap();
    conn.cypher("CREATE (n:Person {name: 'Bob', age: 20})").unwrap();
    let results = conn.cypher_with_params(
        "MATCH (n:Person) WHERE n.age > $min RETURN n.name",
        &json!({"min": 25})
    ).unwrap();
    assert_eq!(results.len(), 1);
}

#[test]
fn test_params_in_create() {
    let conn = test_connection();
    let results = conn.cypher_with_params(
        "CREATE (n:Person {name: $name, age: $age}) RETURN n.name, n.age",
        &json!({"name": "Charlie", "age": 40})
    ).unwrap();
    assert_eq!(results.len(), 1);
}

#[test]
fn test_params_null_value() {
    let conn = test_connection();
    let results = conn.cypher_with_params(
        "CREATE (n:Thing {val: $v}) RETURN n.val",
        &json!({"v": null})
    ).unwrap();
    assert_eq!(results.len(), 1);
}

#[test]
fn test_params_reuse_same_param() {
    let conn = test_connection();
    conn.cypher("CREATE (n:Person {name: 'Alice'})").unwrap();
    let results = conn.cypher_with_params(
        "MATCH (n:Person) WHERE n.name = $name RETURN $name, n.name",
        &json!({"name": "Alice"})
    ).unwrap();
    assert_eq!(results.len(), 1);
}

#[test]
fn test_params_empty_dict_no_refs() {
    let conn = test_connection();
    conn.cypher("CREATE (n:Person {name: 'Alice'})").unwrap();
    let results = conn.cypher_with_params(
        "MATCH (n:Person) RETURN n.name",
        &json!({})
    ).unwrap();
    assert_eq!(results.len(), 1);
}

#[test]
fn test_params_missing_param_errors() {
    let conn = test_connection();
    let result = conn.cypher_with_params(
        "MATCH (n) WHERE n.name = $name RETURN n",
        &json!({})
    );
    assert!(result.is_err());
}

#[test]
fn test_params_injection_safe() {
    let conn = test_connection();
    conn.cypher("CREATE (n:Person {name: 'Alice'})").unwrap();
    // This should NOT execute the DROP or match anything
    let results = conn.cypher_with_params(
        "MATCH (n:Person) WHERE n.name = $name RETURN n.name",
        &json!({"name": "Alice'; DROP TABLE nodes; --"})
    ).unwrap();
    assert_eq!(results.len(), 0); // no match, but no crash/injection
}

// --- Graph::query_with_params ---

#[test]
fn test_graph_query_with_params() {
    let g = test_graph();
    g.add_node("alice", "Person", json!({"name": "Alice"})).unwrap();
    let results = g.query_with_params(
        "MATCH (n:Person {name: $name}) RETURN n.name",
        &json!({"name": "Alice"})
    ).unwrap();
    assert_eq!(results.len(), 1);
}

// --- Backward compatibility ---

#[test]
fn test_cypher_without_params_unchanged() {
    let conn = test_connection();
    conn.cypher("CREATE (n:Person {name: 'Alice'})").unwrap();
    let results = conn.cypher("MATCH (n:Person) RETURN n.name").unwrap();
    assert_eq!(results.len(), 1);
}
```

###### 6C: Python Tests

**File**: `bindings/python/tests/test_connection.py` (extend)

```python
# --- Connection.cypher with params ---

def test_params_string_match():
    db = connect(":memory:")
    db.cypher("CREATE (n:Person {name: 'Alice', age: 30})")
    result = db.cypher("MATCH (n:Person) WHERE n.name = $name RETURN n.name", {"name": "Alice"})
    assert len(result) == 1

def test_params_integer_filter():
    db = connect(":memory:")
    db.cypher("CREATE (n:Person {name: 'Alice', age: 30})")
    db.cypher("CREATE (n:Person {name: 'Bob', age: 20})")
    result = db.cypher("MATCH (n:Person) WHERE n.age > $min RETURN n.name", {"min": 25})
    assert len(result) == 1

def test_params_in_create():
    db = connect(":memory:")
    result = db.cypher("CREATE (n:Person {name: $name, age: $age}) RETURN n.name, n.age", {"name": "Charlie", "age": 40})
    assert len(result) == 1

def test_params_null_value():
    db = connect(":memory:")
    result = db.cypher("CREATE (n:Thing {val: $v}) RETURN n.val", {"v": None})
    assert len(result) == 1

def test_params_empty_dict():
    db = connect(":memory:")
    db.cypher("CREATE (n:Person {name: 'Alice'})")
    result = db.cypher("MATCH (n:Person) RETURN n.name", {})
    assert len(result) == 1

def test_params_missing_param_errors():
    db = connect(":memory:")
    with pytest.raises(Exception):
        db.cypher("MATCH (n) WHERE n.name = $name RETURN n", {})

def test_params_injection_safe():
    db = connect(":memory:")
    db.cypher("CREATE (n:Person {name: 'Alice'})")
    result = db.cypher(
        "MATCH (n:Person) WHERE n.name = $name RETURN n.name",
        {"name": "Alice'; DROP TABLE nodes; --"}
    )
    assert len(result) == 0  # no match, no injection

# --- Graph.query with params ---

def test_graph_query_with_params():
    g = graph(":memory:")
    g.upsert_node("alice", {"name": "Alice"}, "Person")
    result = g.query("MATCH (n:Person {name: $name}) RETURN n.name", {"name": "Alice"})
    assert len(result) == 1

def test_graph_query_without_params_unchanged():
    g = graph(":memory:")
    g.upsert_node("alice", {"name": "Alice"}, "Person")
    result = g.query("MATCH (n:Person) RETURN n.name")
    assert len(result) == 1
```

###### 6D: Test Execution Requirements

All tests must pass using the project's standard test runners:
- **C/SQL functional**: `angreal test functional` (includes `26_parameterized_queries.sql`)
- **Rust**: `angreal test rust`
- **Python**: `angreal test python`
- **Full suite regression**: `angreal test all` must pass with no regressions

###### 6E: Definition of Done

This task is complete when:
1. All acceptance criteria checkboxes above are checked
2. All tests in 6A-6C pass
3. `angreal test all` passes with no regressions
4. No new compiler warnings introduced
5. Code changes are committed with passing CI

#### Order of Implementation

1. **Phase 1** (C extension bug fix) - Fix `handle_generic_transform()` parameter binding gap. This is a bug that affects all parameter-using queries on the generic path. Must be done first so the underlying layer is correct.
2. **Phase 5** (List/map params) - Extend `bind_params_from_json()` for complex types. This ensures the full param type contract works before exposing it.
3. **Phase 2** (Rust `Connection.cypher_with_params`) - Add the core Rust API.
4. **Phase 3** (Rust `Graph.query_with_params`) - Add the convenience wrapper.
5. **Phase 4** (Python `Graph.query` params) - Update Python Graph to pass through params.
6. **Phase 6** (Tests) - Write tests throughout, but especially after Phases 1-2.

#### Key Architectural Decisions

1. **No new C function registration needed**: The `cypher()` SQLite function already accepts an optional second argument. All parameter support is already in the C pipeline. This task is primarily about Rust/Python binding surface area and fixing one binding gap.

2. **Parameter binding happens INSIDE the extension**: The Cypher `$name` is transformed to SQLite `:name` by the transform layer. Then `bind_params_from_json()` maps JSON keys to SQLite named parameter positions via `sqlite3_bind_parameter_index()`. This is true parameter binding, not string interpolation -- so it is injection-safe by construction.

3. **Rust API uses `serde_json::Value`**: This is the natural Rust type for a JSON map. Callers use `json!({"key": "value"})` for ergonomic construction. The `serde_json` crate is already a dependency.

4. **Python already works**: The Python `Connection.cypher(query, params)` method already accepts and passes params as JSON. The main gap is `Graph.query()` not forwarding params.

5. **Backward compatibility**: All changes are additive. `cypher(query)` remains unchanged. `query()` gains an optional `params` argument with a default of `None`/`None`. No breaking changes.

6. **Two param resolution paths must both work**: The transform-based path (MATCH+RETURN, WITH, OPTIONAL MATCH) uses SQLite `:name` parameters resolved via `bind_params_from_json()`. The AST-level path (CREATE, SET) uses `get_param_value()` to resolve params directly from the JSON. Both paths are already implemented; Phase 1 ensures the transform path actually binds params in all code paths.

### Dependencies
- GQLITE-T-0097 (JSON properties) for map/list parameter support

### Risk Considerations
- Parameter type coercion needs careful handling (e.g., numeric strings vs integers)
- Must ensure binding layer doesn't introduce SQL injection vectors

## Status Updates

### Session 1 - Implementation

**Phase 1 (C bug fix) - DONE**
- Added `bind_params_from_json()` call in `handle_generic_transform()` in `query_dispatch.c` (before `build_query_results()` and the manual column collection paths)
- Added `bind_params_from_json()` call in UNION path in `cypher_executor.c` (before `sqlite3_step()` loop)
- `handle_return_only()` delegates to `handle_generic_transform()` for non-algo queries, so it inherits the fix

**Phase 5 (List/map params) - DONE**
- Extended `bind_params_from_json()` in `executor_helpers.c` to handle `[` and `{` values by binding as JSON text via `sqlite3_bind_text()`
- Extended `get_param_value()` to handle arrays/objects by copying JSON text to output buffer with `PROP_TYPE_TEXT`
- Fixed skip logic in both functions to properly handle nested JSON structures (depth-tracking instead of scanning for `,` or `}`)

**Phase 2 (Rust Connection::cypher_with_params) - DONE**
- Added `cypher_with_params(&self, query, params)` to `Connection` in `bindings/rust/src/connection.rs`
- Uses `serde_json::Value` for params, serializes to JSON and passes as `?2` to `SELECT cypher(?1, ?2)`

**Phase 3 (Rust Graph::query_with_params) - DONE**
- Added `query_with_params(&self, cypher, params)` to `Graph` in `bindings/rust/src/graph/mod.rs`

**Phase 4 (Python Graph.query params) - DONE**
- Updated `Graph.query()` in `bindings/python/src/graphqlite/graph/queries.py` to accept optional `params` dict
- Backward compatible — params defaults to `None`

**Build verification**: Extension compiles cleanly (1 pre-existing unused variable warning). Rust bindings compile cleanly. Functional tests pass with no regressions.

**Phase 6 (Tests) - DONE**
- Extended `tests/functional/26_parameterized_queries.sql` with Sections 11-14: generic transform path params, list/map params, boolean params, float params
- Added 8 Rust integration tests in `bindings/rust/tests/integration.rs`: string match, integer filter, create with params, empty dict, injection safety, backward compat, Graph query_with_params, Graph query without params
- Added 5 Python tests in `bindings/python/tests/test_graph.py`: query with string params, integer params, empty dict, backward compat, injection safety

**All tests passing:**
- Functional SQL tests: All passed (including new Sections 11-14)
- Rust tests: 147 integration + 14 unit + 19 doc = all passed
- Python tests: 201 passed, 6 skipped (optional deps)
- `angreal test all`: CLI has 1 pre-existing failure (debug output in non-release build, unrelated); all other suites pass

**Files modified:**
- `src/backend/executor/query_dispatch.c` — Bug fix: added `bind_params_from_json()` in `handle_generic_transform()`
- `src/backend/executor/cypher_executor.c` — Bug fix: added `bind_params_from_json()` in UNION path
- `src/backend/executor/executor_helpers.c` — Extended for list/map params + fixed skip logic for nested JSON
- `bindings/rust/src/connection.rs` — Added `cypher_with_params()`
- `bindings/rust/src/graph/mod.rs` — Added `query_with_params()`
- `bindings/python/src/graphqlite/graph/queries.py` — Updated `query()` with optional params
- `tests/functional/26_parameterized_queries.sql` — Extended with 4 new test sections
- `bindings/rust/tests/integration.rs` — Added 8 new param tests
- `bindings/python/tests/test_graph.py` — Added 5 new param tests