---
id: refactor-rust-param-api-to-builder
level: task
title: "Refactor Rust param API to builder pattern (cypher_builder / query_builder)"
short_code: "GQLITE-T-0101"
created_at: 2026-02-07T13:10:52.982026+00:00
updated_at: 2026-02-07T13:22:34.827683+00:00
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

# Refactor Rust param API to builder pattern (cypher_builder / query_builder)

**Follows from**: GQLITE-T-0098 (Parameterized Cypher API)

## Objective

Replace `Connection::cypher_with_params()` and `Graph::query_with_params()` with a fluent builder pattern (`cypher_builder()` / `query_builder()`). This is phase 1 of a planned migration: introduce the builder now, deprecate the old methods, and in a future release swap `cypher()` / `query()` to return the builder directly.

## Backlog Item Details

### Type
- [x] Feature - New functionality or enhancement

### Priority
- [ ] P2 - Medium (nice to have)

### Business Justification
- **User Value**: Cleaner, more ergonomic API for parameterized queries; individual `.param()` calls avoid constructing JSON manually
- **Business Value**: Better API design attracts Rust users; builder pattern is idiomatic Rust
- **Effort Estimate**: S - New struct + thin wrappers, no C changes

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [x] `CypherQuery` builder struct exists in `bindings/rust/src/query_builder.rs`
- [x] `CypherQuery` is exported from `graphqlite` crate root
- [x] `Connection::cypher_builder(query)` returns `CypherQuery`
- [x] `Graph::query_builder(cypher)` returns `CypherQuery`
- [x] `.param(key, value)` sets individual params with `Into<serde_json::Value>`
- [x] `.params(&json!({...}))` sets bulk params from JSON object
- [x] `.run()` executes and returns `Result<CypherResult>`
- [x] `.run()` with no params works (equivalent to `cypher()`)
- [x] `Connection::cypher_with_params()` has `#[deprecated]` attribute pointing to builder
- [x] `Graph::query_with_params()` has `#[deprecated]` attribute pointing to builder
- [x] All existing tests pass with no regressions
- [x] New tests cover builder `.param()`, `.params()`, and no-params `.run()` paths

## Implementation Plan

### Step 1: Create `CypherQuery` builder struct

**New file**: `bindings/rust/src/query_builder.rs`

```rust
use crate::{Connection, CypherResult, Error, Result};

/// Builder for parameterized Cypher queries.
///
/// Construct via [`Connection::cypher_builder`] or [`Graph::query_builder`].
///
/// # Examples
///
/// ```no_run
/// use graphqlite::Connection;
/// use serde_json::json;
///
/// let conn = Connection::open_in_memory()?;
/// conn.cypher("CREATE (n:Person {name: 'Alice', age: 30})")?;
///
/// // Individual params:
/// let results = conn.cypher_builder("MATCH (n:Person) WHERE n.name = $name RETURN n")
///     .param("name", "Alice")
///     .run()?;
///
/// // Bulk params:
/// let results = conn.cypher_builder("MATCH (n:Person) WHERE n.age > $min RETURN n")
///     .params(&json!({"min": 25}))
///     .run()?;
/// # Ok::<(), graphqlite::Error>(())
/// ```
pub struct CypherQuery<'a> {
    conn: &'a Connection,
    query: &'a str,
    params: serde_json::Map<String, serde_json::Value>,
}

impl<'a> CypherQuery<'a> {
    pub(crate) fn new(conn: &'a Connection, query: &'a str) -> Self {
        CypherQuery {
            conn,
            query,
            params: serde_json::Map::new(),
        }
    }

    /// Set a single named parameter.
    pub fn param(mut self, key: &str, value: impl Into<serde_json::Value>) -> Self {
        self.params.insert(key.to_string(), value.into());
        self
    }

    /// Set multiple parameters from a JSON object.
    ///
    /// Merges into any previously set params. If `params` is not a JSON object, it is ignored.
    pub fn params(mut self, params: &serde_json::Value) -> Self {
        if let Some(obj) = params.as_object() {
            for (k, v) in obj {
                self.params.insert(k.clone(), v.clone());
            }
        }
        self
    }

    /// Execute the query and return results.
    pub fn run(self) -> Result<CypherResult> {
        if self.params.is_empty() {
            self.conn.cypher(self.query)
        } else {
            let params_value = serde_json::Value::Object(self.params);
            self.conn.execute_cypher_with_params(self.query, &params_value)
        }
    }
}
```

### Step 2: Refactor `Connection` internals

**File**: `bindings/rust/src/connection.rs`

- Extract the core param execution logic from `cypher_with_params()` into a `pub(crate) fn execute_cypher_with_params()` (so the builder can call it without going through the deprecated public method)
- Add `cypher_builder()`:
  ```rust
  pub fn cypher_builder<'a>(&'a self, query: &'a str) -> CypherQuery<'a> {
      CypherQuery::new(self, query)
  }
  ```
- Deprecate `cypher_with_params()`:
  ```rust
  #[deprecated(since = "0.4.0", note = "Use conn.cypher_builder(query).params(&json).run() instead")]
  pub fn cypher_with_params(&self, query: &str, params: &serde_json::Value) -> Result<CypherResult> {
      self.execute_cypher_with_params(query, params)
  }
  ```

### Step 3: Add `query_builder()` to `Graph`

**File**: `bindings/rust/src/graph/mod.rs`

```rust
pub fn query_builder<'a>(&'a self, cypher: &'a str) -> CypherQuery<'a> {
    self.conn.cypher_builder(cypher)
}

#[deprecated(since = "0.4.0", note = "Use g.query_builder(query).params(&json).run() instead")]
pub fn query_with_params(&self, cypher: &str, params: &serde_json::Value) -> Result<CypherResult> {
    self.conn.execute_cypher_with_params(cypher, params)
}
```

### Step 4: Export from `lib.rs`

**File**: `bindings/rust/src/lib.rs`

- Add `mod query_builder;`
- Add `CypherQuery` to the `pub use` line

### Step 5: Migrate tests to builder API

**File**: `bindings/rust/tests/integration.rs`

Replace all `cypher_with_params()` / `query_with_params()` calls with builder equivalents. Add new tests for:
- `.param("key", value)` individual param setting
- Chaining multiple `.param()` calls
- Mixing `.param()` and `.params()`
- `.run()` with no params (plain query via builder)

### Migration Roadmap (future, not this task)

1. **v0.4.0** (this task): Introduce `cypher_builder()` / `query_builder()`, deprecate `cypher_with_params()` / `query_with_params()`
2. **v0.5.0**: Change `cypher()` / `query()` to return `CypherQuery` builder, add `.run()` terminal. Migration: `conn.cypher("...")? ` becomes `conn.cypher("...").run()?`
3. **v0.6.0**: Remove deprecated `cypher_with_params()` / `query_with_params()`

## Files Modified

| File | Change |
|------|--------|
| `bindings/rust/src/query_builder.rs` | **New** — `CypherQuery` builder struct |
| `bindings/rust/src/connection.rs` | Add `cypher_builder()`, extract `execute_cypher_with_params()`, deprecate `cypher_with_params()` |
| `bindings/rust/src/graph/mod.rs` | Add `query_builder()`, deprecate `query_with_params()` |
| `bindings/rust/src/lib.rs` | Add `mod query_builder`, export `CypherQuery` |
| `bindings/rust/tests/integration.rs` | Migrate tests to builder, add new builder-specific tests |

## Verification

1. `angreal test rust` — all tests pass, deprecation warnings on old methods
2. Builder `.param()` with individual key-values works
3. Builder `.params()` with bulk JSON works
4. Builder `.run()` with no params works
5. No regressions in `angreal test all`

## Status Updates

### Completed - 2026-02-07

All steps implemented and verified:
- `CypherQuery` builder struct created in `query_builder.rs`
- `Connection::cypher_builder()` and `Graph::query_builder()` added
- `cypher_with_params()` / `query_with_params()` deprecated with `#[deprecated(since = "0.4.0")]`
- Internal `execute_cypher_with_params()` extracted for builder delegation
- `CypherQuery` exported from crate root
- Integration tests migrated: 8 old tests replaced with 11 builder tests
- `angreal test rust`: 149 passed, 0 failed, 24 doc-tests passed
- `angreal test all`: only pre-existing CLI debug test failure (unrelated)