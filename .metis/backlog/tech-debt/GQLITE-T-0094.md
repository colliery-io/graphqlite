---
id: update-batch-bindings-to-use
level: task
title: "Update batch bindings to use transactions instead of for loops"
short_code: "GQLITE-T-0094"
created_at: 2026-01-10T04:16:05.171987+00:00
updated_at: 2026-01-10T13:55:47.109934+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#tech-debt"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Update batch bindings to use transactions instead of for loops

Refactor existing batch methods (`upsert_nodes_batch`, `upsert_edges_batch`) to wrap operations in a single transaction rather than executing individual upserts in a loop.

## Objective

Improve batch operation performance by wrapping multiple upsert calls in a single SQLite transaction, reducing fsync overhead and providing atomicity guarantees.

## Problem

The current batch methods are implemented as simple for loops:

```rust
// Current implementation - no transaction wrapping
pub fn upsert_nodes_batch(...) {
    for (node_id, props, label) in nodes {
        self.upsert_node(node_id, props, label)?;  // Each call is its own transaction
    }
}
```

Without explicit transaction wrapping, SQLite auto-commits after each statement. This means:
1. Each insert triggers an fsync to disk (slow)
2. No atomicity - partial failures leave inconsistent state
3. Unnecessary overhead from repeated transaction begin/commit

## Proposed Solution

Wrap batch operations in explicit transactions:

```rust
pub fn upsert_nodes_batch(...) -> Result<()> {
    self.begin_transaction()?;
    for (node_id, props, label) in nodes {
        if let Err(e) = self.upsert_node(node_id, props, label) {
            self.rollback()?;
            return Err(e);
        }
    }
    self.commit()?;
    Ok(())
}
```

## Backlog Item Details

### Type
- [x] Tech Debt - Code improvement or refactoring

### Priority
- [x] P1 - High (important for user experience)

### Technical Debt Impact
- **Current Problems**: Batch operations are slow due to per-operation transaction overhead; no atomicity guarantees
- **Benefits of Fixing**: 5-10x performance improvement for batch operations; atomic batch inserts (all-or-nothing)
- **Risk Assessment**: Low risk - straightforward refactoring with clear semantics

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `upsert_nodes_batch` wraps all operations in a single transaction
- [ ] `upsert_edges_batch` wraps all operations in a single transaction
- [ ] Transaction rolls back on any individual operation failure
- [ ] Python bindings maintain the same API (transparent improvement)
- [ ] Benchmark shows measurable improvement for 100+ item batches
- [ ] Unit tests verify atomicity (partial failure = full rollback)

## Implementation Notes

### Technical Approach
1. Add `begin_transaction()`, `commit()`, and `rollback()` methods to Graph if not already present
2. Modify `upsert_nodes_batch` to wrap operations in transaction
3. Modify `upsert_edges_batch` to wrap operations in transaction
4. Ensure proper error handling with rollback on failure
5. Consider adding optional transaction parameter for caller-controlled transactions

### Dependencies
- None - can be implemented independently
- Related to GQLITE-T-0093 (bulk insert feature) which will need similar transaction handling

### Risk Considerations
- Nested transaction handling if caller is already in a transaction
- Large batches may hold locks longer - consider chunking for very large batches

## Status Updates

### Resolution (2026-01-10)

**Outcome**: Resolved differently than originally planned.

Transaction wrapping for batch methods conflicts with the Cypher extension's internal transaction management, causing syntax errors and rollback failures.

**Solution implemented**:
1. **Bulk insert methods** (GQLITE-T-0093) provide the high-performance atomic batch operations users need
2. **Batch methods** remain as convenience wrappers with documented limitations

**Key differences**:
| Aspect | `upsert_*_batch` | `insert_*_bulk` |
|--------|------------------|-----------------|
| Semantics | Upsert (MERGE) | Insert only |
| Atomicity | No | Yes |
| Performance | ~1x (no improvement) | 100-500x faster |
| Use case | Mixed workloads | Building new graphs |

**Documentation updated** to clearly state that batch methods do not provide atomicity, and users should use bulk methods for atomic operations.