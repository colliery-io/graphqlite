---
id: graph-algorithms-should-return
level: task
title: "Graph algorithms should return user-defined node IDs alongside internal rowids"
short_code: "GQLITE-T-0010"
created_at: 2025-12-24T05:18:53.733240+00:00
updated_at: 2025-12-24T15:06:43.265133+00:00
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

# Graph algorithms should return user-defined node IDs alongside internal rowids

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[Parent Initiative]]

## Objective **[REQUIRED]**

Graph algorithms (PageRank, Label Propagation, Betweenness Centrality) currently return internal SQLite rowids as node identifiers. Users must manually map these back to their own node IDs, which is error-prone and creates friction in the API. This enhancement adds the user-defined `id` property to algorithm results alongside the internal rowid.

## Backlog Item Details **[CONDITIONAL: Backlog Item]**

{Delete this section when task is assigned to an initiative}

### Type
- [ ] Bug - Production issue that needs fixing
- [x] Feature - New functionality or enhancement  
- [ ] Tech Debt - Code improvement or refactoring
- [ ] Chore - Maintenance or setup work

### Priority
- [ ] P0 - Critical (blocks users/revenue)
- [ ] P1 - High (important for user experience)
- [x] P2 - Medium (nice to have)
- [ ] P3 - Low (when time permits)

### Impact Assessment **[CONDITIONAL: Bug]**
- **Affected Users**: {Number/percentage of users affected}
- **Reproduction Steps**: 
  1. {Step 1}
  2. {Step 2}
  3. {Step 3}
- **Expected vs Actual**: {What should happen vs what happens}

### Business Justification **[CONDITIONAL: Feature]**
- **User Value**: Users store nodes with meaningful IDs (e.g., "alice", "e1", UUIDs) but algorithms return SQLite rowids (1, 2, 3...). Users must fetch all nodes and build a mapping table to interpret results.
- **Business Value**: Reduces friction for graph analytics use cases. Critical for GraphRAG applications where node identity matters.
- **Effort Estimate**: S - Modify 3 functions in `graph_algorithms.c` to join with vertex table and include `id` property

### Technical Debt Impact **[CONDITIONAL: Tech Debt]**
- **Current Problems**: {What's difficult/slow/buggy now}
- **Benefits of Fixing**: {What improves after refactoring}
- **Risk Assessment**: {Risks of not addressing this}

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] `pagerank()` returns both `node_id` (rowid) and `user_id` (property) columns
- [ ] `labelPropagation()` returns both `node_id` (rowid) and `user_id` (property) columns
- [ ] `betweennessCentrality()` returns both `node_id` (rowid) and `user_id` (property) columns
- [ ] Nodes without an `id` property return NULL for `user_id`
- [ ] Python and Rust bindings work without modification (column names change)
- [ ] Existing tests updated to verify new column

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

## Implementation Notes **[CONDITIONAL: Technical Task]**

### Technical Approach

**File**: `src/backend/executor/graph_algorithms.c`

**Current behavior** (PageRank example):
```sql
SELECT v.id AS node_id, ? AS score FROM _graph_default_v v ...
```
Returns: `{"node_id": 1, "score": 0.15}`

**Proposed change**:
```sql
SELECT v.id AS node_id, 
       json_extract(v.properties, '$.id') AS user_id,
       ? AS score 
FROM _graph_default_v v ...
```
Returns: `{"node_id": 1, "user_id": "alice", "score": 0.15}`

**Functions to modify**:
1. `execute_pagerank()` - Add user_id column to result set
2. `execute_label_propagation()` - Add user_id column to result set  
3. `execute_betweenness_centrality()` - Add user_id column to result set

### Dependencies
None - self-contained change to graph algorithm output format

### Risk Considerations
- **Breaking change**: Clients relying on exact column count may break. Mitigated by adding new column rather than renaming.
- **Performance**: `json_extract` adds minimal overhead since properties are already loaded.

## Status Updates **[REQUIRED]**

*To be added during implementation*