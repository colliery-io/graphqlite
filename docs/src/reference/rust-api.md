# Rust API Reference

## Installation

Add to your `Cargo.toml`:

```toml
[dependencies]
graphqlite = "0.3"
```

## Connection

### Opening a Connection

```rust
use graphqlite::Connection;

// In-memory database
let conn = Connection::open_in_memory()?;

// File-based database
let conn = Connection::open("graph.db")?;

// With custom extension path
let conn = Connection::open_with_extension("graph.db", "/path/to/graphqlite.so")?;
```

### Executing Cypher Queries

```rust
// Execute without results
conn.cypher("CREATE (n:Person {name: 'Alice'})")?;

// Execute with results
let rows = conn.cypher("MATCH (n:Person) RETURN n.name")?;
for row in rows {
    let name: String = row.get(0)?;
    println!("{}", name);
}
```

### Parameterized Queries

For parameterized queries, embed parameters in the query string:

```rust
use serde_json::json;

let params = json!({"name": "Alice", "age": 30});
let query = format!(
    "CREATE (n:Person {{name: '{}', age: {}}})",
    params["name"].as_str().unwrap(),
    params["age"]
);
conn.cypher(&query)?;
```

> **Note**: Direct parameter binding is planned for a future release.

## Row Access

Access row values by column name using the `get()` method:

```rust
let results = conn.cypher("MATCH (n) RETURN n.name AS name, n.age AS age")?;
for row in &results {
    let name: String = row.get("name")?;
    let age: i32 = row.get("age")?;
    println!("{} is {} years old", name, age);
}
```

The column name must match the alias in your RETURN clause. Use `AS` to create readable column names.

## Type Conversions

GraphQLite automatically converts between Cypher and Rust types:

| Cypher Type | Rust Type |
|-------------|-----------|
| Integer | `i32`, `i64` |
| Float | `f64` |
| String | `String`, `&str` |
| Boolean | `bool` |
| Null | `Option<T>` |
| List | `Vec<T>` |
| Map | `serde_json::Value` |

## Error Handling

```rust
use graphqlite::{Connection, Error};

fn example() -> Result<(), Error> {
    let conn = Connection::open_in_memory()?;

    match conn.cypher("INVALID QUERY") {
        Ok(rows) => { /* process rows */ }
        Err(Error::Cypher(msg)) => {
            eprintln!("Cypher query error: {}", msg);
        }
        Err(Error::Sqlite(e)) => {
            eprintln!("SQLite error: {}", e);
        }
        Err(e) => {
            eprintln!("Other error: {}", e);
        }
    }

    Ok(())
}
```

### Error Variants

The `Error` enum includes the following variants:

```rust
pub enum Error {
    Sqlite(rusqlite::Error),           // SQLite database errors
    Json(serde_json::Error),           // JSON parsing errors
    Cypher(String),                    // Cypher query errors
    ExtensionNotFound(String),         // Extension file not found
    TypeError { expected: &'static str, actual: String }, // Type conversion errors
    ColumnNotFound(String),            // Column doesn't exist in result
    GraphExists(String),               // Graph already exists (GraphManager)
    GraphNotFound { name: String, available: Vec<String> }, // Graph not found
    Io(std::io::Error),                // File I/O errors
}
```

## Complete Example

```rust
use graphqlite::Connection;

fn main() -> Result<(), graphqlite::Error> {
    // Open connection
    let conn = Connection::open_in_memory()?;

    // Create nodes
    conn.cypher("CREATE (a:Person {name: 'Alice', age: 30})")?;
    conn.cypher("CREATE (b:Person {name: 'Bob', age: 25})")?;

    // Create relationship
    conn.cypher("
        MATCH (a:Person {name: 'Alice'}), (b:Person {name: 'Bob'})
        CREATE (a)-[:KNOWS {since: 2020}]->(b)
    ")?;

    // Query with aliases
    let results = conn.cypher("
        MATCH (a:Person)-[:KNOWS]->(b:Person)
        RETURN a.name AS from_person, b.name AS to_person
    ")?;

    for row in &results {
        let from: String = row.get("from_person")?;
        let to: String = row.get("to_person")?;
        println!("{} knows {}", from, to);
    }

    // Query with filter (embedding values directly)
    let min_age = 26;
    let results = conn.cypher(&format!(
        "MATCH (n:Person) WHERE n.age >= {} RETURN n.name AS name",
        min_age
    ))?;

    for row in &results {
        let name: String = row.get("name")?;
        println!("Adult: {}", name);
    }

    Ok(())
}
```

## Graph Class

High-level API for graph operations, providing ergonomic methods for nodes, edges, and algorithms.

### Creating a Graph

```rust
use graphqlite::Graph;

// In-memory graph
let g = Graph::open_in_memory()?;

// File-based graph
let g = Graph::open("graph.db")?;

// With custom extension path
let g = Graph::open_with_extension("graph.db", "/path/to/graphqlite.so")?;

// From existing connection
let g = Graph::from_connection(conn)?;
```

### Node Operations

```rust
// Create or update a node
g.upsert_node("alice", [("name", "Alice"), ("age", "30")], "Person")?;

// Check if node exists
if g.has_node("alice")? {
    println!("Alice exists");
}

// Get a node
if let Some(node) = g.get_node("alice")? {
    println!("Found: {:?}", node);
}

// Get all nodes (optionally filtered by label)
let all_nodes = g.get_all_nodes(None)?;
let people = g.get_all_nodes(Some("Person"))?;

// Delete a node (also deletes connected edges)
g.delete_node("alice")?;
```

### Edge Operations

```rust
// Create or update an edge
g.upsert_edge("alice", "bob", [("since", "2020")], "KNOWS")?;

// Check if edge exists
if g.has_edge("alice", "bob")? {
    println!("Edge exists");
}

// Get an edge
if let Some(edge) = g.get_edge("alice", "bob")? {
    println!("Edge: {:?}", edge);
}

// Get all edges
let edges = g.get_all_edges()?;

// Delete an edge
g.delete_edge("alice", "bob")?;
```

### Query Operations

```rust
// Execute Cypher query
let results = g.query("MATCH (n:Person) RETURN n.name")?;

// Get graph statistics
let stats = g.stats()?;
println!("Nodes: {}, Edges: {}", stats.nodes, stats.edges);

// Get node degree (connection count)
let degree = g.node_degree("alice")?;

// Get neighbors
let neighbors = g.get_neighbors("alice")?;
```

### Batch Operations

```rust
// Batch insert nodes
let nodes = vec![
    ("alice", vec![("name", "Alice")], "Person"),
    ("bob", vec![("name", "Bob")], "Person"),
];
g.upsert_nodes_batch(nodes)?;

// Batch insert edges
let edges = vec![
    ("alice", "bob", vec![("since", "2020")], "KNOWS"),
    ("bob", "carol", vec![("since", "2021")], "KNOWS"),
];
g.upsert_edges_batch(edges)?;
```

### Algorithm Methods

#### Centrality

```rust
// PageRank
let results = g.pagerank(0.85, 20)?;  // damping, iterations
for r in results {
    println!("{}: {}", r.user_id.unwrap_or_default(), r.score);
}

// Degree centrality
let results = g.degree_centrality()?;
for r in results {
    println!("{}: in={}, out={}, total={}",
        r.user_id.unwrap_or_default(), r.in_degree, r.out_degree, r.degree);
}

// Betweenness centrality
let results = g.betweenness_centrality()?;

// Closeness centrality
let results = g.closeness_centrality()?;

// Eigenvector centrality
let results = g.eigenvector_centrality(100)?;  // iterations
```

#### Community Detection

```rust
// Label propagation
let results = g.community_detection(10)?;  // iterations
for r in results {
    println!("{} is in community {}", r.user_id.unwrap_or_default(), r.community);
}

// Louvain algorithm
let results = g.louvain(1.0)?;  // resolution
```

#### Connected Components

```rust
// Weakly connected components
let results = g.wcc()?;

// Strongly connected components
let results = g.scc()?;
```

#### Path Finding

```rust
// Shortest path (Dijkstra)
let result = g.shortest_path("alice", "bob", None)?;  // optional weight property
if result.found {
    println!("Path: {:?}, Distance: {:?}", result.path, result.distance);
}

// A* search (with optional lat/lon heuristic)
let result = g.astar("alice", "bob", None, None)?;
println!("Explored {} nodes", result.nodes_explored);

// All-pairs shortest paths
let results = g.apsp()?;
for r in results {
    println!("{} -> {}: {}", r.source, r.target, r.distance);
}
```

#### Traversal

```rust
// Breadth-first search
let results = g.bfs("alice", Some(3))?;  // optional max depth
for r in results {
    println!("{} at depth {} (order {})", r.user_id, r.depth, r.order);
}

// Depth-first search
let results = g.dfs("alice", None)?;  // None = unlimited depth
```

#### Similarity

```rust
// Node similarity (Jaccard)
let results = g.node_similarity(None, None, 0.5, 10)?;  // node1, node2, threshold, top_k
for r in results {
    println!("{} <-> {}: {}", r.node1, r.node2, r.similarity);
}

// K-nearest neighbors
let results = g.knn("alice", 5)?;
for r in results {
    println!("#{}: {} (similarity: {})", r.rank, r.neighbor, r.similarity);
}

// Triangle count
let results = g.triangle_count()?;
for r in results {
    println!("{}: {} triangles, clustering={}",
        r.user_id.unwrap_or_default(), r.triangles, r.clustering_coefficient);
}
```

### Algorithm Result Types

All algorithm methods return strongly-typed result structs:

```rust
// PageRank, Betweenness, Closeness, Eigenvector
pub struct PageRankResult {
    pub node_id: String,
    pub user_id: Option<String>,
    pub score: f64,
}

// Degree Centrality
pub struct DegreeCentralityResult {
    pub node_id: String,
    pub user_id: Option<String>,
    pub in_degree: i64,
    pub out_degree: i64,
    pub degree: i64,
}

// Community Detection, Louvain
pub struct CommunityResult {
    pub node_id: String,
    pub user_id: Option<String>,
    pub community: i64,
}

// WCC, SCC
pub struct ComponentResult {
    pub node_id: String,
    pub user_id: Option<String>,
    pub component: i64,
}

// Shortest Path
pub struct ShortestPathResult {
    pub path: Vec<String>,
    pub distance: Option<f64>,
    pub found: bool,
}

// A* Search
pub struct AStarResult {
    pub path: Vec<String>,
    pub distance: Option<f64>,
    pub found: bool,
    pub nodes_explored: i64,
}

// All-Pairs Shortest Path
pub struct ApspResult {
    pub source: String,
    pub target: String,
    pub distance: f64,
}

// BFS, DFS
pub struct TraversalResult {
    pub user_id: String,
    pub depth: i64,
    pub order: i64,
}

// Node Similarity
pub struct NodeSimilarityResult {
    pub node1: String,
    pub node2: String,
    pub similarity: f64,
}

// KNN
pub struct KnnResult {
    pub neighbor: String,
    pub similarity: f64,
    pub rank: i64,
}

// Triangle Count
pub struct TriangleCountResult {
    pub node_id: String,
    pub user_id: Option<String>,
    pub triangles: i64,
    pub clustering_coefficient: f64,
}
```

## GraphManager

Manages multiple graph databases in a directory with cross-graph query support.

### Creating a GraphManager

```rust
use graphqlite::{graphs, GraphManager};

// Using factory function (recommended)
let mut gm = graphs("./data")?;

// Or direct instantiation
let mut gm = GraphManager::open("./data")?;

// With custom extension path
let mut gm = GraphManager::open_with_extension("./data", "/path/to/graphqlite.so")?;
```

### Graph Management

```rust
// Create a new graph
let social = gm.create("social")?;

// Open an existing graph
let social = gm.open_graph("social")?;

// Open or create
let cache = gm.open_or_create("cache")?;

// List all graphs
for name in gm.list()? {
    println!("Graph: {}", name);
}

// Check if graph exists
if gm.exists("social") {
    println!("Social graph exists");
}

// Delete a graph
gm.drop("old_graph")?;
```

### Cross-Graph Queries

```rust
// Query across multiple graphs using FROM clause
let result = gm.query(
    "MATCH (n:Person) FROM social RETURN n.name, graph(n) AS source",
    &["social"]
)?;

for row in &result {
    let name: String = row.get("n.name")?;
    let source: String = row.get("source")?;
    println!("{} from {}", name, source);
}
```

### Raw SQL Cross-Graph Queries

```rust
let results = gm.query_sql(
    "SELECT COUNT(*) FROM social.nodes",
    &["social"]
)?;
```

### Complete Multi-Graph Example

```rust
use graphqlite::graphs;

fn main() -> graphqlite::Result<()> {
    let mut gm = graphs("./data")?;

    // Create and populate graphs
    {
        let social = gm.create("social")?;
        social.query("CREATE (n:Person {name: 'Alice', user_id: 'u1'})")?;
        social.query("CREATE (n:Person {name: 'Bob', user_id: 'u2'})")?;
    }

    {
        let products = gm.create("products")?;
        products.query("CREATE (n:Product {name: 'Phone', sku: 'p1'})")?;
    }

    // List graphs
    println!("Graphs: {:?}", gm.list()?);  // ["products", "social"]

    // Cross-graph query
    let result = gm.query(
        "MATCH (n:Person) FROM social RETURN n.name ORDER BY n.name",
        &["social"]
    )?;

    for row in &result {
        println!("Person: {}", row.get::<String>("n.name")?);
    }

    // Clean up
    gm.drop("products")?;
    gm.drop("social")?;

    Ok(())
}
```

### Error Handling

```rust
use graphqlite::{graphs, Error};

let mut gm = graphs("./data")?;

match gm.open_graph("nonexistent") {
    Ok(g) => { /* use graph */ }
    Err(Error::GraphNotFound { name, available }) => {
        println!("Graph '{}' not found. Available: {:?}", name, available);
    }
    Err(e) => { /* handle other errors */ }
}

match gm.create("existing") {
    Ok(g) => { /* use graph */ }
    Err(Error::GraphExists(name)) => {
        println!("Graph '{}' already exists", name);
    }
    Err(e) => { /* handle other errors */ }
}
```

## Extension Loading

For advanced use cases, wrap an existing rusqlite connection:

```rust
use rusqlite::Connection as SqliteConnection;
use graphqlite::Connection;

let sqlite_conn = SqliteConnection::open_in_memory()?;
let conn = Connection::from_rusqlite(sqlite_conn)?;
```

Or specify a custom extension path:

```rust
let conn = Connection::open_with_extension("graph.db", "/path/to/graphqlite.so")?;
```
