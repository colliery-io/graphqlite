---
id: fix-strict-aliasing-violations-in
level: task
title: "Fix strict-aliasing violations in executor property value passing"
short_code: "GQLITE-T-0141"
created_at: 2026-03-27T13:03:50.793919+00:00
updated_at: 2026-03-27T13:03:50.793919+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/backlog"
  - "#tech-debt"


exit_criteria_met: false
initiative_id: NULL
---

# Fix strict-aliasing violations in executor property value passing

## Objective

Eliminate strict-aliasing violations in the executor's property value passing pattern. Currently, `get_param_value()` and `evaluate_function_call_via_sqlite()` write `int64_t`, `double`, and `int` values into a `void*` backed by a `char[]` buffer, then callers read them back with `*(int64_t*)set_str_buf` etc. This is undefined behavior under C99 strict aliasing rules.

## Backlog Item Details

### Type
- [x] Tech Debt - Code improvement or refactoring

### Priority
- [x] P2 - Medium (nice to have)

### Technical Debt Impact
- **Current Problems**: `get_param_value()` in `executor_helpers.c` and `evaluate_function_call_via_sqlite()` in `executor_set.c` use `void*` output parameters backed by `char[]` buffers to pass int64/double/bool/string values. Callers type-pun with `*(int64_t*)set_str_buf`. This violates C99 strict aliasing (related to GitHub issue #46).
- **Benefits of Fixing**: Correct C99 semantics, no UB under `-fstrict-aliasing`, cleaner API surface.
- **Risk Assessment**: Low risk of breakage in practice (works on current compilers), but becomes a real bug if LTO or aggressive optimization is enabled.

## Affected Files

- `src/include/executor/cypher_schema.h` — define `property_value` union here (alongside `property_type` enum)
- `src/include/executor/executor_internal.h` — update `get_param_value()` signature
- `src/backend/executor/executor_helpers.c` — `get_param_value()` writes into union instead of `void*`
- `src/backend/executor/executor_set.c` — `evaluate_function_call_via_sqlite()` and all callers use union
- `src/backend/executor/executor_create.c` — uses `get_param_value()`, needs update
- `src/backend/executor/graph_algorithms.c` — uses `get_param_value()`, needs update

## Approach

Introduce a `property_value` union in `cypher_schema.h`:

```c
typedef union property_value {
    int64_t  as_int;
    double   as_real;
    int      as_bool;
    char     as_str[4096];
} property_value;
```

Change `get_param_value()` and `evaluate_function_call_via_sqlite()` signatures from `(void *out_value, size_t value_size)` to `(property_value *out_value)`. Update all call sites to read from the appropriate union member.

## Acceptance Criteria

- [ ] No `*(int64_t*)char_buf` or `*(double*)char_buf` casts remain in executor code
- [ ] `get_param_value()` and `evaluate_function_call_via_sqlite()` use `property_value` union
- [ ] All callers updated to use union members
- [ ] All existing tests pass (`angreal test all`)
- [ ] Builds clean with `-fstrict-aliasing -Wstrict-aliasing=2`

## Status Updates

*To be added during implementation*