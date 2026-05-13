---
id: tck-extension-sigsegv-in-build
level: task
title: "[TCK] Extension SIGSEGV in build_query_results — 8 crashing scenarios"
short_code: "GQLITE-T-0218"
created_at: 2026-05-13T17:03:48.991954+00:00
updated_at: 2026-05-13T20:26:55.833124+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# Extension SIGSEGV in `executor_match.c:187` (`build_query_results`)

## Source
Filed during [[GQLITE-T-0211]] triage of the [[GQLITE-I-0037]] baseline run. See `docs/tck/baseline-2026-05-13.md`.

## Classification
- Type: bug
- Priority: P0
- Affected TCK scenarios: 8

## Description

8 TCK scenarios cause the SQLite extension worker process to segfault. macOS produces a crash report on each. The supervisor catches the dead worker and respawns it so the run completes, but every crash is a P0 bug — the C extension is reading from address 0x8 (NULL+offset).

**Stack** (from the macOS crash report on the first occurrence):
```
0  build_query_results          executor_match.c:187
1  handle_generic_transform     query_dispatch.c:620
2  dispatch_query_pattern       query_dispatch.c:569
3  cypher_executor_execute_ast  cypher_executor.c:215
4  cypher_executor_execute      cypher_executor.c:402
5  graphqlite_cypher_func       extension.c:140
```

**EXC_BAD_ACCESS (SIGSEGV), KERN_INVALID_ADDRESS at 0x0000000000000008** — a NULL+8 dereference, typical of an uninitialised struct field or a missing NULL-check on a result-set/column pointer in `build_query_results`.

## Affected feature files (top 4)

- `vendor/tck/features/clauses/with/With1.feature` — 3 scenario(s)
- `vendor/tck/features/clauses/with/With6.feature` — 3 scenario(s)
- `vendor/tck/features/clauses/with-skip-limit/WithSkipLimit2.feature` — 1 scenario(s)
- `vendor/tck/features/clauses/with-where/WithWhere7.feature` — 1 scenario(s)

## Reproduction

1. Build the extension (`angreal build extension`).
2. Run `angreal test tck --filter With1 --limit 5` (or any of the affected files).
3. macOS shows a crash dialog; `build/tck-results.json` records the scenario as `error: ExtensionCrash`.

## Resolution — 2026-05-13

`src/backend/executor/executor_match.c:build_query_results` now synthesizes identifier return-items from `sqlite3_column_count` / `sqlite3_column_name` when the caller arrived with `return_clause->items == NULL` (the generic-transform dispatch path on queries like `MATCH ... WITH ... MATCH ... RETURN *`). The original NULL+8 dereference at line 187 is gone.

**Verification (1615-scenario baseline, commit bbe6dd0 vs prior bff8aa6):**

| metric | before | after | delta |
| --- | ---: | ---: | ---: |
| pass    | 495 | 494 | -1 |
| fail    | 389 | 398 | +9 |
| error   | 542 | 534 | **-8** (crash class eliminated) |
| skipped | 189 | 189 |  0 |

The 1-pass dip is not a real regression — 4 negative-test scenarios in `clauses/with/With6` and `clauses/with-orderBy/WithOrderBy4` previously "passed" only because the extension crashed (counted by the comparator as "expected error raised"). With the crash gone, those queries now succeed silently when openCypher requires them to raise — moving into [[GQLITE-T-0222]] (accepts queries spec requires rejected). Three other scenarios moved from `error` → `pass` (real wins): With1[5], With6[5], WithWhere7[2].

Closing as fixed.

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).