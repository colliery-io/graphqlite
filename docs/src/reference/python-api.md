# Python API Reference

## Installation

```bash
pip install graphqlite
```

## Module Functions

### graphqlite.connect()

Create a connection to a SQLite database with GraphQLite loaded.

```python
from graphqlite import connect

conn = connect(":memory:")
conn = connect("graph.db")
conn = connect("graph.db", extension_path="/path/to/graphqlite.dylib")
```

**Parameters**:
- `database` (str) - Database path or `:memory:`
- `extension_path` (str, optional) - Path to extension file

**Returns**: `Connection`

### graphqlite.load()

Load GraphQLite into an existing sqlite3 connection.

```python
import sqlite3
import graphqlite

conn = sqlite3.connect(":memory:")
graphqlite.load(conn)
```

**Parameters**:
- `conn` - sqlite3.Connection or apsw.Connection
- `entry_point` (str, optional) - Extension entry point

### graphqlite.loadable_path()

Get the path to the loadable extension.

```python
path = graphqlite.loadable_path()
```

**Returns**: str

### graphqlite.wrap()

Wrap an existing sqlite3 connection with GraphQLite support.

```python
import sqlite3
import graphqlite

conn = sqlite3.connect(":memory:")
wrapped = graphqlite.wrap(conn)
results = wrapped.cypher("RETURN 1 AS x")
```

**Parameters**:
- `conn` - sqlite3.Connection object
- `extension_path` (str, optional) - Path to extension file

**Returns**: `Connection`

### graphqlite.graph()

Factory function to create a Graph instance.

```python
from graphqlite import graph

g = graph(":memory:")
g = graph("graph.db", namespace="myapp")
```

**Parameters**:
- `db_path` (str) - Database path or `:memory:`
- `namespace` (str, optional) - Graph namespace (default: "default")
- `extension_path` (str, optional) - Path to extension file

**Returns**: `Graph`

## CypherResult Class

Result container returned by `cypher()` queries.

```python
results = conn.cypher("MATCH (n:Person) RETURN n.name, n.age")

# Length
print(len(results))  # Number of rows

# Indexing
first_row = results[0]  # Get first row as dict

# Iteration
for row in results:
    print(row["n.name"])

# Column names
print(results.columns)  # ["n.name", "n.age"]

# Convert to list
all_rows = results.to_list()  # List of dicts
```

**Properties**:
- `columns` - List of column names

**Methods**:
- `to_list()` - Return all rows as a list of dictionaries

## Connection Class

### Connection.cypher()

Execute a Cypher query with optional parameters.

```python
conn.cypher("CREATE (n:Person {name: 'Alice'})")
results = conn.cypher("MATCH (n) RETURN n.name")
for row in results:
    print(row["n.name"])

# With parameters
results = conn.cypher(
    "MATCH (n:Person {name: $name}) RETURN n",
    {"name": "Alice"}
)
```

The `query` parameter is the Cypher query string. The optional `params` parameter accepts a dictionary that will be converted to JSON for parameter binding.

**Returns**: `CypherResult` object (iterable, supports indexing and `len()`)

### Connection.execute()

Execute raw SQL.

```python
conn.execute("SELECT * FROM nodes")
```

## Graph Class

High-level API for graph operations.

### Constructor

```python
from graphqlite import Graph

g = Graph(":memory:")
g = Graph("graph.db")
```

### Node Operations

#### upsert_node()

Create or update a node.

```python
g.upsert_node("alice", {"name": "Alice", "age": 30}, label="Person")
```

**Parameters**:
- `node_id` (str) - Unique node identifier
- `properties` (dict) - Node properties
- `label` (str, optional) - Node label

#### get_node()

Get a node by ID.

```python
node = g.get_node("alice")
# {"id": "alice", "label": "Person", "properties": {"name": "Alice", "age": 30}}
```

**Returns**: dict or None

#### has_node()

Check if a node exists.

```python
exists = g.has_node("alice")  # True
```

**Returns**: bool

#### delete_node()

Delete a node.

```python
g.delete_node("alice")
```

#### get_all_nodes()

Get all nodes, optionally filtered by label.

```python
all_nodes = g.get_all_nodes()
people = g.get_all_nodes(label="Person")
```

**Returns**: List of dicts

### Edge Operations

#### upsert_edge()

Create or update an edge.

```python
g.upsert_edge("alice", "bob", {"since": 2020}, rel_type="KNOWS")
```

**Parameters**:
- `source_id` (str) - Source node ID
- `target_id` (str) - Target node ID
- `properties` (dict) - Edge properties
- `rel_type` (str, optional) - Relationship type

#### get_edge()

Get an edge between two nodes.

```python
edge = g.get_edge("alice", "bob")
```

Returns the first edge found between the source and target nodes, or `None` if no edge exists.

#### has_edge()

Check if an edge exists.

```python
exists = g.has_edge("alice", "bob")
```

**Returns**: bool

#### delete_edge()

Delete an edge between two nodes.

```python
g.delete_edge("alice", "bob")
```

#### get_all_edges()

Get all edges.

```python
edges = g.get_all_edges()
```

**Returns**: List of dicts

### Graph Operations

#### get_neighbors()

Get a node's neighbors (connected by edges in either direction).

```python
neighbors = g.get_neighbors("alice")
```

**Parameters**:
- `node_id` (str) - Node ID

**Returns**: List of neighbor node dicts

#### node_degree()

Get a node's degree, which is the total number of edges connected to the node (both incoming and outgoing).

```python
degree = g.node_degree("alice")  # 5
```

Returns an integer count of connected edges.

#### stats()

Get graph statistics.

```python
stats = g.stats()
# {"nodes": 100, "edges": 250}
```

**Returns**: dict

### Query Methods

#### query()

Execute a Cypher query and return results as a list of dictionaries.

```python
results = g.query("MATCH (n:Person) RETURN n.name")
for row in results:
    print(row["n.name"])
```

This method is for queries that don't require parameters. For parameterized queries, access the underlying connection:

```python
results = g.connection.cypher(
    "MATCH (n:Person {name: $name}) RETURN n",
    {"name": "Alice"}
)
```

### Algorithm Methods

#### Centrality Algorithms

##### pagerank()

Compute PageRank scores for all nodes.

```python
results = g.pagerank(damping=0.85, iterations=20)
# [{"node_id": "alice", "score": 0.25}, ...]
```

**Parameters**:
- `damping` (float, default: 0.85) - Damping factor
- `iterations` (int, default: 20) - Number of iterations

##### degree_centrality()

Compute in-degree, out-degree, and total degree for all nodes.

```python
results = g.degree_centrality()
# [{"node_id": "alice", "in_degree": 2, "out_degree": 3, "degree": 5}, ...]
```

##### betweenness_centrality()

Compute betweenness centrality (how often a node lies on shortest paths).

```python
results = g.betweenness_centrality()
# Alias: g.betweenness()
```

**Returns**: List of `{"node_id": str, "score": float}`

##### closeness_centrality()

Compute closeness centrality (average distance to all other nodes).

```python
results = g.closeness_centrality()
# Alias: g.closeness()
```

**Returns**: List of `{"node_id": str, "score": float}`

##### eigenvector_centrality()

Compute eigenvector centrality (influence based on connections to high-scoring nodes).

```python
results = g.eigenvector_centrality(iterations=100)
```

**Parameters**:
- `iterations` (int, default: 100) - Maximum iterations

#### Community Detection

##### community_detection()

Detect communities using label propagation.

```python
results = g.community_detection(iterations=10)
# [{"node_id": "alice", "community": 1}, ...]
```

**Parameters**:
- `iterations` (int, default: 10) - Maximum iterations

##### louvain()

Detect communities using the Louvain algorithm (modularity optimization).

```python
results = g.louvain(resolution=1.0)
```

**Parameters**:
- `resolution` (float, default: 1.0) - Higher values produce more communities

##### leiden_communities()

Detect communities using the Leiden algorithm.

```python
results = g.leiden_communities(resolution=1.0, random_seed=42)
```

**Parameters**:
- `resolution` (float, default: 1.0) - Resolution parameter
- `random_seed` (int, optional) - Random seed for reproducibility

**Requires**: `graspologic>=3.0` (`pip install graspologic`)

#### Connected Components

##### weakly_connected_components()

Find weakly connected components (ignoring edge direction).

```python
results = g.weakly_connected_components()
# Aliases: g.connected_components(), g.wcc()
```

**Returns**: List of `{"node_id": str, "component": int}`

##### strongly_connected_components()

Find strongly connected components (respecting edge direction).

```python
results = g.strongly_connected_components()
# Alias: g.scc()
```

**Returns**: List of `{"node_id": str, "component": int}`

#### Path Finding

##### shortest_path()

Find the shortest path between two nodes using Dijkstra's algorithm.

```python
path = g.shortest_path("alice", "bob", weight_property="distance")
# {"distance": 2, "path": ["alice", "carol", "bob"], "found": True}
# Alias: g.dijkstra()
```

**Parameters**:
- `source_id` (str) - Starting node ID
- `target_id` (str) - Ending node ID
- `weight_property` (str, optional) - Edge property to use as weight

**Returns**: `{"path": list, "distance": float|None, "found": bool}`

##### astar()

Find the shortest path using A* algorithm with optional geographic heuristic.

```python
path = g.astar("alice", "bob", lat_prop="latitude", lon_prop="longitude")
# Alias: g.a_star()
```

**Parameters**:
- `source_id` (str) - Starting node ID
- `target_id` (str) - Ending node ID
- `lat_prop` (str, optional) - Latitude property name for heuristic
- `lon_prop` (str, optional) - Longitude property name for heuristic

**Returns**: `{"path": list, "distance": float|None, "found": bool, "nodes_explored": int}`

##### all_pairs_shortest_path()

Compute shortest distances between all node pairs (Floyd-Warshall).

```python
results = g.all_pairs_shortest_path()
# Alias: g.apsp()
```

**Returns**: List of `{"source": str, "target": str, "distance": float}`

**Note**: O(n²) complexity. Use with caution on large graphs.

#### Traversal

##### bfs()

Breadth-first search from a starting node.

```python
results = g.bfs("alice", max_depth=3)
# Alias: g.breadth_first_search()
```

**Parameters**:
- `start_id` (str) - Starting node ID
- `max_depth` (int, default: -1) - Maximum depth (-1 for unlimited)

**Returns**: List of `{"user_id": str, "depth": int, "order": int}`

##### dfs()

Depth-first search from a starting node.

```python
results = g.dfs("alice", max_depth=5)
# Alias: g.depth_first_search()
```

**Parameters**:
- `start_id` (str) - Starting node ID
- `max_depth` (int, default: -1) - Maximum depth (-1 for unlimited)

**Returns**: List of `{"user_id": str, "depth": int, "order": int}`

#### Similarity

##### node_similarity()

Compute Jaccard similarity between node neighborhoods.

```python
# All pairs above threshold
results = g.node_similarity(threshold=0.5)

# Specific pair
results = g.node_similarity(node1_id="alice", node2_id="bob")

# Top-k most similar pairs
results = g.node_similarity(top_k=10)
```

**Parameters**:
- `node1_id` (str, optional) - First node ID
- `node2_id` (str, optional) - Second node ID
- `threshold` (float, default: 0.0) - Minimum similarity threshold
- `top_k` (int, default: 0) - Return only top-k pairs (0 for all)

**Returns**: List of `{"node1": str, "node2": str, "similarity": float}`

##### knn()

Find k-nearest neighbors for a node based on Jaccard similarity.

```python
results = g.knn("alice", k=10)
```

**Parameters**:
- `node_id` (str) - Node to find neighbors for
- `k` (int, default: 10) - Number of neighbors to return

**Returns**: List of `{"neighbor": str, "similarity": float, "rank": int}`

##### triangle_count()

Count triangles and compute clustering coefficients.

```python
results = g.triangle_count()
# Alias: g.triangles()
```

**Returns**: List of `{"node_id": str, "triangles": int, "clustering_coefficient": float}`

#### Export

##### to_rustworkx()

Export the graph to a rustworkx PyDiGraph for use with rustworkx algorithms.

```python
graph, node_map = g.to_rustworkx()
```

**Returns**: Tuple of (rustworkx.PyDiGraph, dict mapping node IDs to indices)

**Requires**: `rustworkx>=0.13` (`pip install rustworkx`)

### Batch Operations

#### upsert_nodes_batch()

```python
nodes = [
    ("alice", {"name": "Alice"}, "Person"),
    ("bob", {"name": "Bob"}, "Person"),
]
g.upsert_nodes_batch(nodes)
```

#### upsert_edges_batch()

```python
edges = [
    ("alice", "bob", {"since": 2020}, "KNOWS"),
    ("bob", "carol", {"since": 2021}, "KNOWS"),
]
g.upsert_edges_batch(edges)
```

## GraphManager Class

Manages multiple graph databases in a directory with cross-graph query support.

### Constructor

```python
from graphqlite import graphs, GraphManager

# Using factory function (recommended)
gm = graphs("./data")

# Or direct instantiation
gm = GraphManager("./data")
gm = GraphManager("./data", extension_path="/path/to/graphqlite.dylib")
```

### Context Manager

```python
with graphs("./data") as gm:
    # Work with graphs...
    pass  # All connections closed automatically
```

### Graph Management

#### list()

List all graphs in the directory.

```python
names = gm.list()  # ["products", "social", "users"]
```

**Returns**: List of graph names (sorted)

#### exists()

Check if a graph exists.

```python
if gm.exists("social"):
    print("Graph exists")
```

**Returns**: bool

#### create()

Create a new graph.

```python
g = gm.create("social")
```

**Parameters**:
- `name` (str) - Graph name

**Returns**: `Graph` instance

**Raises**: `FileExistsError` if graph already exists

#### open()

Open an existing graph.

```python
g = gm.open("social")
```

**Parameters**:
- `name` (str) - Graph name

**Returns**: `Graph` instance

**Raises**: `FileNotFoundError` if graph doesn't exist

#### open_or_create()

Open a graph, creating it if it doesn't exist.

```python
g = gm.open_or_create("cache")
```

**Returns**: `Graph` instance

#### drop()

Delete a graph and its database file.

```python
gm.drop("old_graph")
```

**Raises**: `FileNotFoundError` if graph doesn't exist

### Cross-Graph Queries

#### query()

Execute a Cypher query across multiple graphs.

```python
result = gm.query(
    "MATCH (n:Person) FROM social RETURN n.name, graph(n) AS source",
    graphs=["social"]
)
for row in result:
    print(f"{row['n.name']} from {row['source']}")
```

**Parameters**:
- `cypher` (str) - Cypher query with FROM clauses
- `graphs` (list) - Graph names to attach
- `params` (dict, optional) - Query parameters

**Returns**: `CypherResult`

#### query_sql()

Execute raw SQL across attached graphs.

```python
result = gm.query_sql(
    "SELECT COUNT(*) FROM social.nodes",
    graphs=["social"]
)
```

**Parameters**:
- `sql` (str) - SQL query with graph-prefixed table names
- `graphs` (list) - Graph names to attach
- `parameters` (tuple, optional) - Query parameters

**Returns**: List of tuples

### Collection Interface

```python
# Length
len(gm)  # Number of graphs

# Membership
"social" in gm  # True/False

# Iteration
for name in gm:
    print(name)
```

## Utility Functions

### escape_string()

Escape a string for use in Cypher.

```python
from graphqlite import escape_string

safe = escape_string("It's a test")
```

### sanitize_rel_type()

Sanitize a relationship type name.

```python
from graphqlite import sanitize_rel_type

safe = sanitize_rel_type("has-friend")  # "HAS_FRIEND"
```

### CYPHER_RESERVED

A set of reserved Cypher keywords that need special handling in queries.

```python
from graphqlite import CYPHER_RESERVED

if my_label.upper() in CYPHER_RESERVED:
    my_label = f"`{my_label}`"  # Quote reserved words
```

Contains keywords like: `MATCH`, `CREATE`, `RETURN`, `WHERE`, `AND`, `OR`, `NOT`, `IN`, `AS`, `WITH`, `ORDER`, `BY`, `LIMIT`, `SKIP`, `DELETE`, `SET`, `REMOVE`, `MERGE`, `ON`, `CASE`, `WHEN`, `THEN`, `ELSE`, `END`, `TRUE`, `FALSE`, `NULL`, etc.
