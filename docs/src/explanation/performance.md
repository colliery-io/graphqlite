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

### Algorithm Caching

Graph algorithms scan the entire graph. If your graph doesn't change frequently, cache results:

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
make performance
```

This runs:
- Insertion benchmarks
- Traversal benchmarks across topologies
- Algorithm benchmarks
- Query benchmarks
