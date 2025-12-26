---
id: refactor-rust-bindings-module
level: task
title: "Refactor Rust bindings module structure"
short_code: "GQLITE-T-0041"
created_at: 2025-12-25T18:14:36.277517+00:00
updated_at: 2025-12-25T20:15:07.036985+00:00
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

# Refactor Rust bindings module structure

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[Parent Initiative]]

## Objective **[REQUIRED]**

Break up the 1,958-line god struct in `graph.rs` (61 methods) into focused modules for maintainability.

## Backlog Item Details **[CONDITIONAL: Backlog Item]**

{Delete this section when task is assigned to an initiative}

### Type
- [ ] Bug - Production issue that needs fixing
- [ ] Feature - New functionality or enhancement  
- [ ] Tech Debt - Code improvement or refactoring
- [ ] Chore - Maintenance or setup work

### Priority
- [ ] P0 - Critical (blocks users/revenue)
- [ ] P1 - High (important for user experience)
- [ ] P2 - Medium (nice to have)
- [ ] P3 - Low (when time permits)

### Impact Assessment **[CONDITIONAL: Bug]**
- **Affected Users**: {Number/percentage of users affected}
- **Reproduction Steps**: 
  1. {Step 1}
  2. {Step 2}
  3. {Step 3}
- **Expected vs Actual**: {What should happen vs what happens}

### Business Justification **[CONDITIONAL: Feature]**
- **User Value**: {Why users need this}
- **Business Value**: {Impact on metrics/revenue}
- **Effort Estimate**: {Rough size - S/M/L/XL}

### Technical Debt Impact **[CONDITIONAL: Tech Debt]**
- **Current Problems**: {What's difficult/slow/buggy now}
- **Benefits of Fixing**: {What improves after refactoring}
- **Risk Assessment**: {Risks of not addressing this}

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] No single file exceeds 400 lines
- [ ] Graph impl blocks split across focused modules
- [ ] Algorithms organized by family (centrality, community, paths, traversal, etc.)
- [ ] Shared parsing logic extracted to `algorithms/parsing.rs`
- [ ] Redundant aliases removed (keep one: `bfs` not `breadth_first_search`, etc.)
- [ ] Unused `namespace` field removed
- [ ] Result types consolidated in `algorithms/mod.rs`
- [ ] All existing tests pass
- [ ] Batch operations use transactions

## Test Cases **[CONDITIONAL: Testing Task]**

{Delete unless this is a testing task}

### Test Case 1: {Test Case Name}
- **Test ID**: TC-001
- **Preconditions**: {What must be true before testing}
- **Steps**: 
  1. {Step 1}
  2. {Step 2}
  3. {Step 3}
- **Expected Results**: {What should happen}
- **Actual Results**: {To be filled during execution}
- **Status**: {Pass/Fail/Blocked}

### Test Case 2: {Test Case Name}
- **Test ID**: TC-002
- **Preconditions**: {What must be true before testing}
- **Steps**: 
  1. {Step 1}
  2. {Step 2}
- **Expected Results**: {What should happen}
- **Actual Results**: {To be filled during execution}
- **Status**: {Pass/Fail/Blocked}

## Documentation Sections **[CONDITIONAL: Documentation Task]**

{Delete unless this is a documentation task}

### User Guide Content
- **Feature Description**: {What this feature does and why it's useful}
- **Prerequisites**: {What users need before using this feature}
- **Step-by-Step Instructions**:
  1. {Step 1 with screenshots/examples}
  2. {Step 2 with screenshots/examples}
  3. {Step 3 with screenshots/examples}

### Troubleshooting Guide
- **Common Issue 1**: {Problem description and solution}
- **Common Issue 2**: {Problem description and solution}
- **Error Messages**: {List of error messages and what they mean}

### API Documentation **[CONDITIONAL: API Documentation]**
- **Endpoint**: {API endpoint description}
- **Parameters**: {Required and optional parameters}
- **Example Request**: {Code example}
- **Example Response**: {Expected response format}

## Implementation Notes

### Current State
- `graph.rs`: 1,958 lines, 61 public methods on single Graph struct
- 7+ duplicate result parsing blocks (DRY violation)
- 6 redundant method aliases (betweenness/betweenness_centrality, etc.)
- Unused `namespace` field
- 12 result types scattered in file
- `connection.rs`, `error.rs`, `result.rs` are fine

### Target Structure
```
src/
├── lib.rs
├── connection.rs                  # As-is
├── error.rs                       # As-is
├── result.rs                      # As-is
├── utils.rs                       # escape_string, sanitize_rel_type
├── graph/
│   ├── mod.rs                     # Graph struct, new(), query()
│   ├── nodes.rs                   # impl Graph { has_node, get_node, ... }
│   ├── edges.rs                   # impl Graph { has_edge, get_edge, ... }
│   ├── queries.rs                 # impl Graph { stats, neighbors, ... }
│   └── batch.rs                   # impl Graph { upsert_nodes_batch, ... }
└── algorithms/
    ├── mod.rs                     # Result types, re-exports
    ├── parsing.rs                 # extract_node_id, extract_score, etc.
    ├── centrality.rs              # impl Graph { pagerank, degree_centrality, ... }
    ├── community.rs               # impl Graph { community_detection, louvain }
    ├── components.rs              # impl Graph { wcc, scc }
    ├── paths.rs                   # impl Graph { shortest_path, dijkstra, astar }
    ├── traversal.rs               # impl Graph { bfs, dfs }
    └── similarity.rs              # impl Graph { node_similarity, triangle_count }
```

### Parsing Helpers to Extract
```rust
// algorithms/parsing.rs
pub(super) fn extract_node_id(row: &Row) -> Option<String>;
pub(super) fn extract_user_id(row: &Row) -> Option<String>;
pub(super) fn extract_score(row: &Row, field: &str) -> f64;
pub(super) fn extract_component_id(row: &Row) -> Option<i64>;
```

### Aliases to Remove
- `betweenness()` → keep `betweenness_centrality()`
- `closeness()` → keep `closeness_centrality()`
- `breadth_first_search()` → keep `bfs()`
- `depth_first_search()` → keep `dfs()`
- `a_star()` → keep `astar()`
- `triangles()` → keep `triangle_count()`
- `jaccard()`, `jaccard_similarity()` → keep `node_similarity()`

## Status Updates **[REQUIRED]**

*To be added during implementation*