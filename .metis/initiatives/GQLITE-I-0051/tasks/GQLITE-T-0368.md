---
id: h1-make-the-python-performance
level: task
title: "H1: make the Python performance harness portable to macOS (no /proc, dylib default)"
short_code: "GQLITE-T-0368"
created_at: 2026-09-07T01:25:19.067543+00:00
updated_at: 2026-09-07T01:47:01.726404+00:00
parent: GQLITE-I-0051
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0051
---

# H1: make the Python performance harness portable to macOS (no /proc, dylib default)

## Parent Initiative

[[GQLITE-I-0051]]

## Objective **[REQUIRED]**

`tests/performance/python/harness.py` reads `/proc/self/status` and defaults to `graphqlite.so`, so it cannot run on macOS, which is where this work is being validated.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] `/proc` read when available, `ru_maxrss` fallback otherwise (bytes on macOS, kB on Linux).
- [ ] Default extension name follows the platform; README documents the fallback semantics.

## Status Updates **[REQUIRED]**

- 2026-09-06: created, implemented; used for all validation numbers in this initiative.