# Python API Reference

Version: **0.4.4**

---

## Module-level Functions

### `graphqlite.connect`

```python
graphqlite.connect(database=":memory:", extension_path=None) -> Connection
```

Open a new SQLite connection with GraphQLite loaded.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `database` | str | `":memory:"` | SQLite database path or `":memory:"` |
| `extension_path` | str \| None | `None` | Path to the `.dylib`/`.so`/`.dll`; auto-detected if `None` |

Returns: `Connection`

---

### `graphqlite.wrap`

```python
graphqlite.wrap(conn: sqlite3.Connection, extension_path=None) -> Connection
```

Wrap an existing `sqlite3.Connection` with GraphQLite loaded into it.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `conn` | `sqlite3.Connection` | required | An open SQLite connection |
| `extension_path` | str \| None | `None` | Path to extension; auto-detected if `None` |

Returns: `Connection`

---

### `graphqlite.load`

```python
graphqlite.load(conn, entry_point=None) -> None
```

Load the GraphQLite extension into `conn` without wrapping. Useful when you want to keep a plain `sqlite3.Connection`.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `conn` | `sqlite3.Connection` | required | Connection to load into |
| `entry_point` | str \| None | `None` | Extension entry point symbol; auto-detected if `None` |

---

### `graphqlite.loadable_path`

```python
graphqlite.loadable_path() -> str
```

Return the filesystem path of the bundled extension library. Use to pass to `conn.load_extension()` manually.

---

## `Connection`

A thin wrapper around `sqlite3.Connection` that adds Cypher query support.

### `Connection.cypher`

```python
conn.cypher(query: str, params=None) -> CypherResult
```

Execute a Cypher query.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `query` | str | required | Cypher query string |
| `params` | dict \| None | `None` | Parameter map; values substituted for `$name` placeholders |

Returns: `CypherResult`

Raises: `sqlite3.Error` on parse or execution failure.

**Example**

```python
result = conn.cypher("MATCH (n:Person) WHERE n.age > $min RETURN n.name", {"min": 25})
for row in result:
    print(row["n.name"])
```

---

### `Connection.execute`

```python
conn.execute(sql: str, parameters=()) -> sqlite3.Cursor
```

Execute raw SQL. Passes through to the underlying `sqlite3.Connection`.

---

### `Connection.commit`

```python
conn.commit() -> None
```

Commit the current transaction.

---

### `Connection.rollback`

```python
conn.rollback() -> None
```

Roll back the current transaction.

---

### `Connection.close`

```python
conn.close() -> None
```

Close the connection and release all resources.

---

### `Connection.sqlite_connection`

```python
conn.sqlite_connection -> sqlite3.Connection
```

The underlying `sqlite3.Connection` object.

---

## `CypherResult`

Returned by `Connection.cypher()`. Represents the result set of a Cypher query.

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `.columns` | `list[str]` | Ordered list of column names |

### Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `len` | `len(result) -> int` | Number of rows |
| `iter` | `for row in result` | Iterate rows as `dict` |
| `index` | `result[i]` | Access row by 0-based index; returns `dict` |
| `to_list` | `result.to_list() -> list[dict]` | Return all rows as a list of dicts |

**Example**

```python
result = conn.cypher("MATCH (n:Person) RETURN n.name, n.age")
print(result.columns)      # ['n.name', 'n.age']
print(len(result))         # row count
for row in result:
    print(row["n.name"])
rows = result.to_list()    # list of dicts
```

---

## `Graph`

High-level graph API built on top of `Connection`. Manages a single named graph in a SQLite database.

### Constructor

```python
graphqlite.Graph(db_path=":memory:", namespace="default", extension_path=None)
graphqlite.graph(db_path=":memory:", namespace="default", extension_path=None) -> Graph
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `db_path` | str | `":memory:"` | SQLite database path |
| `namespace` | str | `"default"` | Graph namespace identifier |
| `extension_path` | str \| None | `None` | Path to extension; auto-detected if `None` |

---

### Node Operations

#### `Graph.upsert_node`

```python
graph.upsert_node(node_id: str, props: dict, label: str = "Entity") -> int
```

Insert or update a node. `node_id` is a user-defined string identifier. Returns the internal integer ID.

#### `Graph.get_node`

```python
graph.get_node(id: str) -> dict | None
```

Return all properties of the node with user ID `id`, or `None` if not found. The returned dict includes `"_id"` (internal) and `"_label"`.

#### `Graph.has_node`

```python
graph.has_node(id: str) -> bool
```

Return `True` if a node with user ID `id` exists.

#### `Graph.delete_node`

```python
graph.delete_node(id: str) -> None
```

Delete the node and all its incident edges.

#### `Graph.get_all_nodes`

```python
graph.get_all_nodes(label: str = None) -> list[dict]
```

Return all nodes. If `label` is given, filter to that label only.

---

### Edge Operations

#### `Graph.upsert_edge`

```python
graph.upsert_edge(source: str, target: str, props: dict, rel_type: str = "RELATED") -> int
```

Insert or update an edge from `source` to `target` of type `rel_type`. Returns internal edge ID.

**Note**: Uses merge semantics — existing properties not included in `props` are preserved, not removed.

#### `Graph.get_edge`

```python
graph.get_edge(src: str, dst: str, rel_type: str = None) -> dict | None
```

Return edge properties, or `None`. If `rel_type` is `None`, returns the first matching edge.

#### `Graph.has_edge`

```python
graph.has_edge(src: str, dst: str, rel_type: str = None) -> bool
```

Return `True` if an edge from `src` to `dst` (optionally of `rel_type`) exists.

#### `Graph.delete_edge`

```python
graph.delete_edge(src: str, dst: str, rel_type: str = None) -> None
```

Delete the matching edge(s).

#### `Graph.get_all_edges`

```python
graph.get_all_edges() -> list[dict]
```

Return all edges with their properties.

---

### Query Methods

#### `Graph.node_degree`

```python
graph.node_degree(id: str) -> int
```

Total degree (in + out) of the node.

#### `Graph.get_neighbors`

```python
graph.get_neighbors(id: str) -> list[dict]
```

Return nodes connected to `id` in either direction (undirected — includes both incoming and outgoing edges).

#### `Graph.get_node_edges`

```python
graph.get_node_edges(id: str) -> list[dict]
```

Return all edges incident to `id` (in and out).

#### `Graph.get_edges_from`

```python
graph.get_edges_from(id: str) -> list[dict]
```

Return outgoing edges from `id`.

#### `Graph.get_edges_to`

```python
graph.get_edges_to(id: str) -> list[dict]
```

Return incoming edges to `id`.

#### `Graph.get_edges_by_type`

```python
graph.get_edges_by_type(id: str, rel_type: str) -> list[dict]
```

Return edges of a specific type incident to `id`.

#### `Graph.stats`

```python
graph.stats() -> dict
```

Return graph statistics. Keys: `node_count`, `edge_count`.

#### `Graph.query`

```python
graph.query(cypher: str, params: dict = None) -> list[dict]
```

Execute a Cypher query and return all rows as a list of dicts.

---

### Graph Cache

#### `Graph.load_graph`

```python
graph.load_graph() -> dict
```

Load the graph into the in-memory adjacency cache for algorithm use. Returns status dict.

#### `Graph.unload_graph`

```python
graph.unload_graph() -> dict
```

Release the in-memory adjacency cache.

#### `Graph.reload_graph`

```python
graph.reload_graph() -> dict
```

Unload and reload the cache.

#### `Graph.graph_loaded`

```python
graph.graph_loaded() -> bool
```

Return `True` if the adjacency cache is currently loaded.

---

### Graph Algorithms

All algorithm methods return lists of dicts. See [Graph Algorithms](./algorithms.md) for full parameter and return field documentation.

| Method | Signature |
|--------|-----------|
| PageRank | `graph.pagerank(damping=0.85, iterations=20)` |
| Degree centrality | `graph.degree_centrality()` |
| Betweenness centrality | `graph.betweenness_centrality()` |
| Closeness centrality | `graph.closeness_centrality()` |
| Eigenvector centrality | `graph.eigenvector_centrality(iterations=100)` |
| Label propagation | `graph.community_detection(iterations=10)` |
| Louvain | `graph.louvain(resolution=1.0)` |
| Leiden | `graph.leiden_communities(resolution=1.0, random_seed=None)` |
| Weakly connected components | `graph.weakly_connected_components()` |
| Strongly connected components | `graph.strongly_connected_components()` |
| Shortest path | `graph.shortest_path(source, target, weight_property=None)` |
| A* | `graph.astar(source, target, lat_prop=None, lon_prop=None)` |
| All-pairs shortest path | `graph.all_pairs_shortest_path()` |
| BFS | `graph.bfs(start, max_depth=-1)` |
| DFS | `graph.dfs(start, max_depth=-1)` |
| Node similarity | `graph.node_similarity(node1_id=None, node2_id=None, threshold=0.0, top_k=0)` |
| KNN | `graph.knn(node_id, k=10)` |
| Triangle count | `graph.triangle_count()` |

### Method Aliases

The following aliases are available on `Graph` for convenience:

| Alias | Canonical Method |
|-------|-----------------|
| `dijkstra` | `shortest_path` |
| `a_star` | `astar` |
| `apsp` | `all_pairs_shortest_path` |
| `breadth_first_search` | `bfs` |
| `depth_first_search` | `dfs` |
| `triangles` | `triangle_count` |

---

### Bulk Operations

#### `Graph.insert_nodes_bulk`

```python
graph.insert_nodes_bulk(
    nodes: list[tuple[str, dict[str, Any], str]]
) -> dict[str, int]
```

Insert multiple nodes in a single transaction, bypassing Cypher. Each tuple is `(external_id, properties, label)`. Returns a dict mapping each external ID to its internal SQLite rowid.

```python
id_map = g.insert_nodes_bulk([
    ("alice", {"name": "Alice", "age": 30}, "Person"),
    ("bob",   {"name": "Bob",   "age": 25}, "Person"),
])
# id_map = {"alice": 1, "bob": 2}
```

#### `Graph.insert_edges_bulk`

```python
graph.insert_edges_bulk(
    edges: list[tuple[str, str, dict[str, Any], str]],
    id_map: dict[str, int] = None
) -> int
```

Insert multiple edges in a single transaction. Each tuple is `(source_id, target_id, properties, rel_type)`. The optional `id_map` (from `insert_nodes_bulk`) maps external IDs to internal rowids for fast resolution. If `id_map` is `None`, IDs are looked up from the database. Returns the number of edges inserted. Raises `KeyError` if an external ID is not found in `id_map`.

#### `Graph.insert_graph_bulk`

```python
graph.insert_graph_bulk(
    nodes: list[tuple[str, dict[str, Any], str]],
    edges: list[tuple[str, str, dict[str, Any], str]]
) -> BulkInsertResult
```

Insert nodes and edges together. Internally calls `insert_nodes_bulk` then `insert_edges_bulk`. Returns a `BulkInsertResult` dataclass:

| Field | Type | Description |
|-------|------|-------------|
| `nodes_inserted` | `int` | Number of nodes inserted |
| `edges_inserted` | `int` | Number of edges inserted |
| `id_map` | `dict[str, int]` | Mapping from external IDs to internal rowids |

#### `Graph.resolve_node_ids`

```python
graph.resolve_node_ids(ids: list[str]) -> dict[str, int]
```

Look up internal rowids for existing nodes by their external IDs. Returns `{external_id: internal_rowid}`. Use this to build an `id_map` for `insert_edges_bulk` when connecting to nodes that were inserted in a previous operation.

---

### Batch Operations

> **Non-atomicity warning:** Batch upsert methods call `upsert_node`/`upsert_edge` in a loop. If an operation fails partway through, earlier operations will have already completed. For atomic batch inserts, use the bulk insert methods instead, or wrap the call in an explicit transaction.

#### `Graph.upsert_nodes_batch`

```python
graph.upsert_nodes_batch(
    nodes: list[tuple[str, dict[str, Any], str]]
) -> None
```

Upsert multiple nodes. Each tuple is `(node_id, properties, label)`. Uses MERGE semantics (update if exists, create if not).

#### `Graph.upsert_edges_batch`

```python
graph.upsert_edges_batch(
    edges: list[tuple[str, str, dict[str, Any], str]]
) -> None
```

Upsert multiple edges. Each tuple is `(source_id, target_id, properties, rel_type)`. Uses MERGE semantics.

---

### Export

#### `Graph.to_rustworkx`

```python
graph.to_rustworkx() -> PyDiGraph
```

Export the graph to a `rustworkx.PyDiGraph`. Requires `rustworkx` to be installed.

---

## `GraphManager`

Manages multiple named graphs stored as separate SQLite files under a base directory.

### Constructor

```python
graphqlite.GraphManager(base_path: str, extension_path: str = None)
graphqlite.graphs(base_path: str, extension_path: str = None) -> GraphManager
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `base_path` | str | Directory containing graph database files |
| `extension_path` | str \| None | Path to extension; auto-detected if `None` |

### Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `list` | `manager.list() -> list[str]` | Names of all graphs in `base_path` |
| `exists` | `manager.exists(name: str) -> bool` | True if a graph named `name` exists |
| `create` | `manager.create(name: str) -> Graph` | Create and return a new graph |
| `open` | `manager.open(name: str) -> Graph` | Open existing graph; raises if not found |
| `open_or_create` | `manager.open_or_create(name: str) -> Graph` | Open or create |
| `drop` | `manager.drop(name: str) -> None` | Delete the graph database file |
| `query` | `manager.query(cypher: str, graphs: list[str] = None, params: dict = None) -> list` | Query across multiple graphs; `graphs=None` queries all |
| `query_sql` | `manager.query_sql(sql: str, graphs: list[str], parameters: tuple = ()) -> list` | Raw SQL across multiple graphs |
| `close` | `manager.close() -> None` | Close all open connections |

### Dunder Methods

| Method | Description |
|--------|-------------|
| `__iter__` | Iterate over all graph names in `base_path` (same as `list()`) |
| `__contains__` | `name in manager` — True if a graph named `name` exists (same as `exists()`) |
| `__len__` | `len(manager)` — Number of graphs in `base_path` |
| `__enter__` / `__exit__` | Context manager support; calls `close()` on exit |

---

## Utilities

### `graphqlite.escape_string`

```python
graphqlite.escape_string(s: str) -> str
```

Escape a string for safe embedding in a Cypher query literal (single-quote escaping).

### `graphqlite.sanitize_rel_type`

```python
graphqlite.sanitize_rel_type(type: str) -> str
```

Normalize a relationship type string to a safe identifier (uppercase, underscores only).

### `graphqlite.format_props`

```python
graphqlite.format_props(props: dict, escape_fn=escape_string) -> str
```

Format a properties dict as a Cypher property string. For example, `{"name": "Alice", "age": 30}` becomes `{name: 'Alice', age: 30}`. The `escape_fn` is applied to string values; defaults to `escape_string`.

### `graphqlite.CYPHER_RESERVED`

```python
graphqlite.CYPHER_RESERVED -> set[str]
```

Set of all Cypher reserved keywords. Use to check whether an identifier needs quoting.
