---
id: variables-from-merge-are-unbound
level: task
title: "Variables from MERGE are unbound in subsequent WITH/SET clauses"
short_code: "GQLITE-T-0143"
created_at: 2026-03-28T00:46:59.327842+00:00
updated_at: 2026-03-28T02:15:37.925541+00:00
parent: 
blocked_by: []
archived: true

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# Variables from MERGE are unbound in subsequent WITH/SET clauses

**GitHub Issue**: #36
**Priority**: P1 - High

## Objective

Make variables bound by MERGE persist through subsequent WITH clauses so they can be used in downstream MATCH/MERGE/SET operations.

## Bug Description

After `MERGE (i:Label {id: ...}) SET i.prop = ...`, the variable `i` is not available in a following `WITH i` clause. This blocks the common pattern of MERGE a node then pipe through WITH to attach relationships.

## Root Cause

In `src/backend/executor/executor_merge.c`, variables are bound in a local `variable_map` that gets freed with `free_variable_map(var_map)` when MERGE execution completes (lines 699, 1141). No `MERGE+WITH+*` pattern exists in `query_dispatch.c`, and the executor doesn't pass bound variables between clause handlers.

## Reproduction

```cypher
CREATE (c:Company {id: 'acme'})
MERGE (i:Employee {id: 'emp-1'}) ON CREATE SET i.hired = 123
SET i.name = 'Alice'
WITH i
MATCH (c:Company {id: 'acme'})
MERGE (c)-[:EMPLOYS]->(i)
-- Error: "Unbound variable in SET: i"
```

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Variables bound by MERGE persist through WITH into subsequent clauses
- [ ] MERGE + SET + WITH + MATCH + MERGE pattern works end-to-end
- [ ] Repro test passes: `TestIssue36` in `test_issue_repro.py`

## Affected Files

- `src/backend/executor/executor_merge.c` — variable_map lifetime
- `src/backend/executor/query_dispatch.c` — needs MERGE+WITH pattern or variable passing between handlers

## Status Updates

### 2026-03-27: Investigation — architectural blocker identified

**Finding:** This is NOT a simple fix. MERGE is handled entirely at the executor level (in `executor_merge.c`) because it has CREATE-if-not-exists semantics that can't be expressed as SQL. The generic transform pipeline (`cypher_transform_query`) has no `AST_NODE_MERGE` case and errors on it.

The query `MERGE ... SET ... WITH ... MATCH ... MERGE ...` requires:
1. Execute MERGE (executor-level, creates/matches node)
2. Execute SET (needs variable from step 1)
3. Pipe variable through WITH
4. Execute MATCH (SQL-level)  
5. Execute second MERGE (executor-level, needs variables from steps 3-4)

This is a clause pipeline problem — variables must flow between executor-level operations (MERGE) and SQL-level operations (MATCH). Currently, each pattern handler runs in isolation with its own variable map.

**Possible approaches:**
- Add a `MERGE+SET+WITH+MATCH+MERGE` pattern handler (narrow fix, fragile)
- Implement a general clause pipeline executor that can chain executor-level and SQL-level clauses (proper fix, significant effort)
- Store MERGE output in a temp table that WITH can reference (intermediate approach)

**Recommendation:** This should be elevated to an initiative with its own decomposition. It touches `query_dispatch.c` pattern matching, `executor_merge.c` variable lifetime, and potentially requires a new pipeline executor.

### 2026-03-27: Implemented via clause pipeline handler

**Approach:** Added a `MERGE+WITH` pattern handler (priority 55) that splits execution at the WITH boundary:
1. Executes pre-WITH MERGE + SET using existing `execute_merge_clause` + `execute_set_operations` with re-resolved variable map
2. Transforms and executes post-WITH MATCH to find additional variables
3. Calls new `execute_merge_with_variables()` for post-WITH MERGE with combined variable map (pre-bound MERGE variables + MATCH results)

Also added `CLAUSE_WITH` to forbidden list for `MATCH+SET` and `MATCH+MERGE` patterns to prevent them from incorrectly matching queries that contain WITH boundaries.

**Changes (3 files):**
- `src/backend/executor/query_dispatch.c` — new `handle_merge_with_pipeline()`, pattern entry, WITH forbidden on MATCH+SET/MATCH+MERGE
- `src/backend/executor/executor_merge.c` — new `execute_merge_with_variables()`
- `src/include/executor/executor_internal.h` — declaration

**Test results:**
- 921/921 C unit tests pass
- `TestIssue36::test_merge_variable_in_with_then_match` — PASSES
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