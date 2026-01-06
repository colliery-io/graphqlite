# Graph Algorithms

GraphQLite includes 15+ built-in graph algorithms.

## Centrality Algorithms

### PageRank

Measures node importance based on incoming links from important nodes.

```cypher
RETURN pageRank()
RETURN pageRank(0.85, 20)  -- damping, iterations
```

**Returns**: `[{"node_id": int, "user_id": string, "score": float}, ...]`

**Parameters**:
- `damping` (default: 0.85) - Probability of following a link
- `iterations` (default: 20) - Number of iterations

### Degree Centrality

Counts incoming and outgoing connections.

```cypher
RETURN degreeCentrality()
```

**Returns**: `[{"node_id": int, "user_id": string, "in_degree": int, "out_degree": int, "degree": int}, ...]`

### Betweenness Centrality

Measures how often a node lies on shortest paths between other nodes.

```cypher
RETURN betweennessCentrality()
```

**Returns**: `[{"node_id": int, "user_id": string, "score": float}, ...]`

### Closeness Centrality

Measures average distance to all other nodes.

```cypher
RETURN closenessCentrality()
```

**Returns**: `[{"node_id": int, "user_id": string, "score": float}, ...]`

### Eigenvector Centrality

Measures influence based on connections to high-scoring nodes.

```cypher
RETURN eigenvectorCentrality()
RETURN eigenvectorCentrality(100)  -- max iterations
```

**Returns**: `[{"node_id": int, "user_id": string, "score": float}, ...]`

## Community Detection

### Label Propagation

Detects communities by propagating labels through the network.

```cypher
RETURN labelPropagation()
RETURN labelPropagation(10)  -- max iterations
RETURN communities()         -- alias
```

**Returns**: `[{"node_id": int, "user_id": string, "community": int}, ...]`

### Louvain

Hierarchical community detection optimizing modularity.

```cypher
RETURN louvain()
RETURN louvain(1.0)  -- resolution parameter
```

**Returns**: `[{"node_id": int, "user_id": string, "community": int}, ...]`

## Connected Components

### Weakly Connected Components (WCC)

Groups nodes reachable by ignoring edge direction.

```cypher
RETURN wcc()
```

**Returns**: `[{"node_id": int, "user_id": string, "component": int}, ...]`

### Strongly Connected Components (SCC)

Groups nodes where every node can reach every other node following edge direction.

```cypher
RETURN scc()
```

**Returns**: `[{"node_id": int, "user_id": string, "component": int}, ...]`

## Path Finding

### Dijkstra (Shortest Path)

Finds shortest path between two nodes.

```cypher
RETURN dijkstra('source_id', 'target_id')
```

**Returns**: `{"found": bool, "distance": int, "path": [node_ids]}`

The `found` field indicates whether a path exists. When `found` is false, `distance` is null and `path` is empty.

### A* Search

Shortest path with heuristic. Can use geographic coordinates for distance estimation or fall back to uniform heuristic.

```cypher
RETURN astar('source_id', 'target_id')
RETURN astar('source_id', 'target_id', 'lat_prop', 'lon_prop')
```

When `lat_prop` and `lon_prop` are provided, A* uses haversine distance as the heuristic. Without these properties, it behaves similarly to Dijkstra but may explore fewer nodes.

**Returns**: `{"found": bool, "distance": float, "path": [node_ids], "nodes_explored": int}`

### All-Pairs Shortest Paths (APSP)

Computes shortest distances between all node pairs.

```cypher
RETURN apsp()
```

**Returns**: `[{"source": string, "target": string, "distance": int}, ...]`

Note: O(n²) space and time complexity. Use with caution on large graphs.

## Traversal

### Breadth-First Search (BFS)

Explores nodes level by level from a starting point.

```cypher
RETURN bfs('start_id')
RETURN bfs('start_id', 3)  -- max depth
```

**Returns**: `[{"node_id": int, "user_id": string, "depth": int, "order": int}, ...]`

The `order` field indicates the traversal order (0 = starting node, then incrementing).

### Depth-First Search (DFS)

Explores as far as possible along each branch.

```cypher
RETURN dfs('start_id')
RETURN dfs('start_id', 5)  -- max depth
```

**Returns**: `[{"node_id": int, "user_id": string, "depth": int, "order": int}, ...]`

## Similarity

### Node Similarity (Jaccard)

Computes Jaccard similarity between node neighborhoods.

```cypher
RETURN nodeSimilarity()
```

**Returns**: `[{"node1": int, "node2": int, "similarity": float}, ...]`

### K-Nearest Neighbors (KNN)

Finds k most similar nodes to a given node based on Jaccard similarity of neighborhoods.

```cypher
RETURN knn('node_id', 10)  -- node, k
```

**Returns**: `[{"neighbor": string, "similarity": float, "rank": int}, ...]`

Results are ordered by similarity (highest first), with `rank` starting at 1.

### Triangle Count

Counts triangles and computes clustering coefficient.

```cypher
RETURN triangleCount()
```

**Returns**: `[{"node_id": int, "user_id": string, "triangles": int, "clustering_coefficient": float}, ...]`

## Using Results in SQL

Extract algorithm results using SQLite JSON functions:

```sql
SELECT
    json_extract(value, '$.node_id') as id,
    json_extract(value, '$.score') as score
FROM json_each(cypher('RETURN pageRank()'))
ORDER BY score DESC
LIMIT 10;
```
