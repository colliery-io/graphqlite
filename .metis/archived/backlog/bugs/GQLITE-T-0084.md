---
id: variable-length-paths-in-match
level: task
title: "Variable-length paths in MATCH+RETURN queries cause syntax error"
short_code: "GQLITE-T-0084"
created_at: 2026-01-04T15:19:14.677175+00:00
updated_at: 2026-01-05T15:30:11.166198+00:00
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

# Variable-length paths in MATCH+RETURN queries cause syntax error

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[Parent Initiative]]

## Objective

Fix the parser/query dispatch to properly handle variable-length path patterns (e.g., `[:KNOWS*]`) in MATCH+RETURN queries.

## Backlog Item Details

### Type
- [x] Bug - Production issue that needs fixing

### Priority
- [ ] P2 - Medium (nice to have)

### Impact Assessment
- **Affected Users**: Users attempting to traverse graphs with variable-length paths
- **Reproduction Steps**: 
  1. Create nodes and relationships: `CREATE (a:Person)-[:KNOWS]->(b:Person)-[:KNOWS]->(c:Person)`
  2. Run: `MATCH path = (a:Person)-[:KNOWS*]->(end) RETURN end.name`
  3. Observe syntax error
- **Expected vs Actual**: 
  - Expected: Query returns all nodes reachable via KNOWS relationships
  - Actual: `Query failed: Line 1: syntax error, unexpected END_P, expecting ')'`

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `MATCH path = (a)-[:REL*]->(b) RETURN b` executes without syntax error
- [ ] Variable-length path with bounds works: `[:REL*1..3]`
- [ ] CLI test added to verify this functionality

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
The parser likely handles `[:RELTYPE*]` syntax, but the query dispatch system (`query_dispatch.c`) may not properly handle path variables in MATCH+RETURN patterns. Investigation needed in:
- `src/backend/executor/query_dispatch.c` - Pattern matching for MATCH+RETURN
- `src/backend/parser/cypher_gram.y` - Variable-length path grammar rules

### Dependencies
None

### Risk Considerations
Low risk - this is additive functionality

## Status Updates **[REQUIRED]**

### 2026-01-05: Completed
- Root cause: `end` was a reserved keyword (for CASE...END) and couldn't be used as identifier
- Fixed by adding `END_P` token to grammar rules for identifiers and property access
- Updated `%expect-rr` from 2 to 3 to handle new GLR parser ambiguity
- Added regression test `test_end_as_identifier_regression`
- All 762 C unit tests pass
- Committed as 3e2dc6e

Note: The original ticket title mentions "variable-length paths" but that feature works fine.
The actual issue was using `end` as the variable name in the example query.