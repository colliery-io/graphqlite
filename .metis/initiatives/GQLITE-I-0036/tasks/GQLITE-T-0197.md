---
id: aggregate-multi-match-variable
level: task
title: "Aggregate multi-MATCH variable bindings before write clauses"
short_code: "GQLITE-T-0197"
created_at: 2026-04-18T21:43:04.453067+00:00
updated_at: 2026-04-18T22:42:02.224374+00:00
parent: GQLITE-I-0036
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0036
---

# Aggregate multi-MATCH variable bindings before write clauses

## Objective

Make queries with multiple standalone MATCH clauses (`MATCH (a) MATCH (b) ...`) union all MATCH variable bindings into a single `variable_map` before any subsequent write clause runs. Currently only the first MATCH is picked up, causing CREATE/MERGE/SET to see unbound variables and (in the CREATE case) silently create phantom anonymous nodes.

## Size: M-L (3-5 days)

## Resolves

- GQLITE-T-0190.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `execute_match_create_query` in `src/backend/executor/executor_match.c` iterates every `AST_NODE_MATCH` clause in `query->clauses`, runs each through `transform_match_clause`, and unions the resulting bindings into a single var_map before the CREATE clause executes.
- [ ] Same treatment applied to `execute_match_merge_query` (for `MATCH (a) MATCH (b) MERGE ...`).
- [ ] Same treatment for MATCH+SET, MATCH+REMOVE, MATCH+DELETE paths.
- [ ] `MATCH (a:N {node_id:"a"}) MATCH (b:N {node_id:"b"}) CREATE (a)-[:T]->(b)` produces an edge `1→2` (no phantom node). Verify via direct inspection of `nodes` and `edges` tables.
- [ ] `MATCH (a:N {...}) MATCH (b:N {...}) CREATE (a)-[:T {k:$v}]->(b)` writes the parameter on the edge.
- [ ] Same-name rebinding: if two MATCH clauses bind the same variable, the second wins (Neo4j semantics). Document in a comment. Add regression test.
- [ ] Comma-separated MATCH `MATCH (a), (b)` continues to work unchanged.
- [ ] Full functional suite passes.

## Implementation Notes

- Current defect site: `execute_match_create_query` calls `find_match_clause(query)` (returns only the first MATCH). Replace with a loop over `query->clauses` filtered by `AST_NODE_MATCH`.
- Each MATCH needs a fresh `cypher_transform_context`. The unified `var_map` accumulates across iterations.
- Per-clause WHERE predicates must apply to the right aliases before bindings merge.
- OPTIONAL MATCH (LEFT JOIN) is different. If `CLAUSE_OPTIONAL` is set, leave routing to existing `handle_generic_transform`; don't break that path.

## Risks

- Queries that relied on the buggy behavior (second MATCH silently no-op'd) may slow down — unlikely but possible.
- Memory/ownership: each per-clause transform context needs clean tear-down. Run ASAN on functional suite after landing.

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[GQLITE-I-0036]]

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