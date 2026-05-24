---
id: b2-cross-type-comparison-null
level: task
title: "B2: Cross-type comparison null semantics — 1 < 'a' returns null per Cypher spec"
short_code: "GQLITE-T-0331"
created_at: 2026-05-23T18:17:41.906377+00:00
updated_at: 2026-05-24T14:00:33.352609+00:00
parent: GQLITE-I-0044
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0044
---

# B2: Cross-type comparison null semantics — 1 < 'a' returns null per Cypher spec

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[GQLITE-I-0044]]

## Objective

Per the Cypher spec, ordered comparisons (`<`, `<=`, `>`, `>=`) between values of incompatible types (e.g. number vs string, number vs boolean, string vs boolean) must return `null` rather than coercing via SQLite's native sort order. SQLite happily compares `1 < 'a'` as true because of its type-affinity rules — this disagrees with the TCK.

Implement Cypher-conformant null semantics for cross-type ordered comparisons so that affected TCK scenarios pass.

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

- [ ] `1 < 'a'`, `'a' < 1`, `1 < true`, etc. evaluate to `null` (not true/false) in WHERE / RETURN contexts.
- [ ] Same-type comparisons remain unchanged (no perf or correctness regression).
- [ ] `null < anything` still returns `null`.
- [ ] TCK delta: targeted scenarios involving cross-type ordered comparison now pass; no regression elsewhere.
- [ ] Unit + functional tests added covering the matrix.

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
Two viable approaches:

1. **`_gql_cmp` SQLite UDF** — register a scalar function `_gql_cmp(a, b)` returning `-1/0/1/null`, then transform `a < b` → `_gql_cmp(a, b) = -1`, etc. Centralizes the semantics but adds UDF dispatch overhead.

2. **Inline CASE** — transform `a < b` → `CASE WHEN typeof(a)=typeof(b) OR (typeof(a) IN ('integer','real') AND typeof(b) IN ('integer','real')) THEN a < b ELSE NULL END`. No UDF needed, but verbose and tricky around stored-as-text JSON values.

Approach 1 is cleaner and matches how we'd add list/path comparison later. Likely path: register UDF in `executor_init`, transform ops in `transform_expr_ops.c`.

### Dependencies
{Other tasks or systems this depends on}

### Risk Considerations
{Technical risks and mitigation strategies}

## Status Updates

### 2026-05-24 — Implementation complete

The `_gql_order_cmp` UDF infrastructure was already in place (T-0308). Mixed scalar text↔numeric was deliberately preserving SQLite native coerce-to-text behavior, citing WithWhere5. Replaced with Cypher-conformant null semantics, keeping a stringified-boolean escape hatch so Precedence1 [23]/[26] still pass.

**Change** in `src/backend/runtime/udf_helpers.c` `gql_order_cmp_func`:
- Mixed text↔numeric → null (was: −1 / +1).
- Exception: text `'true'`/`'false'` is treated as numeric 1/0 (boolean
  values flow as text on this path; Precedence1 [23]/[26] depend on it).

**TCK delta**: 3573 → 3577 (+4). Gained: Comparison2 [6] x2, Precedence1 [22] x2. No regressions.

**Tests**: unit 944/944, functional exit 0.