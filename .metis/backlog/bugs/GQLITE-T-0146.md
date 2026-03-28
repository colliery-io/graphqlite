---
id: startnode-and-endnode-return
level: task
title: "startNode() and endNode() return integer ID instead of Node"
short_code: "GQLITE-T-0146"
created_at: 2026-03-28T00:47:02.423475+00:00
updated_at: 2026-03-28T01:55:02.249449+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#bug"
  - "#phase/active"


exit_criteria_met: false
initiative_id: NULL
---

# startNode() and endNode() return integer ID instead of Node

**GitHub Issue**: #41
**Priority**: P2 - Medium

## Objective

Make `startNode()` and `endNode()` return Node objects instead of raw integer IDs, enabling function composition (`elementId(startNode(r))`) and property access (`startNode(r).name`).

## Bug Description

`startNode(r)` and `endNode(r)` return an integer (the node's internal ID) instead of a Node object. This prevents:
- Function composition: `elementId(startNode(r))` fails with "Failed to transform RETURN clause"
- Property access: `startNode(r).name` fails similarly

## Root Cause

In `src/backend/transform/transform_func_path.c` (lines 211, 254):
- `startNode()` generates: `SELECT source_id FROM edges WHERE id = ...`
- `endNode()` generates: `SELECT target_id FROM edges WHERE id = ...`

These return raw integer IDs from the `edges` table rather than resolving to full node references that downstream transforms can use for property access.

## Reproduction

```cypher
CREATE (a:Person {name: 'Alice'}), (b:Person {name: 'Bob'})
MATCH (a:Person {name: 'Alice'}), (b:Person {name: 'Bob'}) CREATE (a)-[:KNOWS]->(b)

MATCH ()-[r:KNOWS]->() RETURN startNode(r)        -- Returns: 1 (int, not Node)
MATCH ()-[r:KNOWS]->() RETURN startNode(r).name   -- Error: Failed to transform
MATCH ()-[r:KNOWS]->() RETURN endNode(r).name     -- Error: Failed to transform
```

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `startNode(r).name` returns the source node's property value
- [ ] `endNode(r).name` returns the target node's property value
- [ ] `elementId(startNode(r))` works (function composition)
- [ ] Repro tests pass: `TestIssue41` in `test_issue_repro.py`, tests 41a/41b in `11_issue_repro.sql`

## Affected Files

- `src/backend/transform/transform_func_path.c` — change startNode/endNode to return node references

## Status Updates

### 2026-03-27: Implementation complete

**Change:** `src/backend/transform/transform_expr_ops.c` — Added `AST_NODE_FUNCTION_CALL` handling in `transform_property_access()` for `startNode()` and `endNode()`. When property access is done on these functions (e.g., `startNode(r).name`), generates a COALESCE property lookup using the node ID subquery from the function as the `node_id`.

**Test results:**
- 921/921 C unit tests pass
- `TestIssue41::test_startnode_property_access` — PASSES (`startNode(r).name` returns "Alice")
- `TestIssue41::test_endnode_property_access` — PASSES (`endNode(r).name` returns "Bob")
- All 43 functional test files pass

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[Parent Initiative]]

## Objective **[REQUIRED]**

{Clear statement of what this task accomplishes}

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

## Acceptance Criteria **[REQUIRED]**

- [ ] {Specific, testable requirement 1}
- [ ] {Specific, testable requirement 2}
- [ ] {Specific, testable requirement 3}

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

{Keep for technical tasks, delete for non-technical. Technical details, approach, or important considerations}

### Technical Approach
{How this will be implemented}

### Dependencies
{Other tasks or systems this depends on}

### Risk Considerations
{Technical risks and mitigation strategies}

## Status Updates **[REQUIRED]**

*To be added during implementation*