---
id: refactor-python-bindings-module
level: task
title: "Refactor Python bindings module structure"
short_code: "GQLITE-T-0040"
created_at: 2025-12-25T18:14:36.217246+00:00
updated_at: 2025-12-25T19:59:45.708264+00:00
parent: 
blocked_by: []
archived: true

tags:
  - "#task"
  - "#tech-debt"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Refactor Python bindings module structure

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[Parent Initiative]]

## Objective **[REQUIRED]**

Break up the 1,227-line god class `Graph` in `graph.py` into focused modules for maintainability.

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

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] No single file exceeds 300 lines
- [ ] Graph class uses mixin composition from separate modules
- [ ] Algorithms organized by family (centrality, community, paths, traversal, etc.)
- [ ] Shared parsing logic extracted to `_parsing.py`
- [ ] Platform detection deduplicated into `_platform.py`
- [ ] Tests split by module (test_graph_nodes.py, test_algorithms_centrality.py, etc.)
- [ ] All existing tests pass
- [ ] Batch operations actually batch (use transactions)

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
- `graph.py`: 1,227 lines, 33+ methods on single Graph class
- `connection.py`: 252 lines (acceptable)
- Platform detection duplicated in `__init__.py` and `connection.py`
- 15+ algorithm methods with repetitive parsing patterns
- Batch ops are fake (just loops, no actual batching)

### Target Structure
```
src/graphqlite/
├── __init__.py
├── _platform.py                   # Extracted platform detection
├── connection.py
├── graph/
│   ├── __init__.py                # Graph class (mixin composition)
│   ├── nodes.py                   # NodeOps mixin
│   ├── edges.py                   # EdgeOps mixin
│   ├── queries.py                 # QueryOps mixin
│   └── batch.py                   # BatchOps mixin
├── algorithms/
│   ├── __init__.py
│   ├── _parsing.py                # Shared helpers
│   ├── centrality.py              # pagerank, degree, betweenness, closeness
│   ├── community.py               # label_propagation, louvain, leiden
│   ├── components.py              # wcc, scc
│   ├── paths.py                   # shortest_path, dijkstra, astar
│   ├── traversal.py               # bfs, dfs
│   └── similarity.py              # node_similarity, triangle_count
└── utils.py                       # escape_string, sanitize_rel_type
```

### Graph Composition Pattern
```python
class Graph(NodeOps, EdgeOps, QueryOps, BatchOps, Algorithms):
    def __init__(self, path_or_conn=":memory:"):
        self._conn = ...
```

### Files to Create/Modify
- DELETE: None (refactor only)
- CREATE: `_platform.py`, `graph/` package, `algorithms/` package, `utils.py`
- MODIFY: `__init__.py`, `connection.py` (use _platform)

## Status Updates **[REQUIRED]**

*To be added during implementation*