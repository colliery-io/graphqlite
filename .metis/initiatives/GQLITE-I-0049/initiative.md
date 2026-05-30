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
