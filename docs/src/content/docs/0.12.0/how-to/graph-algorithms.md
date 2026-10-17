---
title: Using Graph Algorithms
slug: 0.12.0/how-to/graph-algorithms
---

GraphQLite includes 18 built-in graph algorithms covering centrality, community detection, path finding, connectivity, traversal, and similarity. All algorithms run inside the SQLite process — no external graph engine is required.

## Setup

All examples in this guide use the same sample graph:

```python
from graphqlite import Graph

g = Graph(":memory:")

# Nodes
for name, role in [
    ("alice", "engineer"), ("bob", "manager"), ("carol", "engineer"),
    ("dave", "analyst"), ("eve", "engineer"), ("frank", "manager"),
]:
    g.upsert_node(name, {"name": name.capitalize(), "role": role}, "Person")

# Edges
for src, dst, weight in [
    ("alice", "bob", 1), ("alice", "carol", 2), ("bob", "dave", 1),
    ("carol", "dave", 3), ("dave", "eve", 1), ("eve", "frank", 2),
    ("frank", "alice", 4), ("bob", "eve", 1),
]:
    g.upsert_edge(src, dst, {"weight": weight}, "KNOWS")
```

## Graph Cache Management

Before running algorithms on large graphs, load the graph into the in-memory cache. This avoids redundant disk reads and speeds up repeated algorithm calls significantly.

### Python

```python
# Load into cache
g.load_graph()

# Check whether the cache is populated
print(g.graph_loaded())  # True

# Run algorithms — all use the cached graph
pr = g.pagerank()
cc = g.community_detection()

# Reload cache after data changes
g.upsert_node("grace", {"name": "Grace"}, "Person")
g.reload_graph()

# Free memory when done
g.unload_graph()
print(g.graph_loaded())  # False
```

### SQL

```sql
-- Load cache
SELECT gql_load_graph();

-- Check status
SELECT gql_graph_loaded();  -- 1 or 0

-- Reload after changes
SELECT gql_reload_graph();

-- Unload
SELECT gql_unload_graph();
```

> For small graphs (under ~10 000 nodes) the cache provides modest gains. For larger graphs, always call `load_graph()` before running multiple algorithms.

***

## Centrality Algorithms

Centrality algorithms measure the relative importance of nodes in the graph.

### PageRank

Assigns scores based on the number and quality of incoming edges. Nodes linked from highly-scored nodes receive higher scores.

**When to use:** Ranking nodes by overall influence or authority (web pages, citations, recommendation networks).

**Python:**

```python
results = g.pagerank(damping=0.85, iterations=20)
# [{"node_id": "alice", "score": 0.21}, ...]

top5 = sorted(results, key=lambda r: r["score"], reverse=True)[:5]
for r in top5:
    print(f"{r['node_id']}: {r['score']:.4f}")
```

**Rust:**

```rust
let results = g.pagerank(0.85, 20)?;
for r in &results {
    println!("{}: {:.4}", r.node_id, r.score);
}
```

**SQL:**

```sql
SELECT
    json_extract(value, '$.node_id') AS node,
    json_extract(value, '$.score')   AS score
FROM json_each(cypher('RETURN pageRank(0.85, 20)'), '$[0].column_0')
ORDER BY score DESC
LIMIT 5;
```

**Parameters:** `damping` (default 0.85), `iterations` (default 20).

***

### Degree Centrality

Counts in-degree, out-degree, and total degree for every node.

**When to use:** Quick identification of hubs and leaf nodes; baseline for other analyses.

**Python:**

```python
results = g.degree_centrality()
# [{"node_id": "alice", "in_degree": 1, "out_degree": 2, "degree": 3}, ...]

# Find the highest out-degree node
hub = max(results, key=lambda r: r["out_degree"])
print(f"Top hub: {hub['node_id']} with {hub['out_degree']} outgoing edges")
```

**Rust:**

```rust
let results = g.degree_centrality()?;
for r in &results {
    println!("{}: in={}, out={}, total={}", r.node_id, r.in_degree, r.out_degree, r.degree);
}
```

**SQL:**

```sql
SELECT
    json_extract(value, '$.node_id')    AS node,
    json_extract(value, '$.out_degree') AS out_degree
FROM json_each(cypher('RETURN degreeCentrality()'), '$[0].column_0')
ORDER BY out_degree DESC;
```

***

### Betweenness Centrality

Measures how often a node appears on shortest paths between other node pairs.

**When to use:** Identifying bottlenecks, bridges, or brokers in a network (infrastructure nodes, information brokers).

**Python:**

```python
results = g.betweenness_centrality()
# [{"node_id": "dave", "score": 0.45}, ...]

for r in sorted(results, key=lambda r: r["score"], reverse=True):
    print(f"{r['node_id']}: {r['score']:.4f}")
```

**Rust:**

```rust
let results = g.betweenness_centrality()?;
for r in &results {
    println!("{}: {:.4}", r.node_id, r.score);
}
```

**SQL:**

```sql
SELECT
    json_extract(value, '$.node_id') AS node,
    json_extract(value, '$.score')   AS score
FROM json_each(cypher('RETURN betweennessCentrality()'), '$[0].column_0')
ORDER BY score DESC;
```

> Betweenness is O(n \* m) and can be slow on graphs with hundreds of thousands of edges.

***

### Closeness Centrality

Measures how quickly a node can reach all other nodes (inverse of average shortest path length).

**When to use:** Finding nodes that can spread information fastest, or nodes most central to communication.

**Python:**

```python
results = g.closeness_centrality()
# [{"node_id": "alice", "score": 0.71}, ...]
```

**Rust:**

```rust
let results = g.closeness_centrality()?;
```

**SQL:**

```sql
SELECT
    json_extract(value, '$.node_id') AS node,
    json_extract(value, '$.score')   AS score
FROM json_each(cypher('RETURN closenessCentrality()'), '$[0].column_0')
ORDER BY score DESC;
```

***

### Eigenvector Centrality

Assigns scores iteratively: a node is important if it is connected to other important nodes.

**When to use:** Influence scoring in social networks; pages linked from authoritative sources.

**Python:**

```python
results = g.eigenvector_centrality(iterations=100)
# [{"node_id": "alice", "score": 0.55}, ...]
```

**Rust:**

```rust
let results = g.eigenvector_centrality(100)?;
```

**SQL:**

```sql
SELECT
    json_extract(value, '$.node_id') AS node,
    json_extract(value, '$.score')   AS score
FROM json_each(cypher('RETURN eigenvectorCentrality(100)'), '$[0].column_0')
ORDER BY score DESC;
```

**Parameters:** `iterations` (default 100) — the algorithm stops when scores converge or iterations are exhausted.

***

## Community Detection

Community detection partitions nodes into clusters based on edge density.

### Label Propagation (community_detection)

Nodes iteratively adopt the label most common among their neighbors. Fast and approximate.

**When to use:** Quick community discovery on large graphs; when exact modularity optimization is not required.

**Python:**

```python
results = g.community_detection(iterations=10)
# [{"node_id": "alice", "community": 1}, ...]

# Group nodes by community
from collections import defaultdict
communities = defaultdict(list)
for r in results:
    communities[r["community"]].append(r["node_id"])

for cid, members in communities.items():
    print(f"Community {cid}: {members}")
```

**Rust:**

```rust
let results = g.community_detection(10)?;
for r in &results {
    println!("{} -> community {}", r.node_id, r.community);
}
```

**SQL:**

```sql
SELECT
    json_extract(value, '$.node_id')   AS node,
    json_extract(value, '$.community') AS community
FROM json_each(cypher('RETURN labelPropagation(10)'), '$[0].column_0')
ORDER BY community;
```

***

### Louvain

Hierarchical community detection that optimizes modularity. Produces higher-quality communities than label propagation at the cost of more computation.

**When to use:** When community quality matters more than speed; medium-sized graphs (under ~100 000 nodes).

**Python:**

```python
results = g.louvain(resolution=1.0)
# [{"node_id": "alice", "community": 0}, ...]

# Higher resolution = more, smaller communities
fine_grained = g.louvain(resolution=2.0)
```

**Rust:**

```rust
let results = g.louvain(1.0)?;
```

**SQL:**

```sql
SELECT
    json_extract(value, '$.node_id')   AS node,
    json_extract(value, '$.community') AS community
FROM json_each(cypher('RETURN louvain(1.0)'), '$[0].column_0')
ORDER BY community;
```

**Parameters:** `resolution` (default 1.0) — increase to find more communities; decrease to merge communities.

***

### Leiden Communities

An improved variant of Louvain that guarantees well-connected communities and avoids the resolution limit problem.

**When to use:** When Louvain produces communities that seem internally disconnected, or for publication-quality community detection.

**Requires:** `pip install graspologic` (or `pip install graphqlite[leiden]`).

**Python:**

```python
# Install: pip install graphqlite[leiden]
results = g.leiden_communities(resolution=1.0, random_seed=42)
# [{"node_id": "alice", "community": 0}, ...]
```

Leiden is not available via the Cypher `RETURN` interface; use the Python or Rust Graph API.

***

## Path Finding

### Shortest Path (Dijkstra)

Finds the minimum-weight path between two nodes.

**When to use:** Navigation, network routing, social distance ("degrees of separation").

**Python:**

```python
result = g.shortest_path("alice", "frank")
# {"distance": 3, "path": ["alice", "bob", "dave", "eve", "frank"], "found": True}

if result["found"]:
    print(f"Distance: {result['distance']}")
    print(f"Path: {' -> '.join(result['path'])}")
else:
    print("No path found")

# With weighted edges
result = g.shortest_path("alice", "frank", weight_property="weight")
```

**Rust:**

```rust
let result = g.shortest_path("alice", "frank", None)?;  // None = unweighted
if result.found {
    println!("Distance: {:?}", result.distance);
    println!("Path: {:?}", result.path);
}

// Weighted
let result = g.shortest_path("alice", "frank", Some("weight"))?;
```

**SQL:**

```sql
SELECT cypher('RETURN dijkstra(''alice'', ''frank'')');
```

***

### A\* Search

Shortest path with a heuristic to guide the search. When node latitude/longitude properties are available, uses haversine distance as the heuristic, which can dramatically reduce nodes explored.

**When to use:** Geographic routing, maps, spatial graphs where coordinates are available.

**Python:**

```python
# With geographic coordinates
result = g.astar("city_a", "city_b", lat_prop="latitude", lon_prop="longitude")
# {"found": True, "distance": 412.5, "path": [...], "nodes_explored": 18}

print(f"Explored {result['nodes_explored']} nodes (vs full BFS)")

# Without coordinates (falls back to uniform heuristic)
result = g.astar("alice", "frank")
```

**Rust:**

```rust
let result = g.astar("city_a", "city_b", Some("latitude"), Some("longitude"))?;
println!("Explored {} nodes", result.nodes_explored);
```

**SQL:**

```sql
SELECT cypher('RETURN astar(''city_a'', ''city_b'', ''latitude'', ''longitude'')');
```

***

### All-Pairs Shortest Path

Computes shortest distances between every pair of nodes using Floyd-Warshall.

**When to use:** Building distance matrices, computing graph diameter, small dense graphs.

**Python:**

```python
results = g.all_pairs_shortest_path()
# [{"source": "alice", "target": "carol", "distance": 1.0}, ...]

# Build a distance matrix
import numpy as np
nodes = list({r["source"] for r in results})
n = len(nodes)
idx = {name: i for i, name in enumerate(nodes)}
D = np.full((n, n), float("inf"))
for r in results:
    D[idx[r["source"]], idx[r["target"]]] = r["distance"]

print(f"Graph diameter: {D[D < float('inf')].max()}")
```

**Rust:**

```rust
let results = g.apsp()?;
for r in &results {
    println!("{} -> {}: {}", r.source, r.target, r.distance);
}
```

**SQL:**

```sql
SELECT
    json_extract(value, '$.source')   AS src,
    json_extract(value, '$.target')   AS tgt,
    json_extract(value, '$.distance') AS dist
FROM json_each(cypher('RETURN apsp()'), '$[0].column_0')
ORDER BY dist;
```

> All-pairs shortest path is O(n²) in space and time. Avoid on graphs with more than a few thousand nodes.

***

## Connected Components

### Weakly Connected Components

Groups nodes that are reachable from each other when edge direction is ignored.

**When to use:** Finding isolated subgraphs, checking overall graph connectivity, data quality checks.

**Python:**

```python
results = g.weakly_connected_components()
# [{"node_id": "alice", "component": 0}, ...]

# Count components
components = {r["component"] for r in results}
print(f"Graph has {len(components)} weakly connected component(s)")

# Find isolated nodes (singletons)
from collections import Counter
counts = Counter(r["component"] for r in results)
singletons = [cid for cid, cnt in counts.items() if cnt == 1]
print(f"{len(singletons)} isolated node(s)")
```

**Rust:**

```rust
let results = g.wcc()?;
```

**SQL:**

```sql
SELECT
    json_extract(value, '$.component') AS component,
    COUNT(*) AS size
FROM json_each(cypher('RETURN wcc()'), '$[0].column_0')
GROUP BY component
ORDER BY size DESC;
```

***

### Strongly Connected Components

Groups nodes where every node can reach every other node following edge direction.

**When to use:** Detecting cycles, finding strongly coupled subsystems, directed network analysis.

**Python:**

```python
results = g.strongly_connected_components()
# [{"node_id": "alice", "component": 0}, ...]

from collections import Counter
counts = Counter(r["component"] for r in results)
largest = max(counts, key=counts.get)
print(f"Largest SCC has {counts[largest]} nodes")
```

**Rust:**

```rust
let results = g.scc()?;
```

**SQL:**

```sql
SELECT
    json_extract(value, '$.component') AS component,
    COUNT(*) AS size
FROM json_each(cypher('RETURN scc()'), '$[0].column_0')
GROUP BY component
ORDER BY size DESC;
```

***

## Traversal

### Breadth-First Search (BFS)

Explores nodes level by level outward from a starting node.

**When to use:** Finding all nodes within N hops, shortest-hop paths, social network analysis.

**Python:**

```python
results = g.bfs("alice", max_depth=2)
# [{"user_id": "alice", "depth": 0, "order": 0},
#  {"user_id": "bob",   "depth": 1, "order": 1},
#  {"user_id": "carol", "depth": 1, "order": 2}, ...]

# Print reachable nodes within 2 hops
for r in results:
    print(f"  {'  ' * r['depth']}{r['user_id']} (depth {r['depth']})")
```

**Rust:**

```rust
let results = g.bfs("alice", Some(2))?;
for r in &results {
    println!("depth {}: {}", r.depth, r.user_id);
}
```

**SQL:**

```sql
SELECT
    json_extract(value, '$.user_id') AS node,
    json_extract(value, '$.depth')   AS depth
FROM json_each(cypher('RETURN bfs(''alice'', 2)'), '$[0].column_0')
ORDER BY depth, json_extract(value, '$.order');
```

***

### Depth-First Search (DFS)

Explores as deep as possible along each branch before backtracking.

**When to use:** Cycle detection, topological ordering exploration, tree-like structures.

**Python:**

```python
results = g.dfs("alice", max_depth=3)
# [{"user_id": "alice", "depth": 0, "order": 0}, ...]

for r in sorted(results, key=lambda r: r["order"]):
    indent = "  " * r["depth"]
    print(f"{indent}{r['user_id']}")
```

**Rust:**

```rust
let results = g.dfs("alice", None)?;  // None = unlimited depth
for r in &results {
    println!("order {}: {} (depth {})", r.order, r.user_id, r.depth);
}
```

**SQL:**

```sql
SELECT
    json_extract(value, '$.user_id') AS node,
    json_extract(value, '$.depth')   AS depth,
    json_extract(value, '$.order')   AS visit_order
FROM json_each(cypher('RETURN dfs(''alice'', 5)'), '$[0].column_0')
ORDER BY visit_order;
```

***

## Similarity

### Node Similarity (Jaccard)

Computes Jaccard similarity between the neighborhoods of nodes: `|intersection| / |union|`.

**When to use:** Collaborative filtering, finding structurally similar entities, de-duplication.

**Python:**

```python
# All pairs above a threshold
results = g.node_similarity(threshold=0.3)
# [{"node1": "alice", "node2": "carol", "similarity": 0.5}, ...]

# Between two specific nodes
result = g.node_similarity(node1_id="alice", node2_id="bob")

# Top-10 most similar pairs
results = g.node_similarity(top_k=10)

for r in sorted(results, key=lambda r: r["similarity"], reverse=True):
    print(f"{r['node1']} <-> {r['node2']}: {r['similarity']:.3f}")
```

**Rust:**

```rust
// threshold=0.3, top_k=0 (all pairs)
let results = g.node_similarity(None, None, 0.3, 0)?;
for r in &results {
    println!("{} <-> {}: {:.3}", r.node1, r.node2, r.similarity);
}
```

**SQL:**

```sql
SELECT
    json_extract(value, '$.node1')      AS n1,
    json_extract(value, '$.node2')      AS n2,
    json_extract(value, '$.similarity') AS sim
FROM json_each(cypher('RETURN nodeSimilarity()'), '$[0].column_0')
WHERE json_extract(value, '$.similarity') > 0.3
ORDER BY sim DESC;
```

***

### K-Nearest Neighbors (KNN)

Finds the k most similar nodes to a given node based on Jaccard neighborhood similarity.

**When to use:** Recommendation systems ("users like you also connected to..."), suggestion features.

**Python:**

```python
results = g.knn("alice", k=3)
# [{"neighbor": "carol", "similarity": 0.5, "rank": 1},
#  {"neighbor": "dave",  "similarity": 0.33, "rank": 2}, ...]

print("Alice's most similar neighbors:")
for r in results:
    print(f"  #{r['rank']}: {r['neighbor']} (similarity {r['similarity']:.3f})")
```

**Rust:**

```rust
let results = g.knn("alice", 3)?;
for r in &results {
    println!("#{}: {} ({:.3})", r.rank, r.neighbor, r.similarity);
}
```

**SQL:**

```sql
SELECT
    json_extract(value, '$.neighbor')   AS neighbor,
    json_extract(value, '$.similarity') AS similarity,
    json_extract(value, '$.rank')       AS rank
FROM json_each(cypher('RETURN knn(''alice'', 5)'), '$[0].column_0')
ORDER BY rank;
```

***

### Triangle Count

Counts the number of triangles each node participates in and computes the local clustering coefficient.

**When to use:** Measuring graph density and cliquishness; social network cohesion analysis.

**Python:**

```python
results = g.triangle_count()
# [{"node_id": "alice", "triangles": 2, "clustering_coefficient": 0.67}, ...]

# Nodes with high clustering (tight local clusters)
high_cluster = [r for r in results if r["clustering_coefficient"] > 0.5]
total_triangles = sum(r["triangles"] for r in results) // 3  # each triangle counted 3 times
print(f"Total triangles in graph: {total_triangles}")
```

**Rust:**

```rust
let results = g.triangle_count()?;
for r in &results {
    println!("{}: {} triangles, clustering={:.3}",
        r.node_id, r.triangles, r.clustering_coefficient);
}
```

**SQL:**

```sql
SELECT
    json_extract(value, '$.node_id')                AS node,
    json_extract(value, '$.triangles')              AS triangles,
    json_extract(value, '$.clustering_coefficient') AS clustering
FROM json_each(cypher('RETURN triangleCount()'), '$[0].column_0')
ORDER BY triangles DESC;
```

***

## Algorithm Selection Guide

| Goal | Recommended Algorithm |
|------|-----------------------|
| Rank nodes by influence | PageRank |
| Find hubs and leaf nodes | Degree Centrality |
| Find brokers / bridges | Betweenness Centrality |
| Find information spreaders | Closeness Centrality |
| Fast community discovery | Label Propagation |
| High-quality communities | Louvain or Leiden |
| Shortest path (unweighted) | Dijkstra / `shortest_path` |
| Shortest path (geographic) | A\* with lat/lon properties |
| Full distance matrix | APSP (small graphs only) |
| Check connectivity | Weakly Connected Components |
| Find cycles | Strongly Connected Components |
| Find N-hop neighbors | BFS |
| Explore deep paths | DFS |
| Similar-neighbor pairs | Node Similarity |
| "People you may know" | KNN |
| Measure clustering | Triangle Count |

## Performance Tips

1. **Load the cache first.** Call `g.load_graph()` before running multiple algorithms. This populates an in-memory representation that avoids re-reading the database for each algorithm call.

2. **Reload after writes.** After inserting or updating nodes and edges, call `g.reload_graph()` so algorithms see the new data.

3. **Avoid APSP on large graphs.** `all_pairs_shortest_path()` is O(n²) in both time and memory. It is practical up to roughly 5 000 nodes; beyond that, use `shortest_path()` for specific pairs.

4. **Use `max_depth` for BFS/DFS.** Without a depth limit, traversal may visit the entire graph. Always pass a `max_depth` when you only need local neighborhood information.

5. **Betweenness scales as O(n \* m).** On graphs with millions of edges this can take minutes. For approximate betweenness, consider sampling a subset of source nodes.

6. **Leiden requires graspologic.** Install it with `pip install graphqlite[leiden]`. If graspologic is not installed, `leiden_communities()` raises an `ImportError`.
