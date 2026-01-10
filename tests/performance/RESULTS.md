# GraphQLite Performance Results

**Date:** 2026-01-09
**System:** Apple M1 Max, 64GB RAM
**Version:** v0.2.1

## Full Performance Suite

```

GraphQLite Performance Tests
============================
  Mode: full (10K, 100K, 500K, 1M nodes)

  Running tests... 1/72  Running tests... 2/72  Running tests... 3/72  Running tests... 4/72  Running tests... 5/72  Running tests... 6/72  Running tests... 7/72  Running tests... 8/72  Running tests... 9/72  Running tests... 10/72  Running tests... 11/72  Running tests... 12/72  Running tests... 13/72  Running tests... 14/72  Running tests... 15/72  Running tests... 16/72  Running tests... 17/72  Running tests... 18/72  Running tests... 19/72  Running tests... 20/72  Running tests... 21/72  Running tests... 22/72  Running tests... 23/72  Running tests... 24/72  Running tests... 25/72  Running tests... 26/72  Running tests... 27/72  Running tests... 28/72  Running tests... 29/72  Running tests... 30/72  Running tests... 31/72  Running tests... 32/72  Running tests... 33/72  Running tests... 34/72  Running tests... 35/72  Running tests... 36/72  Running tests... 37/72  Running tests... 38/72  Running tests... 39/72  Running tests... 40/72  Running tests... 41/72  Running tests... 42/72  Running tests... 43/72  Running tests... 44/72  Running tests... 45/72  Running tests... 46/72  Running tests... 47/72  Running tests... 48/72  Running tests... 49/72  Running tests... 50/72  Running tests... 51/72  Running tests... 52/72  Running tests... 53/72  Running tests... 54/72  Running tests... 55/72  Running tests... 56/72  Running tests... 57/72  Running tests... 58/72  Running tests... 59/72  Running tests... 60/72  Running tests... 61/72  Running tests... 62/72  Running tests... 63/72  Running tests... 64/72  Running tests... 65/72  Running tests... 66/72  Running tests... 67/72  Running tests... 68/72  Running tests... 69/72  Running tests... 70/72  Running tests... 71/72  Running tests... 72/72  Running tests... done!          

┌────────────┬────────────┬─────────┬──────────┬──────────┬──────────┐
│ Category   │ Test       │ Nodes   │ Edges    │ Time     │ Extra    │
├────────────┼────────────┼─────────┼──────────┼──────────┼──────────┤
│ Insert     │ Bulk load  │     10K │      50K │    103ms │   582K/s │
├────────────┼────────────┼─────────┼──────────┼──────────┼──────────┤
│ Topology   │ chain      │     10K │       9K │      0ms │      0ms │
│ Topology   │ tree       │     10K │       9K │      0ms │      0ms │
│ Topology   │ sparse     │     10K │      50K │      0ms │      0ms │
│ Topology   │ moderate   │     10K │     200K │      0ms │      2ms │
│ Topology   │ dense      │     10K │     500K │      0ms │      9ms │
│ Topology   │ bipartite  │     10K │      50K │      0ms │      0ms │
│ Topology   │ normal     │     10K │      95K │      0ms │      1ms │
│ Topology   │ powerlaw   │     10K │      24K │      0ms │      0ms │
├────────────┼────────────┼─────────┼──────────┼──────────┼──────────┤
│ Algorithm  │ PageRank   │     10K │      50K │     14ms │        - │
│ Algorithm  │ LabelProp  │     10K │      50K │     14ms │        - │
│ Algorithm  │ Aggregates │     10K │      50K │     47ms │        - │
├────────────┼────────────┼─────────┼──────────┼──────────┼──────────┤
│ Query      │ Lookup     │     10K │      50K │      0ms │        - │
│ Query      │ 1-hop      │     10K │      50K │      0ms │        - │
│ Query      │ 2-hop      │     10K │      50K │      0ms │        - │
│ Query      │ 3-hop      │     10K │      50K │      1ms │        - │
│ Query      │ Filter     │     10K │      50K │     30ms │        - │
│ Query      │ MATCH all  │     10K │      50K │     29ms │        - │
├────────────┼────────────┼─────────┼──────────┼──────────┼──────────┤
│ Insert     │ Bulk load  │    100K │     500K │    528ms │   1.1M/s │
├────────────┼────────────┼─────────┼──────────┼──────────┼──────────┤
│ Topology   │ chain      │    100K │      99K │      0ms │      0ms │
│ Topology   │ tree       │    100K │      99K │      0ms │      0ms │
│ Topology   │ sparse     │    100K │     500K │      0ms │      0ms │
│ Topology   │ moderate   │    100K │     2.0M │      0ms │      2ms │
│ Topology   │ dense      │    100K │     5.0M │      0ms │      9ms │
│ Topology   │ bipartite  │    100K │     500K │      0ms │      0ms │
│ Topology   │ normal     │    100K │     959K │      0ms │      1ms │
│ Topology   │ powerlaw   │    100K │     242K │      0ms │      0ms │
├────────────┼────────────┼─────────┼──────────┼──────────┼──────────┤
│ Algorithm  │ PageRank   │    100K │     500K │    160ms │        - │
│ Algorithm  │ LabelProp  │    100K │     500K │    155ms │        - │
│ Algorithm  │ Aggregates │    100K │     500K │    512ms │        - │
├────────────┼────────────┼─────────┼──────────┼──────────┼──────────┤
│ Query      │ Lookup     │    100K │     500K │      0ms │        - │
│ Query      │ 1-hop      │    100K │     500K │      0ms │        - │
│ Query      │ 2-hop      │    100K │     500K │      0ms │        - │
│ Query      │ 3-hop      │    100K │     500K │      1ms │        - │
│ Query      │ Filter     │    100K │     500K │    347ms │        - │
│ Query      │ MATCH all  │    100K │     500K │    331ms │        - │
├────────────┼────────────┼─────────┼──────────┼──────────┼──────────┤
│ Insert     │ Bulk load  │    500K │     2.5M │    2.45s │   1.2M/s │
├────────────┼────────────┼─────────┼──────────┼──────────┼──────────┤
│ Topology   │ chain      │    500K │     499K │      0ms │      0ms │
│ Topology   │ tree       │    500K │     499K │      0ms │      0ms │
│ Topology   │ sparse     │    500K │     2.5M │      1ms │      1ms │
│ Topology   │ moderate   │    500K │    10.0M │      0ms │      2ms │
│ Topology   │ bipartite  │    500K │     2.5M │      0ms │      0ms │
│ Topology   │ normal     │    500K │     4.7M │      0ms │      0ms │
│ Topology   │ powerlaw   │    500K │     1.2M │      0ms │      0ms │
├────────────┼────────────┼─────────┼──────────┼──────────┼──────────┤
│ Algorithm  │ PageRank   │    500K │     2.5M │    891ms │        - │
│ Algorithm  │ LabelProp  │    500K │     2.5M │    860ms │        - │
│ Algorithm  │ Aggregates │    500K │     2.5M │    2.60s │        - │
├────────────┼────────────┼─────────┼──────────┼──────────┼──────────┤
│ Query      │ Lookup     │    500K │     2.5M │      0ms │        - │
│ Query      │ 1-hop      │    500K │     2.5M │      0ms │        - │
│ Query      │ 2-hop      │    500K │     2.5M │      0ms │        - │
│ Query      │ 3-hop      │    500K │     2.5M │      1ms │        - │
│ Query      │ Filter     │    500K │     2.5M │    1.69s │        - │
│ Query      │ MATCH all  │    500K │     2.5M │    1.68s │        - │
├────────────┼────────────┼─────────┼──────────┼──────────┼──────────┤
│ Insert     │ Bulk load  │    1.0M │     5.0M │    4.90s │   1.2M/s │
├────────────┼────────────┼─────────┼──────────┼──────────┼──────────┤
│ Topology   │ chain      │    1.0M │     999K │      0ms │      0ms │
│ Topology   │ tree       │    1.0M │     999K │      0ms │      0ms │
│ Topology   │ sparse     │    1.0M │     5.0M │      0ms │      1ms │
│ Topology   │ moderate   │    1.0M │    20.0M │      1ms │      2ms │
│ Topology   │ bipartite  │    1.0M │     5.0M │      0ms │      0ms │
│ Topology   │ normal     │    1.0M │     9.5M │      0ms │      1ms │
│ Topology   │ powerlaw   │    1.0M │     2.4M │      0ms │      0ms │
├────────────┼────────────┼─────────┼──────────┼──────────┼──────────┤
│ Algorithm  │ PageRank   │    1.0M │     5.0M │   45.07s │        - │
│ Algorithm  │ LabelProp  │    1.0M │     5.0M │   45.12s │        - │
│ Algorithm  │ Aggregates │    1.0M │     5.0M │    5.37s │        - │
├────────────┼────────────┼─────────┼──────────┼──────────┼──────────┤
│ Query      │ Lookup     │    1.0M │     5.0M │      0ms │        - │
│ Query      │ 1-hop      │    1.0M │     5.0M │      0ms │        - │
│ Query      │ 2-hop      │    1.0M │     5.0M │      0ms │        - │
│ Query      │ 3-hop      │    1.0M │     5.0M │      1ms │        - │
│ Query      │ Filter     │    1.0M │     5.0M │    3.53s │        - │
│ Query      │ MATCH all  │    1.0M │     5.0M │    3.42s │        - │
└────────────┴────────────┴─────────┴──────────┴──────────┴──────────┘

  Mode: full | Iterations per query: 3
  Time column shows avg per query for Query/Algorithm tests
  Topology tests show 1-hop time (Time) and 2-hop time (Extra)

```

## Cache Performance Comparison

```

GraphQLite Cache Performance Comparison
========================================

  Mode: full (1K, 10K, 100K, 500K nodes)

Testing graph: 1K nodes, 5K edges...
Testing graph: 10K nodes, 50K edges...
Testing graph: 100K nodes, 500K edges...
Testing graph: 500K nodes, 2.5M edges...

┌─────────┬──────────┬───────────────────────────┬───────────────────────────┬───────────────────────────┐
│         │          │        PageRank           │     Label Propagation     │    Degree Centrality      │
│ Nodes   │ Edges    ├─────────┬─────────┬───────┼─────────┬─────────┬───────┼─────────┬─────────┬───────┤
│         │          │ Uncached│ Cached  │Speedup│ Uncached│ Cached  │Speedup│ Uncached│ Cached  │Speedup│
├─────────┼──────────┼─────────┼─────────┼───────┼─────────┼─────────┼───────┼─────────┼─────────┼───────┤
│      1K │       5K │     1ms │    <1ms │   N/A │     1ms │    <1ms │   N/A │     1ms │    <1ms │   N/A │
│     10K │      50K │    13ms │     6ms │  2.1x │    13ms │     6ms │  2.1x │    14ms │     8ms │  1.7x │
│    100K │     500K │   154ms │    73ms │  2.1x │   151ms │    75ms │  2.0x │   171ms │    96ms │  1.7x │
│    500K │     2.5M │   890ms │   396ms │  2.2x │   896ms │   414ms │  2.1x │   1.02s │   509ms │  2.0x │
└─────────┴──────────┴─────────┴─────────┴───────┴─────────┴─────────┴───────┴─────────┴─────────┴───────┘

  Iterations per measurement: 3
  Speedup = Uncached Time / Cached Time

```

## Key Findings

### Insertion Performance
- **1.1-1.2M nodes+edges/second** bulk loading rate
- Scales linearly with graph size

### Query Performance
- **Sub-millisecond** node lookups and 1-3 hop traversals
- Filter scans scale with node count (~3.5s for 1M nodes)

### Algorithm Performance (Uncached)
| Graph Size | PageRank | Label Prop |
|------------|----------|------------|
| 10K nodes | 14ms | 14ms |
| 100K nodes | 160ms | 155ms |
| 500K nodes | 891ms | 860ms |
| 1M nodes | 45s | 45s |

### Cache Speedup
| Graph Size | PageRank | Label Prop | Degree Cent |
|------------|----------|------------|-------------|
| 10K nodes | **2.1x** | **2.1x** | **1.7x** |
| 100K nodes | **2.1x** | **2.0x** | **1.7x** |
| 500K nodes | **2.2x** | **2.1x** | **2.0x** |

**Recommendation:** Use `gql_load_graph()` when running multiple algorithms on the same graph for ~2x speedup.

### Hop Depth Traversal

Traversal time is **independent of graph size** - it scales only with the number of paths found.

| Hops | Paths Found | Time (any graph size) |
|------|-------------|----------------------|
| 1 | 5 | <1ms |
| 2 | 25 | <1ms |
| 3 | 125 | 1ms |
| 4 | 625 | 2ms |
| 5 | 3,125 | 12ms |
| 6 | 15,625 | 58ms |

Path count grows as `degree^hops` (5^n with avg degree 5). Time scales linearly with path count.

```
┌─────────┬──────────┬────────────────┬────────────────┬────────────────┬────────────────┬────────────────┬────────────────┐
│         │          │     1-hop      │     2-hop      │     3-hop      │     4-hop      │     5-hop      │     6-hop      │
│ Nodes   │ Edges    ├────────┬───────┼────────┬───────┼────────┬───────┼────────┬───────┼────────┬───────┼────────┬───────┤
│         │          │  Time  │ Count │  Time  │ Count │  Time  │ Count │  Time  │ Count │  Time  │ Count │  Time  │ Count │
├─────────┼──────────┼────────┼───────┼────────┼───────┼────────┼───────┼────────┼───────┼────────┼───────┼────────┼───────┤
│     10K │      50K │   1ms │    5 │  <1ms │   25 │   1ms │  125 │   2ms │  625 │  11ms │   3K │  55ms │  15K │
│    100K │     500K │   1ms │    5 │  <1ms │   25 │   1ms │  125 │   2ms │  625 │  12ms │   3K │  58ms │  15K │
│    500K │     2.5M │   1ms │    5 │  <1ms │   25 │   1ms │  125 │   2ms │  625 │  12ms │   3K │  60ms │  15K │
└─────────┴──────────┴────────┴───────┴────────┴───────┴────────┴───────┴────────┴───────┴────────┴───────┴────────┴───────┘
```
