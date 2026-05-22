---
id: fix-functional-test-suite-blocked
level: task
title: "Fix functional test suite blocked at test 26 (CREATE...RETURN)"
short_code: "GQLITE-T-0180"
created_at: 2026-03-29T17:25:30.858263+00:00
updated_at: 2026-03-29T17:34:32.350528+00:00
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

# Fix functional test suite blocked at test 26 (CREATE...RETURN)

## Objective

`tests/functional/26_parameterized_queries.sql` test 4.1 uses `CREATE (p:Person {...}) RETURN p.name` which hits the unimplemented `RETURN after CREATE` code path. This causes the test to fail with `-bail`, which stops the entire functional test suite. **All tests from 27 onward (including 37_call_subquery.sql) never run.**

## Impact

- **Severity**: P1 — silently blocks 10+ functional test files from running
- **Affected tests**: 27_foreach_clause.sql through 37_call_subquery.sql and beyond
- **Reproduction**: `angreal test functional` — stops at test 26

## Root Cause

`tests/functional/26_parameterized_queries.sql` line 78:
```sql
SELECT cypher('CREATE (p:Person {name: $name, age: $age, city: $city}) RETURN p.name', '{"name": "TestUser", "age": 40, "city": "Boston"}') as result;
```

The executor's `handle_create` pattern does not support a trailing RETURN clause — it returns `"RETURN after CREATE not yet implemented"`.

## Fix Options

1. **Fix the test** (quick): Split into `CREATE` then `MATCH ... RETURN` (avoids the unimplemented feature, tests what it intended to test — parameterized CREATE)
2. **Implement CREATE+RETURN** (proper): Add a `handle_create_return` pattern in `query_dispatch.c` that executes CREATE then returns the created node

Option 1 is the right fix for the test suite — the test is supposed to test parameterized queries, not CREATE+RETURN.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `angreal test functional` runs all test files through to completion (no early exit at 26)
- [ ] Test 26 parameterized CREATE still validates that the node was created correctly
- [ ] No test files are silently skipped

## Status Updates

*To be added during implementation*