---
id: label-disjunction-in-node-patterns
level: task
title: "Label disjunction in node patterns (n:Label1|Label2)"
short_code: "GQLITE-T-0125"
created_at: 2026-03-17T13:38:53.245690+00:00
updated_at: 2026-03-17T18:30:08.811862+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#feature"
  - "#phase/blocked"


exit_criteria_met: false
initiative_id: NULL
---

# Label disjunction in node patterns (n:Label1|Label2)

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[Parent Initiative]]

## Objective

Support label OR matching in node patterns: `MATCH (n:Person|Employee)` matches nodes with either label. Currently only AND (multiple labels) is supported. Coverage matrix Section 4.1.

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

## Status Updates

### Blocked — GLR grammar ambiguity

**Attempted approach**: Added `|` as a separator in `label_list` grammar rule (`:Label1|Label2`), with sentinel-based detection in `make_node_pattern` and OR-based JOIN generation in `transform_match.c`.

**Problem**: The `|` character creates a GLR ambiguity (5 S/R conflicts instead of 4) because it's already used in:
- Pattern comprehensions: `[(n)-[r]->(m) | expr]`
- Relationship type disjunction: `-[:TYPE1|TYPE2]->`
- List comprehension: `[x IN list | expr]`

The parser reports "syntax is ambiguous" at runtime for label disjunction patterns.

**Possible solutions**:
1. Add GLR merge rules to resolve the ambiguity based on context
2. Use `%dprec` or `%merge` directives for the conflicting rules
3. Handle `|` in label position at the scanner level (context-dependent tokenization)
4. Use a completely different syntax (e.g., `(n:Label1+Label2)` for disjunction)

**What was reverted**: All grammar, AST, and transform changes for label disjunction. Only list slicing changes remain.

**Next step**: Needs focused grammar engineering — not a quick win.