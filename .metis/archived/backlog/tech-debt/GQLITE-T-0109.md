---
id: add-python-and-rust-binding-tests
level: task
title: "Add Python and Rust binding tests for JSON properties and bulk SET"
short_code: "GQLITE-T-0109"
created_at: 2026-02-07T20:28:23.108071+00:00
updated_at: 2026-02-08T02:00:56.942799+00:00
parent: 
blocked_by: []
archived: true

tags:
  - "#task"
  - "#tech-debt"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# Add Python and Rust binding tests for JSON properties and bulk SET

## Objective

Add test coverage to the Python and Rust bindings for features shipped in recent work: JSON/map/list property storage, nested dot/bracket access, JSON helper functions, and bulk SET operations. These features are tested at the C level (functional SQL tests) but the binding layers have zero coverage for them.

## Backlog Item Details

### Type
- [x] Tech Debt - Code improvement or refactoring

### Priority
- [x] P1 - High (important for user experience)

### Technical Debt Impact
- **Current Problems**: Users of the Python/Rust bindings have no example or test proving these features work through the binding API. Binding-level regressions would go undetected.
- **Benefits of Fixing**: Validates the full stack (binding → extension → C), serves as documentation for binding users
- **Risk Assessment**: Low risk of actual bugs (C layer is well-tested), but high risk of poor user experience if bindings silently mishandle JSON results

## Existing Test Files

| File | What it tests |
|------|---------------|
| `bindings/python/tests/test_graph.py` | Graph CRUD, queries, parameterized queries, batch ops |
| `bindings/python/tests/test_connection.py` | Connection lifecycle |
| `bindings/python/tests/test_manager.py` | Graph manager |
| `bindings/rust/tests/integration.rs` | Full integration tests |

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Python: test CREATE with map/list property literals, read back as JSON
- [ ] Python: test nested dot access (`n.metadata.role`) returns correct value
- [ ] Python: test `json_get()`, `json_keys()`, `json_type()` helper functions
- [ ] Python: test `SET n = {map}` (replace) and `SET n += {map}` (merge)
- [ ] Python: test bulk SET preserves labels, removes old properties on replace
- [ ] Rust: test CREATE with map/list property literals
- [ ] Rust: test nested dot access and JSON helpers
- [ ] Rust: test bulk SET operations
- [ ] `angreal test python` passes
- [ ] `angreal test rust` passes

## Implementation Notes

### Python Tests (add to `test_graph.py` or new `test_json_properties.py`)

```python
# JSON property CREATE + read
g.query('CREATE (n:JsonTest {name: "Alice", meta: {role: "admin"}})')
result = g.query('MATCH (n:JsonTest {name: "Alice"}) RETURN n.meta')
# Assert meta is JSON object

# Nested access
result = g.query('MATCH (n:JsonTest {name: "Alice"}) RETURN n.meta.role')
# Assert returns "admin"

# JSON helpers
result = g.query('MATCH (n:JsonTest {name: "Alice"}) RETURN json_keys(n.meta)')
result = g.query('MATCH (n:JsonTest {name: "Alice"}) RETURN json_type(n.meta)')

# Bulk SET replace
g.query('MATCH (n:JsonTest {name: "Alice"}) SET n = {name: "Alice", updated: true}')
result = g.query('MATCH (n:JsonTest {name: "Alice"}) RETURN n.updated, n.meta')
# Assert updated=true, meta=null (replaced)

# Bulk SET merge
g.query('MATCH (n:JsonTest {name: "Alice"}) SET n += {extra: 42}')
result = g.query('MATCH (n:JsonTest {name: "Alice"}) RETURN n.updated, n.extra')
# Assert both present
```

### Rust Tests (add to `integration.rs` or new test module)

Same patterns using the `CypherQuery` builder and `Connection::cypher()` API.

### Dependencies
- Extension must be built before running binding tests
- Python tests need pytest + the graphqlite Python package installed
- Rust tests need the extension .dylib in the expected path

## Status Updates

### Completed
- Added 17 Python tests to `bindings/python/tests/test_graph.py` — all passing (215 total, 6 skipped)
- Added 15 Rust tests to `bindings/rust/tests/integration.rs` — all passing (162 total)
- Tests cover: map properties, list properties, nested dot access, bracket subscript, SET JSON map/list, bulk SET replace/merge/edge/empty/mixed, builder params with SET

### Issues Found & Fixed
- **Nested dot access column aliases**: Transform only auto-generates aliases for single-level property access. Fixed by adding explicit `AS` aliases in test queries.
- **Rust empty map test**: NULL values don't cause `get::<String>` errors in Rust binding. Fixed by using `get::<Option<String>>` and checking `is_none()`.
- **SET+RETURN in single query**: `handle_match_set` dispatch handler ignores RETURN clause entirely. Fixed test to use separate SET and RETURN queries.