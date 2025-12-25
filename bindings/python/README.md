# GraphQLite Python

Python bindings for GraphQLite, a SQLite extension that adds graph database capabilities using Cypher.

## Installation

```bash
pip install graphqlite
```

## Quick Start

### High-Level Graph API (Recommended)

The `Graph` class provides an ergonomic interface for common graph operations:

```python
from graphqlite import Graph

# Create a graph (in-memory or file-based)
g = Graph(":memory:")

# Add nodes
g.upsert_node("alice", {"name": "Alice", "age": 30}, label="Person")
g.upsert_node("bob", {"name": "Bob", "age": 25}, label="Person")

# Add edges
g.upsert_edge("alice", "bob", {"since": 2020}, rel_type="KNOWS")

# Query
print(g.stats())              # {'nodes': 2, 'edges': 1}
print(g.get_neighbors("alice"))  # [{'id': 'bob', ...}]
print(g.node_degree("alice"))    # 1

# Graph algorithms
ranks = g.pagerank()
communities = g.community_detection()

# Raw Cypher when needed
results = g.query("MATCH (a)-[:KNOWS]->(b) RETURN a.name, b.name")
```

### Low-Level Cypher API

For complex queries or when you need full control:

```python
from graphqlite import connect

db = connect("graph.db")

db.cypher("CREATE (a:Person {name: 'Alice', age: 30})")
db.cypher("CREATE (b:Person {name: 'Bob', age: 25})")
db.cypher("""
    MATCH (a:Person {name: 'Alice'}), (b:Person {name: 'Bob'})
    CREATE (a)-[:KNOWS]->(b)
""")

results = db.cypher("MATCH (a:Person)-[:KNOWS]->(b) RETURN a.name, b.name")
for row in results:
    print(f"{row['a.name']} knows {row['b.name']}")
```

## API Reference

### Graph Class

```python
from graphqlite import Graph, graph

# Constructor
g = Graph(db_path=":memory:", namespace="default", extension_path=None)

# Or use the factory function
g = graph(":memory:")
```

#### Node Operations

| Method | Description |
|--------|-------------|
| `upsert_node(node_id, props, label="Entity")` | Create or update a node |
| `get_node(node_id)` | Get node by ID |
| `has_node(node_id)` | Check if node exists |
| `delete_node(node_id)` | Delete node and its edges |
| `get_all_nodes(label=None)` | Get all nodes, optionally by label |

#### Edge Operations

| Method | Description |
|--------|-------------|
| `upsert_edge(source, target, props, rel_type="RELATED")` | Create edge between nodes |
| `get_edge(source, target)` | Get edge properties |
| `has_edge(source, target)` | Check if edge exists |
| `delete_edge(source, target)` | Delete edge |
| `get_all_edges()` | Get all edges |

#### Graph Queries

| Method | Description |
|--------|-------------|
| `node_degree(node_id)` | Count edges connected to node |
| `get_neighbors(node_id)` | Get adjacent nodes |
| `get_node_edges(node_id)` | Get all edges for a node |
| `stats()` | Get node/edge counts |
| `query(cypher)` | Execute raw Cypher query |

#### Graph Algorithms

**Centrality**
| Method | Description |
|--------|-------------|
| `pagerank(damping=0.85, iterations=20)` | PageRank importance scores |
| `degree_centrality()` | In/out/total degree for each node |
| `betweenness_centrality()` | Betweenness centrality scores |
| `closeness_centrality()` | Closeness centrality scores |
| `eigenvector_centrality(iterations=100)` | Eigenvector centrality scores |

**Community Detection**
| Method | Description |
|--------|-------------|
| `community_detection(iterations=10)` | Label propagation communities |
| `louvain(resolution=1.0)` | Louvain modularity optimization |
| `leiden_communities(resolution, seed)` | Leiden algorithm (requires graspologic) |

**Connected Components**
| Method | Description |
|--------|-------------|
| `weakly_connected_components()` | Weakly connected components |
| `strongly_connected_components()` | Strongly connected components |

**Path Finding**
| Method | Description |
|--------|-------------|
| `shortest_path(source, target, weight)` | Dijkstra's shortest path |
| `astar(source, target, lat, lon)` | A* with optional heuristic |
| `all_pairs_shortest_path()` | All-pairs shortest paths (Floyd-Warshall) |

**Traversal**
| Method | Description |
|--------|-------------|
| `bfs(start, max_depth=-1)` | Breadth-first search |
| `dfs(start, max_depth=-1)` | Depth-first search |

**Similarity**
| Method | Description |
|--------|-------------|
| `node_similarity(n1, n2, threshold, top_k)` | Jaccard similarity |
| `knn(node, k=10)` | K-nearest neighbors |
| `triangle_count()` | Triangle counts and clustering coefficients |

**Export**
| Method | Description |
|--------|-------------|
| `to_rustworkx()` | Export to rustworkx PyDiGraph (requires rustworkx) |

#### Batch Operations

```python
# Batch insert nodes
g.upsert_nodes_batch([
    ("n1", {"name": "Alice"}, "Person"),
    ("n2", {"name": "Bob"}, "Person"),
])

# Batch insert edges
g.upsert_edges_batch([
    ("n1", "n2", {"weight": 1.0}, "KNOWS"),
])
```

### Connection Class

```python
from graphqlite import connect, wrap

# Open new connection
db = connect("graph.db")
db = connect(":memory:")

# Wrap existing sqlite3 connection
import sqlite3
conn = sqlite3.connect("graph.db")
db = wrap(conn)
```

#### Methods

| Method | Description |
|--------|-------------|
| `cypher(query)` | Execute Cypher query, return results |
| `execute(sql)` | Execute raw SQL |
| `close()` | Close connection |

### CypherResult

Results from `cypher()` calls:

```python
results = db.cypher("MATCH (n) RETURN n.name")

len(results)           # Number of rows
results[0]             # First row as dict
results.columns        # Column names
results.to_list()      # All rows as list

for row in results:
    print(row["n.name"])
```

### Utility Functions

```python
from graphqlite import escape_string, sanitize_rel_type, CYPHER_RESERVED

# Escape strings for Cypher queries
safe = escape_string("It's a test")  # "It\\'s a test"

# Sanitize relationship types
rel = sanitize_rel_type("has-items")  # "has_items"
rel = sanitize_rel_type("CREATE")     # "REL_CREATE" (reserved word)

# Set of Cypher reserved keywords
if "MATCH" in CYPHER_RESERVED:
    print("MATCH is reserved")
```

## Extension Path

The extension is located automatically. To specify a custom path:

```python
db = connect("graph.db", extension_path="/path/to/graphqlite.dylib")
```

Or set the `GRAPHQLITE_EXTENSION_PATH` environment variable.

## Troubleshooting

See [FAQ.md](FAQ.md) for common issues and solutions.

## License

MIT
