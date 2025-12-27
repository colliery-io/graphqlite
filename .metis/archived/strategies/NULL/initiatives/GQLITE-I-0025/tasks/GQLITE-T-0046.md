---
id: create-dynamic-buffer-utility-for
level: task
title: "Create dynamic_buffer utility for growing strings"
short_code: "GQLITE-T-0046"
created_at: 2025-12-26T20:34:29.694265+00:00
updated_at: 2025-12-26T20:50:05.650327+00:00
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

# Create dynamic_buffer utility for growing strings

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[GQLITE-I-0025]]

## Objective

Create a reusable growing string buffer utility that will be the foundation for the unified SQL builder.

## Files to Create

- `src/backend/transform/sql_builder.h`
- `src/backend/transform/sql_builder.c`

## Implementation

### dynamic_buffer struct
```c
typedef struct {
    char *data;
    size_t len;
    size_t capacity;
} dynamic_buffer;
```

### Functions to Implement
```c
void dbuf_init(dynamic_buffer *buf);
void dbuf_free(dynamic_buffer *buf);
void dbuf_clear(dynamic_buffer *buf);
void dbuf_append(dynamic_buffer *buf, const char *str);
void dbuf_appendf(dynamic_buffer *buf, const char *fmt, ...);
void dbuf_append_char(dynamic_buffer *buf, char c);
char *dbuf_finish(dynamic_buffer *buf);  // Returns owned string, resets buffer
```

### Requirements
- Start with 256 byte initial capacity
- Double capacity on growth
- Handle NULL inputs gracefully
- All functions must check allocation failures

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] dynamic_buffer compiles without warnings
- [ ] Unit tests pass (test_dbuf_*)
- [ ] Makefile updated to build new files

## Unit Tests
- test_dbuf_init_free
- test_dbuf_append_simple
- test_dbuf_appendf_format
- test_dbuf_grow_large_string
- test_dbuf_clear_reuse