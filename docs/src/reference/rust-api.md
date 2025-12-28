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

### By Index

```rust
let rows = conn.cypher("MATCH (n) RETURN n.name, n.age")?;
for row in rows {
    let name: String = row.get(0)?;
    let age: i32 = row.get(1)?;
}
```

### By Column Name

```rust
let rows = conn.cypher("MATCH (n) RETURN n.name AS name, n.age AS age")?;
for row in rows {
    let name: String = row.get_by_name("name")?;
    let age: i32 = row.get_by_name("age")?;
}
```

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
        Err(Error::CypherError(msg)) => {
            eprintln!("Query error: {}", msg);
        }
        Err(e) => {
            eprintln!("Other error: {}", e);
        }
    }

    Ok(())
}
```

## Complete Example

```rust
use graphqlite::Connection;
use serde_json::json;

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

    // Query
    let rows = conn.cypher("
        MATCH (a:Person)-[:KNOWS]->(b:Person)
        RETURN a.name AS from, b.name AS to
    ")?;

    for row in rows {
        let from: String = row.get_by_name("from")?;
        let to: String = row.get_by_name("to")?;
        println!("{} knows {}", from, to);
    }

    // Query with filter
    let min_age = 26;
    let rows = conn.cypher(&format!(
        "MATCH (n:Person) WHERE n.age >= {} RETURN n.name",
        min_age
    ))?;

    for row in &rows {
        let name: String = row.get(0)?;
        println!("Adult: {}", name);
    }

    Ok(())
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
