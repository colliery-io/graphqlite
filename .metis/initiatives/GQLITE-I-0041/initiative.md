---
id: header-and-api-mechanical-cleanup
level: initiative
title: "Header and API Mechanical Cleanup"
short_code: "GQLITE-I-0041"
created_at: 2026-05-19T14:17:52.059954+00:00
updated_at: 2026-05-19T14:17:52.059954+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/discovery"


exit_criteria_met: false
estimated_complexity: S
initiative_id: header-and-api-mechanical-cleanup
---

# Header and API Mechanical Cleanup

## Context

The architectural review of 2026-05-19 identified a cluster of small
mechanical cleanups (Risks 4, 6, and the "quick wins" list) that share a
common shape — each is a one-shot fix with no behavior change, but
together they remove a lot of friction:

- **Risk 4 — API ossification.** `executor_internal.h:91-100` declares five
  variants of `execute_merge_clause_*`: `execute_merge_clause`,
  `execute_merge_clause_with_vars`, `execute_merge_clause_with_varmap`,
  `execute_merge_clause_with_vars_ex`, `execute_merge_with_variables`. They
  differ only in which combination of input and output `var_map` arguments
  they take. The same pattern affects `execute_match_*` (8 variants).
- **Risk 6 — Shadow / orphaned headers.**
  - `src/include/transform/transform_internal.h` (32 LOC) re-declares
    functions already declared in `cypher_transform.h`. Effectively dead.
  - `src/include/transform/transform_functions.h` (120 LOC) is a manifest
    header collecting 90+ function declarations spread across 8 `.c`
    files. Adding a new function requires editing both the .c file and
    this header. Fragile.
  - No `transform_func_*.h` per `.c` file — callers see the union of all
    function decls through one mega-header.
- **Quick wins:**
  - Three `.bak` files (`executor_set.c.bak`, `executor_remove.c.bak`,
    `executor_merge.c.bak`) under `src/backend/executor/`. The
    `.gitignore` lists `*.bak` but these predate it.
  - Generated parser files (`src/generated/cypher_gram.tab.{c,h}`,
    `cypher_scanner.c`) are committed to git with no documented
    regeneration discipline. Risk: contributor edits `.y`/`.l` source,
    forgets to regenerate, commit drifts.
  - No `CONTRIBUTING.md` documenting build/test taxonomy, ownership
    conventions, or the "before push, run X" checklist.

None of these is large alone. Together they reduce a non-trivial amount
of header / API noise and give new contributors a clearer entry point.

## Goals

- **G1**: Headers are normalized — one `.h` per `.c` where appropriate, no
  shadow declarations, no manifest mega-header.
- **G2**: `execute_*_with_*` variant proliferation collapsed to canonical
  signatures. New code physically can't pick the wrong variant — there's
  only one.
- **G3**: Quick wins shipped — `.bak` files removed, generated-files
  policy documented and enforced, `CONTRIBUTING.md` exists.
- **G4**: Zero regression on unit, functional, TCK.

## Non-Goals

- The god-file splits (covered by GQLITE-I-0040).
- SQL generation consolidation (covered by GQLITE-I-0039).
- Other transform-context cleanup beyond what header/API normalization
  forces.
- Bindings (Python/Rust) — separate initiative when their parity work
  happens.

## Detailed Design

### Variant collapse — canonical signature for `execute_*_clause`

Today's five MERGE variants take some combination of: optional
`external_vars` (input), optional `**out_var_map` (output). The canonical
signature is:

```c
int execute_merge_clause(cypher_executor *executor,
                         cypher_merge *merge,
                         cypher_result *result,
                         variable_map *in_vars,        /* NULL if none */
                         variable_map **out_vars);     /* NULL if caller doesn't want output */
```

Same pattern for `execute_match_*`. Five MERGE functions and 8 MATCH-shaped
functions collapse to one each. Callers that currently use the
parameterless variants pass `NULL` for both new args.

### Header normalization — one `.h` per `.c` for `transform_func_*`

Today: `transform_functions.h` declares 90+ functions across 8 `.c` files
in a single manifest. Each `.c` file should own its own header instead:

- `transform_func_string.h` — string decls from `transform_func_string.c`
- `transform_func_math.h` — math decls
- `transform_func_path.h`, `transform_func_entity.h`,
  `transform_func_aggregate.h`, `transform_func_graph.h`,
  `transform_func_list.h`, `transform_func_dispatch.h` — one each

After the split in GQLITE-I-0040, this also covers
`transform_func_temporal.h`, `transform_func_geo.h`,
`transform_func_json.h`, `transform_func_typeconv.h`.

Then delete `transform_functions.h` outright.

### Shadow header deletion

`transform_internal.h` is fully redundant with `cypher_transform.h`. After
confirming no callers (or migrating those callers), delete it.

### Generated-files policy

Two viable policies:

- **Option A**: Keep in git. Add CI check: `make parser` followed by
  `git diff --exit-code src/generated/` — fails if generated files are
  stale.
- **Option B**: `.gitignore` them. Add build step that always regenerates
  (current Makefile already does on demand).

Recommend Option A unless build times suffer — keeping generated files in
git makes `git clone && make` work without flex/bison installed locally,
which matters for the Rust binding build flow.

### `CONTRIBUTING.md` content

Short, ~80 lines. Covers:

- Build: `angreal build extension` is canonical; `make` is the underlying
  tool. Use angreal.
- Test taxonomy: when to run which suite (unit for parser/transform
  changes, functional for SQL semantics, TCK before merging anything in
  transform/executor, all three before release).
- Ownership conventions in headers (caller-owns vs borrowed pointers).
- "Before push" checklist:
  `angreal test unit && angreal test functional && angreal test tck`.
- Generated files: regeneration discipline per policy chosen above.

## Alternatives Considered

- **Skip the variant collapse; just live with the proliferation.**
  Rejected — the next contributor adding a sixth MERGE variant continues
  the pattern. A canonical signature stops it.
- **Replace manifest header with auto-generated header (build-time
  concatenation).** Rejected — auto-generation adds toolchain complexity
  for a problem better solved by per-file headers, which is the
  conventional C pattern.
- **Defer quick wins as a "good first issue" list.** Rejected — they're
  small enough to bundle here, and `CONTRIBUTING.md` being absent makes
  the "good first issue" pitch harder.

## Implementation Plan

### Phase 1 — Quick wins (parallel-safe, can land in any order)

- **C1**: Delete `.bak` files. One commit.
- **C2**: Decide on generated-files policy. Document the decision and any
  CI check needed.
- **C3**: Write `CONTRIBUTING.md`.
- **C4**: Delete `transform_internal.h` after migrating its (likely zero)
  callers.

### Phase 2 — Per-file headers for `transform_func_*`

- **C5**: Create `transform_func_string.h`, `transform_func_math.h`,
  `transform_func_path.h`, `transform_func_entity.h`,
  `transform_func_aggregate.h`, `transform_func_graph.h`,
  `transform_func_dispatch.h`, `transform_func_list.h` (plus any new
  headers introduced by GQLITE-I-0040). Each contains only the decls for
  its paired `.c` file.
- **C6**: Update every `#include "transform/transform_functions.h"` to
  include the per-file headers it actually needs.
- **C7**: Delete `transform_functions.h`.

### Phase 3 — `execute_*_with_*` variant collapse

- **C8**: Design canonical signature for `execute_merge_clause`. Document
  in commit message.
- **C9**: Migrate every caller of any MERGE variant to the canonical
  signature.
- **C10**: Delete the four deprecated MERGE variants.
- **C11**: Same exercise for `execute_match_*` — design canonical
  signature, migrate callers, delete variants.

## Exit Criteria

- [ ] No `.bak` files under `src/`.
- [ ] `transform_internal.h` deleted.
- [ ] `transform_functions.h` deleted; per-file headers exist for each
      `transform_func_*.c`.
- [ ] Exactly one `execute_merge_clause` symbol in `executor_internal.h`.
- [ ] `execute_match_*` collapsed to a canonical set (one per logical
      operation).
- [ ] `CONTRIBUTING.md` exists at repo root.
- [ ] Generated-files policy documented; CI check (if Option A) in place.
- [ ] `angreal test unit && angreal test functional && angreal test tck`
      clean — zero scenario delta.
- [ ] All 11 child tasks (C1–C11) closed.

## Operational Notes

- Phase 1 quick wins can land in any order, even before Phase 2. They
  unblock contributor onboarding immediately.
- Phase 2 (per-file headers) coordinates with GQLITE-I-0040 Phase 3
  (`transform_func_list.c` split) — easiest to do C5–C7 *after* the split
  lands, so the new `transform_func_temporal/geo/json/typeconv` files get
  their headers in the same pass.
- Phase 3 (variant collapse) is more invasive than it looks. Touch every
  caller. Don't skip the audit step (C8) — picking the wrong canonical
  signature means re-migrating every caller again.
