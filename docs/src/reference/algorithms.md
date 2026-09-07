# Graph Algorithms Reference

GraphQLite provides 18 built-in graph algorithms accessible via Cypher functions, the Python `Graph` API, and the Rust `Graph` API.

> For guidance on choosing the right algorithm for your use case, see [Using Graph Algorithms](../how-to/graph-algorithms.md).

---

## PageRank

**Cypher**

```cypher
CALL pageRank([damping, iterations]) YIELD node, score
```

**Python**

```python
graph.pagerank(damping=0.85, iterations=20)
```

**Rust**

```rust
graph.pagerank(damping: f64, iterations: usize) -> Result<Vec<PageRankResult>>
```

**Parameters**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `damping` | Float | `0.85` | Damping factor |
| `iterations` | Integer | `20` | Number of power iterations |

**Return shape**

Python: `list[dict]` with keys `node_id`, `user_id`, `score`

Rust: `Vec<PageRankResult>` — fields: `node_id: i64`, `user_id: String`, `score: f64`

**Complexity**: O(iterations × (V + E))

**Example**

```python
results = graph.pagerank(damping=0.85, iterations=30)
for r in sorted(results, key=lambda x: x['score'], reverse=True)[:5]:
    print(r['user_id'], r['score'])
```

---

## Degree Centrality

**Cypher**

```cypher
CALL degreeCentrality() YIELD node, in_degree, out_degree, degree
```

**Python**

```python
graph.degree_centrality()
```

**Rust**

```rust
graph.degree_centrality() -> Result<Vec<DegreeCentralityResult>>
```

**Parameters**: none

**Return shape**

Python: `list[dict]` with keys `node_id`, `user_id`, `in_degree`, `out_degree`, `degree`

Rust: `Vec<DegreeCentralityResult>` — fields: `node_id: i64`, `user_id: String`, `in_degree: usize`, `out_degree: usize`, `degree: usize`

**Complexity**: O(V + E)

**Example**

```python
for r in graph.degree_centrality():
    print(r['user_id'], 'in:', r['in_degree'], 'out:', r['out_degree'])
```

---

## Betweenness Centrality

**Cypher**

```cypher
CALL betweennessCentrality() YIELD node, score
```

**Python**

```python
graph.betweenness_centrality()
```

**Rust**

```rust
graph.betweenness_centrality() -> Result<Vec<BetweennessCentralityResult>>
```

**Parameters**: none

**Return shape**

Python: `list[dict]` with keys `node_id`, `user_id`, `score`

Rust: `Vec<BetweennessCentralityResult>` — fields: `node_id: i64`, `user_id: String`, `score: f64`

**Complexity**: O(V × E)

**Example**

```python
results = graph.betweenness_centrality()
```

---

## Closeness Centrality

**Cypher**

```cypher
CALL closenessCentrality() YIELD node, score
```

**Python**

```python
graph.closeness_centrality()
```

**Rust**

```rust
graph.closeness_centrality() -> Result<Vec<ClosenessCentralityResult>>
```

**Parameters**: none

**Return shape**

Python: `list[dict]` with keys `node_id`, `user_id`, `score`

Rust: `Vec<ClosenessCentralityResult>` — fields: `node_id: i64`, `user_id: String`, `score: f64`

**Complexity**: O(V × (V + E))

**Example**

```python
results = graph.closeness_centrality()
```

---

## Eigenvector Centrality

**Cypher**

```cypher
CALL eigenvectorCentrality([iterations]) YIELD node, score
```

**Python**

```python
graph.eigenvector_centrality(iterations=100)
```

**Rust**

```rust
graph.eigenvector_centrality(iterations: usize) -> Result<Vec<EigenvectorCentralityResult>>
```

**Parameters**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `iterations` | Integer | `100` | Power iteration count |

**Return shape**

Python: `list[dict]` with keys `node_id`, `user_id`, `score`

Rust: `Vec<EigenvectorCentralityResult>` — fields: `node_id: i64`, `user_id: String`, `score: f64`

**Complexity**: O(iterations × E)

---

## Louvain Community Detection

**Cypher**

```cypher
CALL louvain([resolution]) YIELD node, community
```

**Python**

```python
graph.louvain(resolution=1.0)
```

**Rust**

```rust
graph.louvain(resolution: f64) -> Result<Vec<CommunityResult>>
```

**Parameters**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `resolution` | Float | `1.0` | Resolution parameter controlling community granularity |

**Return shape**

Python: `list[dict]` with keys `node_id`, `user_id`, `community`

Rust: `Vec<CommunityResult>` — fields: `node_id: i64`, `user_id: String`, `community: i64`

**Example**

```python
communities = graph.louvain(resolution=0.5)
```

---

## Leiden Community Detection

**Python only**

```python
graph.leiden_communities(resolution=1.0, random_seed=None)
```

**Parameters**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `resolution` | Float | `1.0` | Resolution parameter |
| `random_seed` | Integer \| None | `None` | Seed for reproducibility |

**Return shape**: `list[dict]` with keys `node_id`, `user_id`, `community`

---

## Label Propagation

**Cypher**

```cypher
CALL labelPropagation([iterations]) YIELD node, community
```

**Python**

```python
graph.community_detection(iterations=10)
```

**Rust**

```rust
graph.community_detection(iterations: usize) -> Result<Vec<CommunityResult>>
```

**Parameters**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `iterations` | Integer | `10` | Maximum iterations |

**Return shape**

Python: `list[dict]` with keys `node_id`, `user_id`, `community`

Rust: `Vec<CommunityResult>` — fields: `node_id: i64`, `user_id: String`, `community: i64`

---

## Weakly Connected Components

**Cypher**

```cypher
CALL weaklyConnectedComponents() YIELD node, component
```

**Python**

```python
graph.weakly_connected_components()
```

**Rust**

```rust
graph.weakly_connected_components() -> Result<Vec<ComponentResult>>
```

**Parameters**: none

**Return shape**

Python: `list[dict]` with keys `node_id`, `user_id`, `component`

Rust: `Vec<ComponentResult>` — fields: `node_id: i64`, `user_id: String`, `component: i64`

**Complexity**: O(V + E)

---

## Strongly Connected Components

**Cypher**

```cypher
CALL stronglyConnectedComponents() YIELD node, component
```

**Python**

```python
graph.strongly_connected_components()
```

**Rust**

```rust
graph.strongly_connected_components() -> Result<Vec<ComponentResult>>
```

**Parameters**: none

**Return shape**

Python: `list[dict]` with keys `node_id`, `user_id`, `component`

Rust: `Vec<ComponentResult>` — fields: `node_id: i64`, `user_id: String`, `component: i64`

**Complexity**: O(V + E) (Tarjan or Kosaraju)

---

## Shortest Path

**Cypher (path function)**

```cypher
MATCH p = shortestPath((a)-[*]->(b))
RETURN p
```

**Cypher (Dijkstra)**

```cypher
CALL dijkstra(source, target[, weight_property]) YIELD path, distance
```

**Python**

```python
graph.shortest_path(source, target, weight_property=None)
```

**Rust**

```rust
graph.shortest_path(source: &str, target: &str, weight_property: Option<&str>) -> Result<ShortestPathResult>
```

**Parameters**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `source` | String | required | Source node user ID |
| `target` | String | required | Target node user ID |
| `weight_property` | String \| None | `None` | Edge property to use as weight; unweighted BFS if `None` |

**Return shape**

Python: `dict` with keys `path` (list of user IDs), `distance` (float), `found` (bool)

Rust: `ShortestPathResult` — fields: `path: Vec<String>`, `distance: f64`, `found: bool`

**Example**

```python
result = graph.shortest_path('alice', 'bob', weight_property='cost')
if result['found']:
    print(result['path'], result['distance'])
```

---

## A* (A-Star)

**Cypher**

```cypher
CALL astar(source, target[, lat_prop, lon_prop]) YIELD path, distance
```

**Python**

```python
graph.astar(source, target, lat_prop=None, lon_prop=None)
```

**Rust**

```rust
graph.astar(source: &str, target: &str, lat_prop: Option<&str>, lon_prop: Option<&str>) -> Result<AStarResult>
```

**Parameters**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `source` | String | required | Source node user ID |
| `target` | String | required | Target node user ID |
| `lat_prop` | String \| None | `None` | Node property for latitude |
| `lon_prop` | String \| None | `None` | Node property for longitude |

**Return shape**

Python: `dict` with keys `path`, `distance`, `found`, `nodes_explored`

Rust: `AStarResult` — fields: `path: Vec<String>`, `distance: f64`, `found: bool`, `nodes_explored: usize`

---

## All-Pairs Shortest Path

**Cypher**

```cypher
CALL apsp() YIELD source, target, distance
```

**Python**

```python
graph.all_pairs_shortest_path()
```

**Rust**

```rust
graph.all_pairs_shortest_path() -> Result<Vec<ApspResult>>
```

**Parameters**: none

**Return shape**

Python: `list[dict]` with keys `source`, `target`, `distance`

Rust: `Vec<ApspResult>` — fields: `source: String`, `target: String`, `distance: f64`

**Complexity**: O(V × (V + E))

---

## BFS (Breadth-First Search)

**Cypher**

```cypher
CALL bfs(start[, max_depth]) YIELD node, depth, order
```

**Python**

```python
graph.bfs(start, max_depth=-1)
```

**Rust**

```rust
graph.bfs(start: &str, max_depth: i64) -> Result<Vec<TraversalResult>>
```

**Parameters**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `start` | String | required | Starting node user ID |
| `max_depth` | Integer | `-1` | Maximum depth; `-1` means unlimited |

**Return shape**

Python: `list[dict]` with keys `user_id`, `depth`, `order`

Rust: `Vec<TraversalResult>` — fields: `user_id: String`, `depth: usize`, `order: usize`

---

## DFS (Depth-First Search)

**Cypher**

```cypher
CALL dfs(start[, max_depth]) YIELD node, depth, order
```

**Python**

```python
graph.dfs(start, max_depth=-1)
```

**Rust**

```rust
graph.dfs(start: &str, max_depth: i64) -> Result<Vec<TraversalResult>>
```

**Parameters**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `start` | String | required | Starting node user ID |
| `max_depth` | Integer | `-1` | Maximum depth; `-1` means unlimited |

**Return shape**: same as BFS — `list[dict]` / `Vec<TraversalResult>` with `user_id`, `depth`, `order`

---

## Node Similarity

**Cypher**

```cypher
CALL nodeSimilarity([node1, node2, threshold, top_k]) YIELD node1, node2, similarity
```

**Python**

```python
graph.node_similarity(node1_id=None, node2_id=None, threshold=0.0, top_k=0)
```

**Rust**

```rust
graph.node_similarity(node1_id: Option<i64>, node2_id: Option<i64>, threshold: f64, top_k: usize) -> Result<Vec<NodeSimilarityResult>>
```

**Parameters**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `node1_id` | Integer \| None | `None` | Fix first node; `None` means all pairs |
| `node2_id` | Integer \| None | `None` | Fix second node; `None` means all pairs |
| `threshold` | Float | `0.0` | Minimum similarity to include |
| `top_k` | Integer | `0` | Return at most `top_k` results; `0` means all |

**Algorithm**: Jaccard similarity based on shared neighbors.

**Limits**: with `threshold > 0` only pairs that share at least one neighbor
can qualify, so candidates are enumerated through shared neighbors and graphs
up to 50,000 nodes are accepted. With `threshold = 0` every pair (including
similarity `0.0`) is part of the result, which is O(N²) by definition, so
all-pairs mode is capped at 5,000 nodes; use a threshold, `top_k`, or the
specific-pair form above that.

**Return shape**

Python: `list[dict]` with keys `node1`, `node2`, `similarity`

Rust: `Vec<NodeSimilarityResult>` — fields: `node1: String`, `node2: String`, `similarity: f64`

---

## KNN (k-Nearest Neighbors)

**Cypher**

```cypher
CALL knn(node, k) YIELD neighbor, similarity, rank
```

**Python**

```python
graph.knn(node_id, k=10)
```

**Rust**

```rust
graph.knn(node_id: i64, k: usize) -> Result<Vec<KnnResult>>
```

**Parameters**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `node_id` | Integer | required | Source node internal ID |
| `k` | Integer | `10` | Number of neighbors to return |

**Return shape**

Python: `list[dict]` with keys `neighbor`, `similarity`, `rank`

Rust: `Vec<KnnResult>` — fields: `neighbor: String`, `similarity: f64`, `rank: usize`

---

## Triangle Count

**Cypher**

```cypher
CALL triangleCount() YIELD node, triangles, clustering_coefficient
```

**Python**

```python
graph.triangle_count()
```

**Rust**

```rust
graph.triangle_count() -> Result<Vec<TriangleCountResult>>
```

**Parameters**: none

**Return shape**

Python: `list[dict]` with keys `node_id`, `user_id`, `triangles`, `clustering_coefficient`

Rust: `Vec<TriangleCountResult>` — fields: `node_id: i64`, `user_id: String`, `triangles: usize`, `clustering_coefficient: f64`

**Complexity**: O(V × degree²)

**Example**

```python
for r in graph.triangle_count():
    print(r['user_id'], r['triangles'], r['clustering_coefficient'])
```
