# GraphQLite Rust

Rust bindings for GraphQLite, a SQLite extension that adds graph database capabilities using Cypher.

## Installation

Add to your `Cargo.toml`:

```toml
[dependencies]
graphqlite = "0.1.0-beta"
```

## Quick Start

### High-Level Graph API (Recommended)

The `Graph` struct provides an ergonomic interface for common graph operations:

```rust
use graphqlite::Graph;

fn main() -> graphqlite::Result<()> {
    let g = Graph::open(":memory:")?;

    // Add nodes
    g.upsert_node("alice", [("name", "Alice"), ("age", "30")], "Person")?;
    g.upsert_node("bob", [("name", "Bob"), ("age", "25")], "Person")?;

    // Add edge
    g.upsert_edge("alice", "bob", [("since", "2020")], "KNOWS")?;

    // Query
    println!("{:?}", g.stats()?);           // GraphStats { nodes: 2, edges: 1 }
    println!("{:?}", g.get_neighbors("alice")?);

    // Graph algorithms
    let ranks = g.pagerank(0.85, 20)?;
    let communities = g.community_detection(10)?;

    Ok(())
}
```

### Low-Level Cypher API

For complex queries or when you need full control:

```rust
use graphqlite::Connection;

fn main() -> graphqlite::Result<()> {
    let conn = Connection::open("graph.db")?;

    conn.cypher("CREATE (a:Person {name: 'Alice', age: 30})")?;
    conn.cypher("CREATE (b:Person {name: 'Bob', age: 25})")?;
    conn.cypher(r#"
        MATCH (a:Person {name: 'Alice'}), (b:Person {name: 'Bob'})
        CREATE (a)-[:KNOWS]->(b)
    "#)?;

    let results = conn.cypher("MATCH (a:Person)-[:KNOWS]->(b) RETURN a.name, b.name")?;
    for row in &results {
        let a: String = row.get("a.name")?;
        let b: String = row.get("b.name")?;
        println!("{} knows {}", a, b);
    }

    Ok(())
}
```

## API Reference

### Graph

```rust
use graphqlite::{Graph, graph};

// Constructor
let g = Graph::open("graph.db")?;
let g = Graph::open(":memory:")?;
let g = Graph::open_in_memory()?;

// Or use the factory function
let g = graph(":memory:")?;
```

#### Node Operations

| Method | Description |
|--------|-------------|
| `upsert_node(id, props, label)` | Create or update a node |
| `get_node(id)` | Get node by ID |
| `has_node(id)` | Check if node exists |
| `delete_node(id)` | Delete node and its edges |
| `get_all_nodes(label)` | Get all nodes, optionally by label |

#### Edge Operations

| Method | Description |
|--------|-------------|
| `upsert_edge(src, dst, props, type)` | Create edge between nodes |
| `get_edge(src, dst)` | Get edge properties |
| `has_edge(src, dst)` | Check if edge exists |
| `delete_edge(src, dst)` | Delete edge |
| `get_all_edges()` | Get all edges |

#### Graph Queries

| Method | Description |
|--------|-------------|
| `node_degree(id)` | Count edges connected to node |
| `get_neighbors(id)` | Get adjacent nodes |
| `stats()` | Get node/edge counts |
| `query(cypher)` | Execute raw Cypher query |

#### Graph Algorithms

**Centrality**
| Method | Description |
|--------|-------------|
| `pagerank(damping, iterations)` | PageRank importance scores |
| `degree_centrality()` | In/out/total degree for each node |
| `betweenness_centrality()` | Betweenness centrality scores |
| `closeness_centrality()` | Closeness centrality scores |
| `eigenvector_centrality(iterations)` | Eigenvector centrality scores |

**Community Detection**
| Method | Description |
|--------|-------------|
| `community_detection(iterations)` | Label propagation communities |
| `louvain(resolution)` | Louvain modularity optimization |

**Connected Components**
| Method | Description |
|--------|-------------|
| `wcc()` | Weakly connected components |
| `scc()` | Strongly connected components |

**Path Finding**
| Method | Description |
|--------|-------------|
| `shortest_path(src, dst, weight)` | Dijkstra's shortest path |
| `astar(src, dst, lat, lon)` | A* with optional heuristic |
| `apsp()` | All-pairs shortest paths (Floyd-Warshall) |

**Traversal**
| Method | Description |
|--------|-------------|
| `bfs(start, max_depth)` | Breadth-first search |
| `dfs(start, max_depth)` | Depth-first search |

**Similarity**
| Method | Description |
|--------|-------------|
| `node_similarity(n1, n2, threshold, k)` | Jaccard similarity |
| `knn(node, k)` | K-nearest neighbors |
| `triangle_count()` | Triangle counts and clustering coefficients |

#### Batch Operations

```rust
// Batch insert nodes (upsert semantics)
g.upsert_nodes_batch([
    ("n1", [("name", "Node1")], "Type"),
    ("n2", [("name", "Node2")], "Type"),
])?;

// Batch insert edges (upsert semantics)
g.upsert_edges_batch([
    ("n1", "n2", [("weight", "1.0")], "CONNECTS"),
])?;
```

#### Bulk Insert (High Performance)

For maximum throughput when building graphs from external data, use the bulk insert methods.
These bypass Cypher parsing entirely and use direct SQL, achieving **100-500x faster** insert rates.

```rust
// Bulk insert nodes - returns HashMap<external_id, internal_rowid>
let id_map = g.insert_nodes_bulk([
    ("alice", vec![("name", "Alice"), ("age", "30")], "Person"),
    ("bob", vec![("name", "Bob"), ("age", "25")], "Person"),
    ("charlie", vec![("name", "Charlie")], "Person"),
])?;

// Bulk insert edges using the ID map - no MATCH queries needed!
let edges_inserted = g.insert_edges_bulk(
    [
        ("alice", "bob", vec![("since", "2020")], "KNOWS"),
        ("bob", "charlie", vec![("since", "2021")], "KNOWS"),
    ],
    &id_map,
)?;

// Or use the convenience method for both
let result = g.insert_graph_bulk(nodes, edges)?;
println!("Inserted {} nodes, {} edges", result.nodes_inserted, result.edges_inserted);

// Resolve existing node IDs (for edges to pre-existing nodes)
let resolved = g.resolve_node_ids(["alice", "bob"])?;
```

| Method | Description |
|--------|-------------|
| `insert_nodes_bulk(nodes)` | Insert nodes, returns ID mapping |
| `insert_edges_bulk(edges, id_map)` | Insert edges using ID map |
| `insert_graph_bulk(nodes, edges)` | Insert both, returns `BulkInsertResult` |
| `resolve_node_ids(ids)` | Resolve external IDs to internal rowids |

### Connection

```rust
use graphqlite::Connection;

let conn = Connection::open("graph.db")?;
let conn = Connection::open(":memory:")?;
let conn = Connection::open_in_memory()?;

// Wrap existing rusqlite connection
let sqlite_conn = rusqlite::Connection::open_in_memory()?;
let conn = Connection::from_rusqlite(sqlite_conn)?;
```

#### Methods

| Method | Description |
|--------|-------------|
| `cypher(query)` | Execute Cypher query, return results |
| `execute(sql)` | Execute raw SQL |
| `sqlite_connection()` | Access underlying rusqlite connection |

### CypherResult

```rust
let results = conn.cypher("MATCH (n) RETURN n.name")?;

results.len();           // Number of rows
results.is_empty();      // Check if empty
results.columns();       // Column names
results[0];              // First row

for row in &results {
    let name: String = row.get("n.name")?;
}
```

### Row

Type-safe value extraction:

```rust
let name: String = row.get("name")?;
let age: i64 = row.get("age")?;
let score: f64 = row.get("score")?;
let active: bool = row.get("active")?;
let maybe: Option<String> = row.get("optional")?;
```

### Utility Functions

```rust
use graphqlite::{escape_string, sanitize_rel_type, CYPHER_RESERVED};

// Escape strings for Cypher queries
let safe = escape_string("it's");  // "it\\'s"

// Sanitize relationship types
let rel = sanitize_rel_type("has-items");  // "has_items"
let rel = sanitize_rel_type("CREATE");     // "REL_CREATE"

// Check reserved keywords
if CYPHER_RESERVED.contains(&"MATCH") {
    println!("MATCH is reserved");
}
```

## Extension Path

The extension is located automatically. To specify a custom path:

```rust
let conn = Connection::open_with_extension("graph.db", "/path/to/graphqlite.dylib")?;
```

Or set the `GRAPHQLITE_EXTENSION_PATH` environment variable.

## License

MIT
