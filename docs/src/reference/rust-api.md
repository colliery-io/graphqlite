# Rust API Reference

Version: **0.4.4**

Crate: `graphqlite`

---

## `Connection`

Low-level connection type wrapping `rusqlite::Connection` with Cypher support.

### Constructors

```rust
Connection::open<P: AsRef<Path>>(path: P) -> Result<Connection>
Connection::open_in_memory() -> Result<Connection>
Connection::from_rusqlite(conn: rusqlite::Connection) -> Result<Connection>
Connection::open_with_extension<P: AsRef<Path>>(path: P, ext_path: &str) -> Result<Connection>
```

| Method | Description |
|--------|-------------|
| `open` | Open or create a database at `path` |
| `open_in_memory` | Open an in-memory database |
| `from_rusqlite` | Wrap an existing `rusqlite::Connection`; loads the extension into it |
| `open_with_extension` | Open database and load extension from explicit path |

### Methods

#### `Connection::cypher`

```rust
fn cypher(&self, query: &str) -> Result<CypherResult>
```

Execute a Cypher query with no parameters.

#### `Connection::cypher_with_params`

> **Deprecated since 0.4.0.** Use `cypher_builder()` instead.

```rust
fn cypher_with_params(&self, query: &str, params: &serde_json::Value) -> Result<CypherResult>
```

Execute a Cypher query with parameters. `params` must be a JSON object; keys correspond to `$name` placeholders.

#### `Connection::cypher_builder`

```rust
fn cypher_builder(&self, query: &str) -> CypherQuery
```

Return a `CypherQuery` builder for chaining parameter additions before execution.

#### `Connection::execute`

```rust
fn execute(&self, sql: &str) -> Result<usize>
```

Execute raw SQL. Returns the number of rows changed.

#### `Connection::sqlite_connection`

```rust
fn sqlite_connection(&self) -> &rusqlite::Connection
```

Borrow the underlying `rusqlite::Connection`.

---

## `CypherQuery`

Builder returned by `Connection::cypher_builder`.

```rust
cypher_query
    .param("name", "Alice")
    .param("age", 30)
    .run() -> Result<CypherResult>
```

---

## `CypherResult`

Represents the rows returned by a Cypher query.

### Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `len` | `fn len(&self) -> usize` | Number of rows |
| `is_empty` | `fn is_empty(&self) -> bool` | True if zero rows |
| `columns` | `fn columns(&self) -> &[String]` | Ordered column names |
| Index | `result[i]` | Returns `&Row` at index `i` |
| Iterate | `for row in &result` | Iterates `&Row` |

---

## `Row`

A single result row.

### `Row::get`

```rust
fn get<T: FromSql>(&self, column: &str) -> Result<T>
```

Get a column value by name. `T` must implement `FromSql`.

**Supported types for `T`**

| Rust type | Cypher type |
|-----------|-------------|
| `String` | String, converted from any scalar |
| `i64` | Integer |
| `f64` | Float |
| `bool` | Boolean |
| `Option<String>` | String or null |
| `Option<i64>` | Integer or null |
| `Option<f64>` | Float or null |
| `Option<bool>` | Boolean or null |

**Example**

```rust
let name: String = row.get("n.name")?;
let age: Option<i64> = row.get("n.age")?;
```

---

## `Graph`

High-level graph API mirroring the Python `Graph`.

### Constructors

```rust
Graph::open(path: &str) -> Result<Graph>
Graph::open_in_memory() -> Result<Graph>
```

### Node Operations

```rust
fn upsert_node(&self, node_id: &str, props: &serde_json::Value, label: &str) -> Result<i64>
fn get_node(&self, id: &str) -> Result<Option<serde_json::Value>>
fn has_node(&self, id: &str) -> Result<bool>
fn delete_node(&self, id: &str) -> Result<()>
fn get_all_nodes(&self, label: Option<&str>) -> Result<Vec<serde_json::Value>>
```

### Edge Operations

```rust
fn upsert_edge(&self, source: &str, target: &str, props: &serde_json::Value, rel_type: &str) -> Result<i64>
fn get_edge(&self, src: &str, dst: &str, rel_type: Option<&str>) -> Result<Option<serde_json::Value>>
fn has_edge(&self, src: &str, dst: &str, rel_type: Option<&str>) -> Result<bool>
fn delete_edge(&self, src: &str, dst: &str, rel_type: Option<&str>) -> Result<()>
fn get_all_edges(&self) -> Result<Vec<serde_json::Value>>
```

### Query

```rust
fn query(&self, cypher: &str, params: Option<&serde_json::Value>) -> Result<Vec<serde_json::Value>>
fn stats(&self) -> Result<serde_json::Value>
fn node_degree(&self, id: &str) -> Result<i64>
fn get_neighbors(&self, id: &str) -> Result<Vec<serde_json::Value>>
fn get_node_edges(&self, id: &str) -> Result<Vec<serde_json::Value>>
fn get_edges_from(&self, id: &str) -> Result<Vec<serde_json::Value>>
fn get_edges_to(&self, id: &str) -> Result<Vec<serde_json::Value>>
fn get_edges_by_type(&self, id: &str, rel_type: &str) -> Result<Vec<serde_json::Value>>
```

### Graph Cache

```rust
fn load_graph(&self) -> Result<serde_json::Value>
fn unload_graph(&self) -> Result<serde_json::Value>
fn reload_graph(&self) -> Result<serde_json::Value>
fn graph_loaded(&self) -> Result<bool>
```

### Graph Algorithms

All return `Result<Vec<T>>` where `T` is a typed result struct. See [Graph Algorithms](./algorithms.md) for parameter defaults.

| Method | Signature | Returns |
|--------|-----------|---------|
| PageRank | `fn pagerank(&self, damping: f64, iterations: usize) -> Result<Vec<PageRankResult>>` | `PageRankResult` |
| Degree centrality | `fn degree_centrality(&self) -> Result<Vec<DegreeCentralityResult>>` | `DegreeCentralityResult` |
| Betweenness | `fn betweenness_centrality(&self) -> Result<Vec<BetweennessCentralityResult>>` | `BetweennessCentralityResult` |
| Closeness | `fn closeness_centrality(&self) -> Result<Vec<ClosenessCentralityResult>>` | `ClosenessCentralityResult` |
| Eigenvector | `fn eigenvector_centrality(&self, iterations: usize) -> Result<Vec<EigenvectorCentralityResult>>` | `EigenvectorCentralityResult` |
| Community (label prop) | `fn community_detection(&self, iterations: usize) -> Result<Vec<CommunityResult>>` | `CommunityResult` |
| Louvain | `fn louvain(&self, resolution: f64) -> Result<Vec<CommunityResult>>` | `CommunityResult` |
| WCC | `fn weakly_connected_components(&self) -> Result<Vec<ComponentResult>>` | `ComponentResult` |
| SCC | `fn strongly_connected_components(&self) -> Result<Vec<ComponentResult>>` | `ComponentResult` |
| Shortest path | `fn shortest_path(&self, source: &str, target: &str, weight_property: Option<&str>) -> Result<ShortestPathResult>` | `ShortestPathResult` |
| A* | `fn astar(&self, source: &str, target: &str, lat_prop: Option<&str>, lon_prop: Option<&str>) -> Result<AStarResult>` | `AStarResult` |
| APSP | `fn all_pairs_shortest_path(&self) -> Result<Vec<ApspResult>>` | `ApspResult` |
| BFS | `fn bfs(&self, start: &str, max_depth: i64) -> Result<Vec<TraversalResult>>` | `TraversalResult` |
| DFS | `fn dfs(&self, start: &str, max_depth: i64) -> Result<Vec<TraversalResult>>` | `TraversalResult` |
| Node similarity | `fn node_similarity(&self, node1_id: Option<i64>, node2_id: Option<i64>, threshold: f64, top_k: usize) -> Result<Vec<NodeSimilarityResult>>` | `NodeSimilarityResult` |
| KNN | `fn knn(&self, node_id: i64, k: usize) -> Result<Vec<KnnResult>>` | `KnnResult` |
| Triangle count | `fn triangle_count(&self) -> Result<Vec<TriangleCountResult>>` | `TriangleCountResult` |

---

## `GraphManager`

Manages named graphs stored as SQLite files in a directory.

### Constructor

```rust
GraphManager::open(path: &str) -> Result<GraphManager>
```

### Methods

```rust
fn list(&self) -> Result<Vec<String>>
fn exists(&self, name: &str) -> bool
fn create(&self, name: &str) -> Result<Graph>
fn open(&self, name: &str) -> Result<Graph>
fn open_or_create(&self, name: &str) -> Result<Graph>
fn drop(&self, name: &str) -> Result<()>
fn query(&self, cypher: &str, graphs: Option<&[&str]>, params: Option<&serde_json::Value>) -> Result<Vec<serde_json::Value>>
fn query_sql(&self, sql: &str, graphs: &[&str], parameters: &[&dyn rusqlite::ToSql]) -> Result<Vec<serde_json::Value>>
fn close(self) -> Result<()>
```

---

## Result Types

All are plain structs deriving `Debug`, `Clone`, `serde::Serialize`, `serde::Deserialize`.

### `PageRankResult`

```rust
pub struct PageRankResult {
    pub node_id: i64,
    pub user_id: String,
    pub score: f64,
}
```

### `DegreeCentralityResult`

```rust
pub struct DegreeCentralityResult {
    pub node_id: i64,
    pub user_id: String,
    pub in_degree: usize,
    pub out_degree: usize,
    pub degree: usize,
}
```

### `BetweennessCentralityResult`

```rust
pub struct BetweennessCentralityResult {
    pub node_id: i64,
    pub user_id: String,
    pub score: f64,
}
```

### `ClosenessCentralityResult`

```rust
pub struct ClosenessCentralityResult {
    pub node_id: i64,
    pub user_id: String,
    pub score: f64,
}
```

### `EigenvectorCentralityResult`

```rust
pub struct EigenvectorCentralityResult {
    pub node_id: i64,
    pub user_id: String,
    pub score: f64,
}
```

### `CommunityResult`

```rust
pub struct CommunityResult {
    pub node_id: i64,
    pub user_id: String,
    pub community: i64,
}
```

### `ComponentResult`

```rust
pub struct ComponentResult {
    pub node_id: i64,
    pub user_id: String,
    pub component: i64,
}
```

### `ShortestPathResult`

```rust
pub struct ShortestPathResult {
    pub path: Vec<String>,
    pub distance: f64,
    pub found: bool,
}
```

### `AStarResult`

```rust
pub struct AStarResult {
    pub path: Vec<String>,
    pub distance: f64,
    pub found: bool,
    pub nodes_explored: usize,
}
```

### `ApspResult`

```rust
pub struct ApspResult {
    pub source: String,
    pub target: String,
    pub distance: f64,
}
```

### `TraversalResult`

```rust
pub struct TraversalResult {
    pub user_id: String,
    pub depth: usize,
    pub order: usize,
}
```

### `NodeSimilarityResult`

```rust
pub struct NodeSimilarityResult {
    pub node1: String,
    pub node2: String,
    pub similarity: f64,
}
```

### `KnnResult`

```rust
pub struct KnnResult {
    pub neighbor: String,
    pub similarity: f64,
    pub rank: usize,
}
```

### `TriangleCountResult`

```rust
pub struct TriangleCountResult {
    pub node_id: i64,
    pub user_id: String,
    pub triangles: usize,
    pub clustering_coefficient: f64,
}
```

---

## `Error` Enum

```rust
pub enum Error {
    Sqlite(rusqlite::Error),
    Json(serde_json::Error),
    Cypher(String),
    ExtensionNotFound(String),
    TypeError(String),
    ColumnNotFound(String),
    GraphError(String),
}
```

| Variant | Cause |
|---------|-------|
| `Sqlite` | SQLite or rusqlite error |
| `Json` | JSON serialization/deserialization failure |
| `Cypher` | Cypher parse or execution error |
| `ExtensionNotFound` | Could not locate the extension library |
| `TypeError` | Type mismatch when reading a column value |
| `ColumnNotFound` | Column name not present in result |
| `GraphError` | General graph operation failure |

`Error` implements `std::error::Error` and `std::fmt::Display`.

---

## Example

```rust
use graphqlite::{Connection, Graph};

fn main() -> graphqlite::Result<()> {
    let conn = Connection::open_in_memory()?;

    conn.cypher("CREATE (:Person {name: 'Alice', age: 30})")?;
    conn.cypher("CREATE (:Person {name: 'Bob', age: 25})")?;
    conn.cypher("MATCH (a:Person {name:'Alice'}), (b:Person {name:'Bob'}) CREATE (a)-[:KNOWS]->(b)")?;

    let result = conn.cypher("MATCH (n:Person) RETURN n.name, n.age ORDER BY n.age")?;
    for row in &result {
        let name: String = row.get("n.name")?;
        let age: i64 = row.get("n.age")?;
        println!("{} is {}", name, age);
    }

    let graph = Graph::open_in_memory()?;
    graph.upsert_node("alice", &serde_json::json!({"age": 30}), "Person")?;
    let scores = graph.pagerank(0.85, 20)?;
    for r in &scores {
        println!("{}: {:.4}", r.user_id, r.score);
    }

    Ok(())
}
```
