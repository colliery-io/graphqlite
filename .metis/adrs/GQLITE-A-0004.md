---
id: 001-complexity-based-gpu-threshold
level: adr
title: "Complexity-Based GPU Threshold Configuration"
number: 1
short_code: "GQLITE-A-0004"
created_at: 2026-01-08T14:35:13.811843+00:00
updated_at: 2026-01-08T14:35:13.811843+00:00
decision_date: 
decision_maker: 
parent: 
archived: false

tags:
  - "#adr"
  - "#phase/draft"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# ADR-4: Complexity-Based GPU Threshold Configuration

## Context

GPU acceleration has overhead (data transfer, kernel launch). For small graphs, CPU execution is faster. We need a mechanism to decide when to use GPU vs CPU for each algorithm.

The naive approach—raw node/edge count thresholds—has problems:
- "50K nodes" means vastly different workloads for sparse vs dense graphs
- Different algorithms have different complexity profiles
- Users must guess appropriate values without understanding implications
- Thresholds don't scale with graph characteristics

Graph algorithm users typically understand Big-O complexity. We can leverage this.

## Decision

**Express GPU thresholds in terms of algorithmic complexity (O-notation). Users configure a "computational cost" threshold; the system calculates actual cost from graph parameters and algorithm complexity.**

### Configuration API

```sql
-- Set threshold as "operations" based on algorithm's O complexity
SELECT graphqlite_config('gpu_threshold.pagerank', 1000000);

-- System internally computes: cost = iterations * (V + E)
-- GPU used when: cost > threshold
```

### Algorithm Complexity Mapping

| Algorithm | Complexity | Cost Formula |
|-----------|------------|--------------|
| PageRank | O(k(V + E)) | `iterations * (V + E)` |
| Label Propagation | O(k(V + E)) | `iterations * (V + E)` |
| Connected Components | O(V + E) | `V + E` |
| Betweenness | O(V × E) | `V * E` |
| Closeness | O(V × (V + E)) | `V * (V + E)` |
| APSP | O(V² log V + VE) | `V*V*log2(V) + V*E` |

### Decision Logic

```
Input: algorithm, V, E, [iterations]
Lookup: complexity formula for algorithm
Compute: cost = f(V, E, iterations)
Compare: cost > threshold ? GPU : CPU
```

### Introspection Functions

```sql
-- Query algorithm's complexity formula
SELECT graphqlite_complexity('pagerank');
-- Returns: "O(k(V + E))"

-- Preview what the system would decide
SELECT graphqlite_gpu_decision('pagerank', 100000, 500000, 20);
-- Returns: { "cost": 12000000, "threshold": 1000000, "use_gpu": true }
```

## Alternatives Analysis

| Option | Pros | Cons | Risk Level | Implementation Cost |
|--------|------|------|------------|-------------------|
| **O-notation thresholds** | Semantically meaningful; accounts for graph density; consistent across algorithms | Requires complexity mapping per algorithm; slightly more complex to explain | Low | Low |
| Raw node count | Simple to understand | Ignores edge count; same threshold inappropriate across algorithms | Medium | Low |
| Raw edge count | Captures density | Ignores node count; inappropriate for node-centric algorithms | Medium | Low |
| Node × Edge product | Accounts for both | Overcounts for linear algorithms; undercounts for quadratic | Medium | Low |
| Auto-detection only | No user configuration needed | Black box; no tuning for specific workloads | Low | Medium |

## Rationale

1. **Semantic meaning**: A threshold of "1M operations" means the same computational effort across algorithms, regardless of whether it's PageRank on a sparse graph or betweenness on a dense one.

2. **Accounts for graph shape**: Sparse graphs (low E/V ratio) naturally compute lower costs than dense graphs at the same node count. The formula captures this.

3. **Algorithm-appropriate**: PageRank's O(k(V+E)) threshold behaves differently from Betweenness's O(V×E). Each gets tuned independently.

4. **Expert-friendly**: Users who understand algorithmic complexity can reason about thresholds. "GPU kicks in at 10M operations" is meaningful.

5. **Tunable edge cases**: Small graphs with high iterations (e.g., 10K nodes, 1000 iterations) might warrant GPU. The cost formula captures this: `1000 * (10000 + 50000) = 60M`.

6. **Reasonable defaults**: We can profile and ship sensible defaults per algorithm. Users only tune if they have specific workloads.

## Consequences

### Positive
- Thresholds are semantically meaningful ("operations" not arbitrary counts)
- Same threshold value represents equivalent computational effort across algorithms
- Naturally handles varying graph density
- Algorithm-specific tuning without per-algorithm mental model
- Introspection functions aid debugging and tuning
- Enables informed decisions for edge cases (small graph, many iterations)

### Negative
- Slightly more complex mental model than raw counts
- Requires maintaining complexity formula per algorithm
- Cost calculation adds minor overhead (negligible vs algorithm execution)
- New algorithms must define their complexity formula

### Neutral
- Users can still think in terms of "graph size" for simple cases
- Defaults handle most users; power users get fine-grained control

## Review Triggers

- If profiling reveals complexity formulas poorly predict actual GPU benefit
- If users consistently misconfigure thresholds despite documentation
- If new algorithm classes require complexity patterns we haven't considered