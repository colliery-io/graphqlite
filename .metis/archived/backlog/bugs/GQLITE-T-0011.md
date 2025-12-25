---
id: return-n-serializes-all-nodes-into
level: task
title: "RETURN n serializes all nodes into single JSON string instead of separate rows"
short_code: "GQLITE-T-0011"
created_at: 2025-12-24T14:16:05.505777+00:00
updated_at: 2025-12-24T14:38:27.912817+00:00
parent: 
blocked_by: []
archived: true

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# RETURN n serializes all nodes into single JSON string instead of separate rows

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[Parent Initiative]]

## Objective **[REQUIRED]**

Fix the Cypher executor to return node objects as separate rows instead of serializing all nodes into a single JSON string.

## Backlog Item Details **[CONDITIONAL: Backlog Item]**

{Delete this section when task is assigned to an initiative}

### Type
- [x] Bug - Production issue that needs fixing

### Priority
- [ ] P1 - High (important for user experience)

### Impact Assessment
- **Affected Users**: Anyone using `RETURN n` to get full node objects
- **Reproduction Steps**: 
  1. Create nodes: `CREATE (n:Person {name: 'Alice'})`
  2. Query: `MATCH (n) RETURN n`
  3. Observe result structure
- **Expected vs Actual**: 
  - **Expected (openCypher standard):** Multiple rows, each with `{"n": {"id": 1, "labels": [...], "properties": {...}}}`
  - **Actual:** Single row with `{"result": "[{\"n\": {...}}, {\"n\": {...}}]"}` - all nodes serialized as JSON string

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

- [ ] `MATCH (n) RETURN n` returns each node as a separate row
- [ ] Each row has key `n` containing the node object directly (not wrapped in JSON string)
- [ ] `RETURN n.name` continues to work (regression test)
- [ ] Python bindings can iterate: `for row in result: node = row["n"]`

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

### Technical Approach
The bug is likely in the C code that handles RETURN clauses. When returning a node variable (vs a property like `n.name`), the executor serializes all results into one JSON array string instead of emitting separate result rows.

**Likely location:** `src/backend/transform/transform_return.c` or the executor code that builds the final JSON result.

### Notes
- `RETURN n.name AS name` works correctly (returns scalars per row)
- `RETURN count(n)` works correctly
- Only `RETURN n` (full node object) exhibits this behavior

## Status Updates **[REQUIRED]**

*To be added during implementation*