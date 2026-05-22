---
id: rec-09-consolidate-extension-c-and
level: task
title: "REC-09: Consolidate extension.c and bundled_init.c into shared implementation"
short_code: "GQLITE-T-0168"
created_at: 2026-03-28T13:59:38.265929+00:00
updated_at: 2026-03-29T18:03:19.178719+00:00
parent: GQLITE-I-0033
blocked_by: []
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0033
---

# REC-09: Consolidate extension.c and bundled_init.c into shared implementation

## Objective

Eliminate code duplication between `extension.c` and `bundled_init.c` by extracting shared implementation into a common file. Addresses findings LEG-005, EVO-06, CF-2 (Major).

## Affected Files

- `src/extension.c` -- 6 SQLite function handlers + `connection_cache`
- `src/bundled_init.c` -- duplicated handlers
- New: `src/extension_impl.c` and `include/extension_impl.h` (shared implementation)

## What To Do

1. First determine if `bundled_init.c` is actually used in any build configuration; if not, delete it entirely
2. If both are needed: extract the 6 SQLite function handler implementations and `connection_cache` management into `extension_impl.c/.h`
3. Have both `extension.c` and `bundled_init.c` include and call the shared implementation
4. Ensure both loadable-extension and bundled build paths compile and test correctly

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] No duplicated function handler code between extension entry points
- [ ] Both build configurations (if applicable) compile and pass tests
- [ ] If `bundled_init.c` is unused, it is deleted with a note in the commit message
- [ ] All unit and functional tests pass

## Effort Estimate

3-5 days

## Status Updates

### 2026-03-29: Complete

`bundled_init.c` was never compiled in any build target — only referenced in the Makefile lint glob. Deleted the file and removed it from lint. No shared implementation needed since only `extension.c` is used.