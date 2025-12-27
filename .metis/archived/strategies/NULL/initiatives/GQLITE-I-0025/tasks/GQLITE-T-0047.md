---
id: implement-sql-builder-v2-core-with
level: task
title: "Implement sql_builder_v2 core with sql_add_* functions"
short_code: "GQLITE-T-0047"
created_at: 2025-12-26T20:34:29.853806+00:00
updated_at: 2025-12-26T20:55:06.255181+00:00
parent: GQLITE-I-0025
blocked_by: []
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: GQLITE-I-0025
---

# Implement sql_builder_v2 core with sql_add_* functions

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[GQLITE-I-0025]]

## Objective

Build the core SQL builder on top of dynamic_buffer. Provides clause-based SQL construction that assembles correctly regardless of call order.

## Depends On
- GQLITE-T-0046 (dynamic_buffer utility)

## sql_builder struct
```c
typedef enum { SQL_JOIN_INNER, SQL_JOIN_LEFT, SQL_JOIN_CROSS } sql_join_type;

typedef struct {
    dynamic_buffer cte;          // WITH RECURSIVE ...
    dynamic_buffer select;       // SELECT columns
    dynamic_buffer from;         // FROM table
    dynamic_buffer joins;        // JOIN clauses
    dynamic_buffer where;        // WHERE conditions
    dynamic_buffer group_by;     // GROUP BY
    dynamic_buffer order_by;     // ORDER BY
    int limit;                   // -1 if not set
    int offset;                  // -1 if not set
    int select_count;
    int where_count;
    bool finalized;
} sql_builder;
```

## Functions to Implement
```c
sql_builder *sql_builder_create(void);
void sql_builder_free(sql_builder *b);
void sql_builder_reset(sql_builder *b);

void sql_select(sql_builder *b, const char *expr, const char *alias);
void sql_from(sql_builder *b, const char *table, const char *alias);
void sql_join(sql_builder *b, sql_join_type type, const char *table, 
              const char *alias, const char *on_condition);
void sql_where(sql_builder *b, const char *condition);
void sql_group_by(sql_builder *b, const char *expr);
void sql_order_by(sql_builder *b, const char *expr, bool desc);
void sql_limit(sql_builder *b, int limit, int offset);
void sql_cte(sql_builder *b, const char *name, const char *query);

char *sql_builder_to_string(sql_builder *b);
```

## Assembly Order
`sql_builder_to_string()` assembles: CTE → SELECT → FROM → JOIN → WHERE → GROUP BY → ORDER BY → LIMIT

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] All sql_* functions implemented
- [ ] sql_builder_to_string produces valid SQL
- [ ] Unit tests pass
- [ ] No memory leaks

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