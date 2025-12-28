# Use Graph Algorithms

GraphQLite includes 15+ built-in graph algorithms. This guide shows how to use them effectively.

## Using Algorithms with the Graph API

The `Graph` class provides direct methods for common algorithms:

```python
from graphqlite import Graph

g = Graph("my_graph.db")

# Centrality
pagerank = g.pagerank(damping=0.85, iterations=20)
degree = g.degree_centrality()

# Community detection
communities = g.community_detection(max_iterations=10)

# Path finding
path = g.shortest_path("alice", "bob")

# Components
components = g.connected_components()
```

## Using Algorithms with Cypher

For algorithms not exposed directly, use the `RETURN` clause:

```python
# Betweenness centrality
results = g.query("RETURN betweennessCentrality()")

# Louvain community detection
results = g.query("RETURN louvain(1.0)")

# A* pathfinding
results = g.query("RETURN astar('start', 'end', 'lat', 'lon')")
```

## Working with Results

All algorithms return JSON results that are parsed into Python dictionaries:

```python
pagerank = g.pagerank()

# Results are a list of dicts
for node in pagerank:
    print(f"Node {node['user_id']}: score {node['score']}")

# Sort by score
top_nodes = sorted(pagerank, key=lambda x: x['score'], reverse=True)[:10]

# Filter
high_scores = [n for n in pagerank if n['score'] > 0.1]
```

## Using Results in SQL

When using raw SQL, extract values with `json_each` and `json_extract`:

```sql
-- Get top 10 PageRank nodes
SELECT
    json_extract(value, '$.node_id') as id,
    json_extract(value, '$.score') as score
FROM json_each(cypher('RETURN pageRank()'))
ORDER BY score DESC
LIMIT 10;
```

## Algorithm Parameters

### PageRank

```python
g.pagerank(damping=0.85, iterations=20)
```

- `damping`: Probability of following a link (default: 0.85)
- `iterations`: Number of iterations (default: 20)

### Label Propagation

```python
g.community_detection(max_iterations=10)
```

- `max_iterations`: Maximum iterations before stopping (default: 10)

### Shortest Path

```python
g.shortest_path(source_id, target_id)
```

Returns `{"distance": int, "path": [node_ids]}` or `None` if no path exists.

### A* Pathfinding

```sql
SELECT cypher('RETURN astar("start", "end", "lat", "lon")');
```

Requires nodes to have coordinate properties for the heuristic.

## Performance Considerations

- **Small graphs (<10K nodes)**: All algorithms run instantly
- **Medium graphs (10K-100K nodes)**: Most algorithms complete in under a second
- **Large graphs (>100K nodes)**: Some algorithms (PageRank, community detection) may take several seconds

For large graphs, consider:
1. Running algorithms in a background thread
2. Caching results if the graph doesn't change frequently
3. Using approximate algorithms for real-time queries
