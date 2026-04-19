---
id: windows-only-timestamp-returns-0
level: task
title: "Windows-only: timestamp() returns 0 in MATCH+SET and MERGE ON CREATE SET"
short_code: "GQLITE-T-0205"
created_at: 2026-04-18T23:50:04.271794+00:00
updated_at: 2026-04-18T23:50:04.271794+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/backlog"
  - "#bug"


exit_criteria_met: false
initiative_id: NULL
---

# Windows-only: timestamp() returns 0 in MATCH+SET and MERGE ON CREATE SET

## Objective

Restore `timestamp()` to return a positive millisecond value on Windows when used inside `SET n.x = timestamp()` and `MERGE (n) ON CREATE SET n.x = timestamp()`. Regressed between v0.4.3 (2026-03-31 Windows CI green) and the PR #62 branch tip.

## Backlog Item Details

### Type
- [x] Bug

### Priority
- [x] P2 - Medium (Windows-only; Linux and macOS paths are green; tests assert `ts > 0` which is the canonical smoke check)

### Impact Assessment
- **Affected Users**: Users on Windows running `SET n.x = timestamp()` or `MERGE ... ON CREATE SET n.x = timestamp()`. Other OSes unaffected.
- **Reproduction** (Windows only, via `bindings/rust/tests/integration.rs`):
  - `test_set_timestamp_function` (line 2858-2869): `MATCH (n) SET n.updated = timestamp(); RETURN n.updated` → `ts` deserializes to `i64` = 0.
  - `test_merge_on_create_set_timestamp` (line 2885-2895): `MERGE (n) ON CREATE SET n.created = timestamp(); RETURN n.created` → same.
- **Expected**: `ts > 0` (milliseconds since Unix epoch, ~1.7e12 in April 2026).
- **Actual on Windows CI** (MSYS2/MinGW64 runner, consistent across 2 runs): `ts == 0`.

### Validity
- [x] Reproduced on two CI runs (fail → rerun → fail) against commit `5b33296` on branch `release/0.4.4-issue-61-fixes`.
- **NOT reproduced** on Linux (Ubuntu) or macOS Rust integration runs on the same commit — all 250 tests pass there.
- Confirmed via `gh run view 23797890085` that the 0.4.3 commit (`9e08c80`) had these tests green on Windows, so the regression is somewhere in the 8 commits `9e08c80..5b33296`. PR #62 bisect via draft PRs #63/#64/#65 was launched to identify the culprit commit.

### Source-code surface
- `src/backend/transform/transform_func_list.c` lines 253-263: `transform_timestamp_function` emits `CAST((julianday('now') - 2440587.5) * 86400000 AS INTEGER)`. PR #62 does NOT touch this file.
- SET function-call evaluation: `executor_set.c` `execute_set_items` → `evaluate_function_call_via_sqlite`. PR #62 does NOT touch this file.
- Plausible interaction surfaces: PR #62's dispatcher/`execute_merge_clause_with_vars_ex` refactor, new function-local `static int64_t/double/int` buffers in `executor_create.c` / `executor_merge.c` PARAMETER branches, or the `MATCH+SET` dispatch-table forbidden-flag change. None of these should affect timestamp eval, but the Windows toolchain/linker could be exposing a latent issue.

## Acceptance Criteria

- [ ] `cargo test --test integration test_set_timestamp_function test_merge_on_create_set_timestamp` passes on Windows MSYS2/MinGW64 CI runner.
- [ ] Root cause identified — either a real regression in PR #62 (patch + add a targeted Windows functional test under `tests/functional/`) or a toolchain-level issue (documented as such; add a platform-skipped assertion if the bug is upstream).
- [ ] Linux and macOS paths remain green.

## Implementation Notes

Bisect approach currently running: draft PRs at commits `0e3e995` (c1), `3c12e61` (c3), `f47a4c2` (c5) to identify the culprit commit range. Once the bad commit is isolated, repeat with a finer bisect if needed and inspect the diff in detail.

## Status Updates

- 2026-04-18: Filed as a follow-up to PR #62 so the issue-#61 fixes are not blocked. Bisect via draft PRs #63/#64/#65 in flight.
- 2026-04-18: **Bisect complete — not caused by PR #62.** All three bisect probes (c1 `0e3e995`, c3 `3c12e61`, c5 `f47a4c2`) fail with the same 2 tests. Baseline probe at `0.4.3` tip + a noop commit (PR #66) **also fails** with the identical two tests and the exact `248 passed; 2 failed` split. This confirms the regression is in the GitHub Actions Windows runner image / MSYS2 / SQLite version between 2026-03-31 (when 0.4.3 tip CI was green on Windows per `gh run view 23797890085`) and today. No code change in issue-61 PR #62 is responsible.
- 2026-04-18: **Retriaged to Windows-runner environment bug.** Next investigation steps (not blocking 0.4.4 release):
  1. Diff the MSYS2 package versions (SQLite, MinGW GCC) between a pre-2026-04 runner image and today's to isolate which upgrade broke `julianday('now')` → integer cast.
  2. Reproduce locally on a Windows MSYS2 env with the same versions.
  3. Candidate fix: replace `CAST((julianday('now') - 2440587.5) * 86400000 AS INTEGER)` in `transform_func_list.c` with a platform-portable equivalent — e.g. `unixepoch('now') * 1000 + CAST(strftime('%f', 'now') AS INTEGER) % 1000` (SQLite ≥ 3.38) or `(strftime('%s','now') * 1000 + CAST(strftime('%f','now')*1000 AS INTEGER) % 1000)`.
  4. Gate the fix behind a targeted Windows functional test (in `tests/functional/`) so a future regression is caught on Linux CI too.
- 2026-04-18: Bisect branches `bisect/c1-0e3e995`, `bisect/c3-3c12e61`, `bisect/c5-f47a4c2`, `bisect/baseline-9e08c80` and associated draft PRs #63/#64/#65/#66 can be closed/deleted after acknowledgement.

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[Parent Initiative]]

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