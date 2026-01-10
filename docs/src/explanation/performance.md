# Performance

This document covers GraphQLite's performance characteristics and optimization strategies.

## Benchmarks

Benchmarks on Apple M1 Max (10 cores, 64GB RAM).

### Insertion Performance

| Nodes | Edges | Time | Rate |
|-------|-------|------|------|
| 100K | 500K | 445ms | 1.3M/s |
| 500K | 2.5M | 2.30s | 1.3M/s |
| 1M | 5.0M | 5.16s | 1.1M/s |

### Traversal by Topology

| Topology | Nodes | Edges | 1-hop | 2-hop |
|----------|-------|-------|-------|-------|
| Chain | 100K | 99K | <1ms | <1ms |
| Sparse | 100K | 500K | <1ms | <1ms |
| Moderate | 100K | 2.0M | <1ms | 2ms |
| Dense | 100K | 5.0M | <1ms | 9ms |
| Normal dist. | 100K | 957K | <1ms | 1ms |
| Power-law | 100K | 242K | <1ms | <1ms |
| Moderate | 500K | 10.0M | 1ms | 2ms |
| Moderate | 1M | 20.0M | <1ms | 2ms |

### Deep Hop Traversal

Traversal time is **independent of graph size** - it scales only with the number of paths found.

| Hops | Paths Found | Time |
|------|-------------|------|
| 1-3 | 5-125 | <1ms |
| 4 | 625 | 2ms |
| 5 | 3,125 | 12ms |
| 6 | 15,625 | 58ms |

Path count grows as `degree^hops`. With average degree 5, expect 5^n paths at n hops.

### Graph Algorithms

| Algorithm | Nodes | Edges | Time |
|-----------|-------|-------|------|
| PageRank | 100K | 500K | 148ms |
| Label Propagation | 100K | 500K | 154ms |
| PageRank | 500K | 2.5M | 953ms |
| Label Propagation | 500K | 2.5M | 811ms |
| PageRank | 1M | 5.0M | 37.81s |
| Label Propagation | 1M | 5.0M | 40.21s |

### Cypher Query Performance

| Query Type | G(100K, 500K) | G(500K, 2.5M) | G(1M, 5M) |
|------------|---------------|---------------|-----------|
| Node lookup | <1ms | 1ms | <1ms |
| 1-hop | <1ms | <1ms | <1ms |
| 2-hop | <1ms | <1ms | <1ms |
| 3-hop | 1ms | 1ms | 1ms |
| Filter scan | 341ms | 1.98s | 3.79s |
| MATCH all | 360ms | 2.05s | 3.98s |

## Optimization Strategies

### Use Indexes Effectively

GraphQLite creates indexes on:
- `nodes(user_id)` - Fast node lookup by ID
- `nodes(label)` - Fast filtering by label
- `edges(source_id)`, `edges(target_id)` - Fast traversal
- Property tables on `(node_id, key)` - Fast property access

Queries that leverage these indexes are fast.

### Limit Variable-Length Paths

Variable-length paths can be expensive:

```cypher
-- Expensive: unlimited depth
MATCH (a)-[*]->(b) RETURN b

-- Better: limit depth
MATCH (a)-[*1..3]->(b) RETURN b
```

### Use Specific Labels

Labels help filter early:

```cypher
-- Slower: scan all nodes
MATCH (n) WHERE n.type = 'Person' RETURN n

-- Faster: use label
MATCH (n:Person) RETURN n
```

### Batch Operations

For bulk inserts, use batch methods:

```python
# Slow: individual inserts
for person in people:
    g.upsert_node(person["id"], person, label="Person")

# Fast: batch insert
nodes = [(p["id"], p, "Person") for p in people]
g.upsert_nodes_batch(nodes)
```

### Graph Caching

GraphQLite can cache the graph structure in memory using a Compressed Sparse Row (CSR) format, providing **1.5-2x speedup** for graph algorithms by eliminating repeated SQLite I/O.

#### SQL Interface

```sql
-- Load graph into memory cache
SELECT gql_load_graph();
-- Returns: {"status":"loaded","nodes":1000,"edges":5000}

-- Check if cache is loaded
SELECT gql_graph_loaded();
-- Returns: {"loaded":true,"nodes":1000,"edges":5000}

-- Reload cache after graph modifications
SELECT gql_reload_graph();

-- Free cache memory
SELECT gql_unload_graph();
```

#### Python Interface

```python
from graphqlite import graph

g = graph(":memory:")
# ... build graph ...

# Load cache for fast algorithm execution
g.load_graph()  # {"status": "loaded", "nodes": 1000, "edges": 5000}

# Run algorithms (all use cached graph)
g.pagerank()
g.community_detection()
g.degree_centrality()

# After modifying graph, reload cache
g.upsert_node("new_node", {}, "Person")
g.reload_graph()

# Free memory when done
g.unload_graph()
```

#### Rust Interface

```rust
use graphqlite::Graph;

let g = Graph::open_in_memory()?;
// ... build graph ...

// Load cache
let status = g.load_graph()?;
println!("Loaded {} nodes", status.nodes.unwrap_or(0));

// Run algorithms with cache
// ... algorithms use cache automatically ...

// Check status
if g.graph_loaded()? {
    g.unload_graph()?;
}
```

#### Cache Performance

Benchmarks on Apple M1 Max with graph caching enabled:

| Nodes | Edges | Algorithm | Uncached | Cached | Speedup |
|-------|-------|-----------|----------|--------|---------|
| 10K | 50K | PageRank | 13ms | 7ms | **1.8x** |
| 10K | 50K | Label Prop | 13ms | 7ms | **1.8x** |
| 100K | 500K | PageRank | 151ms | 91ms | **1.6x** |
| 100K | 500K | Label Prop | 151ms | 87ms | **1.7x** |
| 500K | 2.5M | PageRank | 858ms | 420ms | **2.0x** |
| 500K | 2.5M | Label Prop | 863ms | 412ms | **2.0x** |

**When to use caching:**
- Running multiple algorithms on the same graph
- Repeated analysis workflows
- Interactive exploration where graph doesn't change

**When NOT to use caching:**
- Single algorithm call (cache load overhead may exceed benefit)
- Frequently modified graphs (requires reload after each change)
- Memory-constrained environments

### Result Caching

For application-level caching of algorithm results:

```python
import functools

@functools.lru_cache(maxsize=1)
def get_pagerank():
    return g.pagerank()
```

## Memory Usage

GraphQLite uses SQLite's memory management. Key factors:

- **Page cache**: SQLite caches database pages in memory
- **Algorithm scratch space**: Algorithms allocate temporary structures
- **Result buffers**: Query results are buffered before returning

For large graphs, consider:

```python
# Increase SQLite page cache (default: 2MB)
conn.execute("PRAGMA cache_size = -64000")  # 64MB
```

## Running Benchmarks

Run benchmarks on your hardware:

```bash
# Full performance suite
./tests/performance/run_all_perf.sh full

# Cache comparison benchmark
./tests/performance/perf_cache_comparison.sh full

# Quick cache test
sqlite3 :memory: < tests/performance/perf_cache.sql
```

Available benchmark modes:
- `quick` - Fast smoke test (~30s)
- `standard` - Default benchmarks (~3min)
- `full` - Comprehensive benchmarks (~10min)

Benchmarks cover:
- Insertion performance
- Traversal across topologies (chain, tree, sparse, dense, power-law)
- Algorithm performance (PageRank, Label Propagation, etc.)
- Query performance (lookup, hop traversals, filters)
- Cache performance (uncached vs cached algorithms)
