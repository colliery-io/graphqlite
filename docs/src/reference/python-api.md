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

#### pagerank()

```python
results = g.pagerank(damping=0.85, iterations=20)
```

#### degree_centrality()

```python
results = g.degree_centrality()
```

#### community_detection()

```python
results = g.community_detection(iterations=10)
```

#### shortest_path()

```python
path = g.shortest_path("alice", "bob")
# {"distance": 2, "path": ["alice", "carol", "bob"], "found": True}
```

Returns `{"path": [], "distance": None, "found": False}` if no path exists.

#### connected_components()

```python
components = g.connected_components()
```

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
