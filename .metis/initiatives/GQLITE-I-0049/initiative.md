---
id: tck-deep-item-conformance-temporal
level: initiative
title: "TCK deep-item conformance (Temporal, Quantifier, Merge, ordering)"
short_code: "GQLITE-I-0049"
created_at: 2026-05-30T14:17:28.737710+00:00
updated_at: 2026-05-30T14:17:28.737710+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/discovery"


exit_criteria_met: false
estimated_complexity: L
initiative_id: tck-deep-item-conformance-temporal
---

# TCK deep-item conformance (Temporal, Quantifier, Merge, ordering)

## Context

After the +44-scenario TCK push (PRs #76–#87, main at **3728 pass / 148 non-pass**
as of 2026-05-30), the remaining gaps are no longer one-line fixes — they cluster
into a handful of deep, multi-scenario features. This initiative tracks those
clusters, one task each, so each can be decomposed and ground out independently
while progress survives compaction.

Baseline: `angreal test tck` → pass=3728 fail=123 error=25 skipped=4.
Verification gate for every task: rigorous full pass-set diff (zero regressions),
unit 944/944, functional clean. Work stacks on one branch per the PR-batching
preference; squash-merge per cluster or per batch as the user directs.

## Goals & Non-Goals

**Goals:**
- Close the large TCK clusters: Temporal (~42), Quantifier (~30), Merge (~14),
  ordering/WithOrderBy (~10), with zero regressions.
- Decompose each cluster into a task with its own root-cause analysis + sub-steps.

**Non-Goals:**
- Boolean type preservation (architecturally blocked — see memory
  `boolean_type_preservation_blocked`; SQLite has no bool type / can't bind subtypes).
- Full rewrites of the value-representation layer.

## Cluster inventory (task-per-cluster)

| Task | Cluster | ~Count | Root cause | Difficulty |
|------|---------|--------|------------|------------|
| T-A | Temporal | ~42 | duration arithmetic, duration.between, date selection, parsing, accessors, named TZ | Medium, decomposable |
| T-B | Quantifier | ~30 | all/any/none/single through the WITH list pipeline | Medium (one root cause may cascade) |
| T-C | Merge | ~14 | bind path, bound-var reuse, direction, multi-row MATCH+MERGE cartesian iteration | Medium-hard |
| T-D | WithOrderBy/ordering | ~10 | ORDER BY stability/tie-break across types (refines GQLITE-T-0340 comparator) | Low-medium |
| T-E | Existential subqueries (2/3) | 5 | full/nested EXISTS { full-query / aggregation / nesting } | Hard |
| T-F | Write-path list/UNWIND (List12, Pattern2) | 7 | entity-list through SET / pattern-comprehension write path | Hard |
| T-G | Misc contained singletons | ~6 | smallest-int literal (Literals2/3/4 [8]), varlen path-length (Path3 [1], Path1), runtime TypeErrors (TypeConversion3) | Mixed |

## Recommended sequence

1. **Temporal (T-A)** — biggest, cleanly decomposable, steady high-yield. **STARTING HERE.**
2. **Quantifier (T-B)** — best count-to-effort; existing analysis in memory `quantifier_equality_dropped`.
3. **Merge (T-C)** — multi-row MATCH+MERGE iteration unlocks several at once.
4. **WithOrderBy (T-D)** — small comparator refinement.

## Detailed Design

Per-cluster design lives in each child task. The shared method: (a) enumerate the
cluster's failing scenarios + diagnostics, (b) find the common root cause(s),
(c) fix smallest-first, (d) rigorous pass-set diff after each, (e) record findings
in the task as working memory.

## Implementation Plan

Phase per cluster, in the sequence above. Each cluster task is decomposed into
sub-steps once started (human check-in before decomposing, per Metis HITL).

## Status

- 2026-05-30: Initiative created. Structure approved (initiative + task-per-cluster).
  Starting with Temporal (T-A).
- 2026-06-02: **WithOrderBy (T-D) cluster CLOSED (+10, PR #91).** `WithOrderBy1`
  [45] (all 10 type examples). Two root causes: (1) a non-aggregating WITH's
  ORDER BY over a *kept* input variable was only applied on the outer SELECT, so a
  downstream aggregating WITH (`collect`) never saw sorted rows — now also pushed
  into the CTE body via `order_expr_all_live_input` gate in `transform_with.c`
  (SQLite preserves an ordered subquery's order through `json_group_array`);
  (2) `gql_order_cmp_func`'s temporal compare used `parse_temporal_ns` whose
  `epoch*1e9` overflows int64 ~year 2262, so year-9999 datetimes sorted *smallest*
  — new overflow-safe `cmp_temporal_strings()` compares `(epoch_seconds, sub_ns)`.
  Verified zero regressions, 3776→3786, unit 944/944, functional clean.
- 2026-06-02: **Merge (T-C) — deep architectural blockers identified, deferred.**
  Investigated the undirected MATCH+MERGE+RETURN cluster (Merge5 [12]/[13]).
  Root cause of row-doubling: the RETURN-after-MERGE re-query builds a synthetic
  combined MATCH (original MATCH pattern + MERGE pattern). For undirected
  `(a)-[r]-(b)` whose endpoints reuse MATCH variables, the transform re-emits the
  shared endpoints into a *reset* FROM and drops the MATCH nodes' property
  constraints, so the undirected OR matches each edge twice. A property-transplant
  fix (copy MATCH node props onto MERGE endpoints, drop redundant standalone nodes)
  was prototyped but blocked by a *second* transform bug: a fresh-fresh undirected
  connected pattern in the re-query drops endpoint property joins entirely (the
  parsed-equivalent query emits them fine — the re-query path resets the builder
  mid-emission). Reverted (no commit). Additional Merge blockers: (a) MERGE
  processes only the FIRST match row (`break` in `executor_merge.c` ~line 1088), so
  multi-row MATCH+MERGE can't bind/return per-row (Merge8 [1] expects +3 rels);
  (b) `handle_create_return` is a hand-rolled projection that doesn't handle
  aggregate RETURN items (`count(*)`/`count(a)` → `?column?`/null) — gates
  Merge5 [4], Merge9 [3]. These need transform/executor rework, not a contained
  fix; left for a dedicated effort.
- 2026-06-02: Comparison2 [3] confirmed blocked by boolean-type-preservation
  (`WHERE <bool-var>` and `WHERE b = true` both filter all rows). List12 [1] is the
  entity-in-list property access deep item (`x.name` on collected nodes → null).
