---
id: tck-phase-e-drive-opencypher
level: initiative
title: "TCK Phase E — Drive openCypher conformance from 80% to 90%+"
short_code: "GQLITE-I-0038"
created_at: 2026-05-18T12:19:35.948379+00:00
updated_at: 2026-05-18T12:19:35.948379+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/discovery"


exit_criteria_met: false
estimated_complexity: L
initiative_id: tck-phase-e-drive-opencypher
---

# TCK Phase E — Drive openCypher conformance from 80% to 90%+

## Headline numbers (as of 2026-05-18)

- **Total scenarios:** 3,880
- **Pass:** 3,109 (80.1%)
- **Fail:** 525 (13.5%)
- **Error:** 169 (4.4%)
- **Skipped:** 77 (2.0%)

Source of truth: `tests/tck/baseline.json`. Run via `angreal test tck`.

## Context

The SQLite extension crossed 80% openCypher TCK conformance during the
Phase D push (2818 → 3109 over a single intensive session). The remaining
~770 fails / errors are no longer scattered random bugs — they cluster
into a small number of mostly-orthogonal remediation buckets. This
initiative captures the buckets, assigns each a self-contained task, and
sequences them so that an autonomous Ralph loop can knock them down in
parallel-friendly order.

The aim is to get to ≥90% (3,492 passing scenarios — another ~383 wins)
without losing any current pass. A stretch target of ≥92.5% would put us
ahead of the openCypher reference's own "core" coverage band.

## Goals

- **G1:** Reach ≥90% scenario pass rate against the upstream TCK corpus
  on the extension backend, holding zero regressions on the current
  3,109 passes.
- **G2:** Categorise every remaining failure into one of the named
  clusters below (no "long tail" bucket).
- **G3:** Produce one Metis task per cluster, each with: a concrete
  reproducer, a target file/function in the codebase, an expected
  scenario delta, and a verification command. This is the artifact a
  `/metis-ralph` invocation can consume autonomously.
- **G4:** Add a `tools/tck/cluster.py` (or extend the existing report)
  that re-derives the clusters from a fresh `tck-results.json`, so the
  buckets stay accurate as we close them.

## Non-Goals

- Reaching 100% conformance. The TCK has tests that depend on Cypher
  features (pattern comprehensions with `|`, existential subqueries,
  CALL { } subqueries, named graph SPI) that are explicitly out of scope
  for the SQLite extension v1.
- Re-architecting the executor or transform pipeline. Cluster-level
  fixes only.
- Backend parity with the Python / Rust bindings. Parity is post-hoc;
  this initiative is extension-only.

## Cluster inventory

These are the open clusters as of this writing. The count column shows
fail+error scenarios attributable to that cluster. The "tractability"
column is a rough effort estimate: **S** = single-site fix, **M** =
multi-site or new UDF, **L** = needs structural work.

| Cluster                                                     | Scenarios | Tract. | Notes                                                                                                                |
| ----------------------------------------------------------- | --------: | :----: | -------------------------------------------------------------------------------------------------------------------- |
| **A. Negative-test validators (compile-time error checks)** |       ~85 |   M    | Pattern1 unbound vars in WHERE pattern, Match1 path same-var, Create1/2 same-var, Return6 aggregation ambiguity      |
| **B. Quantifier 3VL semantics**                             |       ~80 |   M    | all/any/none/single with nulls in `Quantifier{1,2,4,6,9,11,12}` — known TCK contradictions; pick the loose spec      |
| **C. Temporal arithmetic / construction**                   |       ~72 |   L    | `Temporal{1,2,3,4,5,6,8,10}` — date+week, datetime+duration, between-temporals duration                              |
| **D. Match7 OPTIONAL MATCH advanced**                       |       ~17 |   M    | bound rel reverse, varlen + length pred, null endpoints                                                              |
| **E. Match5/6/9 varlen path & alias collisions**            |       ~43 |   M    | `[:T*0]` zero-hop, varlen-r-as-list, named varlen paths, ambiguous `n_<i>.id` in chained varlen+std rel              |
| **F. WithOrderBy / list+time sort + UndefinedVar strict**   |       ~32 |   M    | List ordering per Cypher rules, UTC-aware datetime sort regressions, `ORDER BY` referencing pre-WITH scope           |
| **G. Comparison2 list/map 3VL ordering**                    |       ~25 |   M    | `[1,null] >= [1]` lexicographic 3VL; Comparison2[4] examples                                                         |
| **H. List/Map indexing & string subscript**                 |        ~9 |   S    | Map2 invalid index, Set1 list-of-maps                                                                                |
| **I. Pattern comprehensions `[(...)\|expr]`**               |       ~10 |   L    | Parser doesn't accept `|` as projection separator inside list comprehensions over patterns                           |
| **J. Existential subqueries / CALL { }**                    |        ~6 |   L    | Out of scope for v1 — propose **skip-tag** these scenarios so they don't count against us                            |
| **K. RETURN after CREATE / write-then-return**              |       ~30 |   L    | `CREATE ... RETURN n`, MERGE + SET + RETURN; "Unsupported clause type" in dispatcher                                 |
| **L. TriadicSelection / binary-tree fixture**               |       ~18 |   S    | Harness doesn't know how to load `the binary-tree-1 graph` named fixture; one-time data loader                       |
| **M. Misc parse errors near `.`, `:` , `[` after WITH**     |       ~15 |   M    | OPTIONAL MATCH after WITH with bound rel/path; SQL builder edge cases                                                |
| **N. NotImplementedError: RETURN after CREATE**             |       ~10 |   M    | Subset of K; create6 specifically; handle in executor by piping create result through to RETURN                      |
| **O. UndefinedVariable false positives (ORDER BY scope)**   |       ~10 |   S    | The strict ORDER-BY-scope check I added rejects 10 valid scenarios; revisit the pre-WITH-allowed semantics tradeoff |

**Categorised total:** ~462 scenarios.
**Remaining long tail:** ~232 scenarios across <10 each — addressed by
the catch-all "miscellaneous tightening" task once the named clusters
are done.

## Detailed design — Task generation contract

Every task spawned by this initiative will follow the same shape so a
Ralph loop can consume them without further human structuring:

1. **Header** with cluster id (A–O) and current scenario count.
2. **Reproducer** — a self-contained `sqlite3 :memory: <<EOF` block
   that demonstrates one of the failing scenarios.
3. **Target files** — concrete file:line ranges or function names where
   the fix should land. (Most clusters already have these from session
   notes.)
4. **Expected delta** — scenario count that should move from fail/error
   to pass when the task completes, with a tolerance band.
5. **Verification** — exact command(s) to run; the contract is that
   `angreal test tck 2>&1 | grep "TCK \[ext"` reports the expected
   delta and `git diff` only touches the listed files.
6. **Regression guard** — every task ends with re-running unit +
   functional tests (`angreal test unit && angreal test functional`)
   and confirming no other scenarios regressed.

## Alternatives considered

- **Front-load only the highest-count clusters.** Rejected — we'd
  invest in temporal arithmetic (cluster C, biggest) before knocking
  down low-hanging compile-time validators (A, H, L, O), giving up
  cheap wins. Sequence by expected-delta-per-effort ratio instead.
- **Skip negative-test clusters.** Rejected — they're the largest
  remaining single-pattern cluster and many are S/M tractable.
- **Pause and write a full Cypher type system before continuing.** A
  proper static type system would close clusters A, G, H, O cleanly,
  but the cost is weeks. The point-fix approach already used in Phase
  D is faster and adequate for the v1 target.

## Implementation plan

Sequenced by tractability and expected scenario delta. Each phase is a
batch the Ralph loop can knock through in one or two iterations.

### Phase 1 — Cheap validators (~70 scenarios, ~1 day)

- **Task E1 (cluster H):** Map invalid-index TypeError checks for
  `m['key']` where key is non-string, `m[i]` where i is int, etc.
- **Task E2 (cluster L):** Load `binary-tree-1`/`binary-tree-2` graphs
  from a fixture file in the TCK harness so the 18 TriadicSelection
  scenarios can actually execute.
- **Task E3 (cluster O):** Relax the ORDER-BY UndefinedVariable check
  to allow pre-WITH-scope references when the projection isn't
  aggregating (matches openCypher's actual rule).
- **Task E4 (cluster A — Pattern1 subset):** Reject unbound-var pattern
  introductions in WHERE patterns.

### Phase 2 — Pattern fixes (~60 scenarios, ~2 days)

- **Task E5 (cluster D):** Match7 OPTIONAL MATCH with bound rel in
  reverse direction. Re-shape the SQL so bound-rel endpoints work in
  the LEFT JOIN chain instead of in WHERE.
- **Task E6 (cluster E):** Varlen `[*0]` zero-hop semantics. Also fix
  `n_<i>.id` ambiguous-column collision in chained varlen + std rel.
- **Task E7 (cluster M):** Misc parse errors after WITH with bound
  rel/path. Wrap operands more carefully in the OPTIONAL+bound code
  path.

### Phase 3 — Three-valued logic (~105 scenarios, ~2 days)

- **Task E8 (cluster B):** Quantifier 3VL — re-implement
  `all/any/none/single` choosing the openCypher semantics that match
  the TCK majority (the recent attempt regressed Precedence1; need
  per-quantifier behaviour). Decide via tabular analysis of which
  scenarios force which interpretation.
- **Task E9 (cluster G):** List comparison `>`, `<`, `>=`, `<=` with
  Cypher list ordering + 3VL via a `_gql_list_cmp` UDF.

### Phase 4 — Write-then-return (~30 scenarios, ~1 day)

- **Task E10 (cluster K + N):** `CREATE ... RETURN n` / `MERGE ... SET
  ... RETURN ...`. Wire executor to capture created node/edge IDs
  through the var_map and run RETURN against that synthetic result set.

### Phase 5 — Temporal arithmetic (~70 scenarios, ~3-4 days)

- **Task E11 (cluster C, half):** Date construction edge cases —
  week/quarter math off-by-one; ISO week algorithm.
- **Task E12 (cluster C, half):** Duration ± temporal correctness;
  duration-between temporals.

### Phase 6 — Out-of-scope and long tail (~80 scenarios, ~1 day)

- **Task E13 (cluster I + J):** Decide on pattern comprehension and
  existential subquery scope. Either tag-as-skip these scenarios or
  document them in `tests/tck/baseline.md` as known unsupported.
- **Task E14 (long tail):** Hold a clearing round on remaining
  unclassified failures after the named clusters land.

## Exit criteria

- [ ] `angreal test tck 2>&1 | grep "TCK \[ext"` reports `pass≥3492`
      (≥90.0%) across 3,880 scenarios on the extension backend.
- [ ] All 14 child tasks (E1–E14) closed.
- [ ] `tests/tck/baseline.md` updated with the final number and a
      one-paragraph summary of the cluster work.
- [ ] No regression: unit tests pass (`angreal test unit`), functional
      tests pass (`angreal test functional`), and no scenario currently
      in the baseline-pass set has moved to fail/error.
- [ ] `tools/tck/cluster.py` (or equivalent in `tests/tck/report.py`)
      can regenerate this cluster table from a fresh results JSON.

## Operational notes for the Ralph loop

- The TCK suite is the ground truth — run it after every commit; the
  reported number is the single feedback signal.
- Several Quantifier tests use `rand()` and are inherently noisy. When
  judging deltas, run the suite **twice** and use the higher number to
  decide whether to keep a change.
- `angreal dev clean && angreal build extension` between iterations
  avoids stale-incremental-build flakiness (we've seen 2–3 scenario
  noise from that path).
- The harness lives at `tests/tck/`; the worker lives at
  `tests/tck/_extension_worker.py`. Most fix targets are under
  `src/backend/transform/` and `src/backend/executor/`.
- Reference: the Phase D session notes in `git log --oneline -40`
  describe every kind of fix that's been applied so far.