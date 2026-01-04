# Rust API Reference

## Installation

Add to your `Cargo.toml`:

```toml
[dependencies]
graphqlite = "0.1"
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

The `Error` enum includes variants for Cypher errors, SQLite errors, JSON parsing errors, and type conversion errors.

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
