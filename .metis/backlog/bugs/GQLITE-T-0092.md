---
id: detach-delete-deletes-all-nodes
level: task
title: "DETACH DELETE deletes all nodes instead of matched node"
short_code: "GQLITE-T-0092"
created_at: 2026-01-08T18:23:06.278364+00:00
updated_at: 2026-01-08T22:17:14.531690+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# DETACH DELETE deletes all nodes instead of matched node

## Objective

Fix the Cypher-to-SQL translation for DELETE operations to correctly include property constraints from MATCH patterns, preventing unintended deletion of all nodes.

## Backlog Item Details

### Type
- [x] Bug - Production issue that needs fixing

### Priority
- [x] P0 - Critical (blocks users/revenue)

### Severity
**High** - Data loss bug affecting all delete operations

### Version
graphqlite 0.2.0

### Impact Assessment
- **Affected Users**: All users performing targeted node deletions
- **Reproduction Steps**: 
  1. Create multiple nodes with distinct `id` properties
  2. Execute `MATCH (n {id: 'specific_id'}) DETACH DELETE n`
  3. Observe all nodes are deleted, not just the matched one
- **Expected vs Actual**: 
  - **Expected**: Only node matching `{id: 'specific_id'}` is deleted
  - **Actual**: All nodes in the database are deleted

### Workaround
None currently - delete operations are unsafe until fixed.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `MATCH (n {id: 'x'}) DETACH DELETE n` only deletes the node where id='x'
- [ ] `MATCH (n {prop: value}) DELETE n` respects property constraints
- [ ] `MATCH (n:Label {prop: value}) DETACH DELETE n` respects both label and property constraints
- [ ] Edges connected to deleted nodes are removed (DETACH behavior preserved)
- [ ] Nodes not matching the pattern remain untouched
- [ ] All existing DELETE tests pass
- [ ] New regression tests added for this specific scenario

## Reproduction

```rust
use graphqlite::Graph;

#[test]
fn test_delete_node_preserves_others() {
    let g = Graph::open_in_memory().unwrap();

    // Insert 3 nodes
    g.upsert_node("node_a", [("name", "A"), ("file", "f1")], "Test").unwrap();
    g.upsert_node("node_b", [("name", "B"), ("file", "f1")], "Test").unwrap();
    g.upsert_node("node_c", [("name", "C"), ("file", "f2")], "Test").unwrap();

    assert_eq!(g.stats().unwrap().nodes, 3);

    // Delete only node_a
    g.delete_node("node_a").unwrap();

    // EXPECTED: 2 nodes remain
    // ACTUAL: 0 nodes remain
    assert_eq!(g.stats().unwrap().nodes, 2);  // FAILS - returns 0
    assert!(!g.has_node("node_a").unwrap());  // FAILS - all gone
    assert!(g.has_node("node_b").unwrap());   // FAILS - deleted
    assert!(g.has_node("node_c").unwrap());   // FAILS - deleted
}
```

## Analysis

The `delete_node` method in `bindings/rust/src/graph/nodes.rs:78-85` generates:

```rust
let query = format!(
    "MATCH (n {{id: '{}'}}) DETACH DELETE n",
    escape_string(node_id)
);
```

For `node_id = "node_a"`, this produces:
```cypher
MATCH (n {id: 'node_a'}) DETACH DELETE n
```

**Observation**: Property matching works for reads (`has_node` uses `MATCH (n {id: '...'}) RETURN count(n)` and correctly finds only the matching node), but fails for deletes.

### Root Cause (Confirmed)

**AST Mutation Bug:** The `transform_match.c` file mutates the AST during transformation by setting `first_pair->key = NULL` (lines 669, 729, 923) to mark properties as "handled".

In `execute_match_delete_query` (`executor_delete.c`):
1. Line 31: `transform_match_clause(ctx, match)` transforms MATCH, mutating the AST
2. Line 76: `execute_match_return_query(executor, match, ...)` transforms the SAME `match` object again
3. The second transformation sees `first_pair->key = NULL`, skipping the property filter
4. Result: ALL nodes returned instead of filtered nodes → all deleted

**Proof:** `MATCH (n {id: 'node_a'}) RETURN n` correctly returns 1 row (single transform). `MATCH (n {id: 'node_a'}) DETACH DELETE n` deletes all nodes (double transform mutates AST).

## Implementation Notes

### Files to Fix
- `src/backend/executor/executor_delete.c` - Remove unnecessary first transform
- OR `src/backend/transform/transform_match.c` - Don't mutate AST

### Recommended Solution (Option A - Minimal Fix)

Remove the unnecessary `transform_match_clause` call in `execute_match_delete_query`. The `ctx` created at line 24 isn't used after line 31 anyway - it's just wasted work that happens to mutate the AST.

```c
// DELETE these lines (24-35) from execute_match_delete_query:
cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
if (!ctx) { ... }
if (transform_match_clause(ctx, match) < 0) { ... }
```

And remove the corresponding `cypher_transform_free_context(ctx)` calls.

### Alternative Solution (Option B - Better Long-term)

Fix `transform_match.c` to not mutate the AST. Instead of:
```c
first_pair->key = NULL;  /* Mark first property as handled */
```

Use a bitmap or set in the transform context to track which properties have been processed.

## Test Cases

### Test Case 1: Single Node Delete Preserves Others
- **Test ID**: TC-001
- **Steps**: Create 3 nodes, delete one by property match
- **Expected**: 2 nodes remain

### Test Case 2: Delete with Edges Preserves Unconnected
- **Test ID**: TC-002
- **Steps**: Create nodes with edges, delete connected node
- **Expected**: Unconnected nodes remain, edges to deleted node removed

### Test Case 3: Raw Cypher DETACH DELETE
- **Test ID**: TC-003
- **Steps**: Execute raw `MATCH (n {id: 'x'}) DETACH DELETE n`
- **Expected**: Only matched node deleted

## Status Updates

*To be added during implementation*