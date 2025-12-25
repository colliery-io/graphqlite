# GraphQLite Rust

Rust bindings for GraphQLite, a SQLite extension that adds graph database capabilities using Cypher.

## Installation

Add to your `Cargo.toml`:

```toml
[dependencies]
graphqlite = "0.1.0-beta"
```

## Quick Start

```rust
use graphqlite::Connection;

fn main() -> graphqlite::Result<()> {
    // Open a database
    let conn = Connection::open("graph.db")?;

    // Create nodes
    conn.cypher("CREATE (a:Person {name: 'Alice', age: 30})")?;
    conn.cypher("CREATE (b:Person {name: 'Bob', age: 25})")?;

    // Create relationships
    conn.cypher(r#"
        MATCH (a:Person {name: 'Alice'}), (b:Person {name: 'Bob'})
        CREATE (a)-[:KNOWS]->(b)
    "#)?;

    // Query the graph
    let results = conn.cypher("MATCH (a:Person)-[:KNOWS]->(b) RETURN a.name, b.name")?;
    for row in &results {
        let a: String = row.get("a.name")?;
        let b: String = row.get("b.name")?;
        println!("{} knows {}", a, b);
    }

    Ok(())
}
```

## API

### `Connection::open(path)`

Open a database file or in-memory database.

```rust
let conn = Connection::open("graph.db")?;
let conn = Connection::open(":memory:")?;
let conn = Connection::open_in_memory()?;
```

### `Connection::from_rusqlite(conn)`

Wrap an existing rusqlite connection.

```rust
let sqlite_conn = rusqlite::Connection::open_in_memory()?;
let conn = Connection::from_rusqlite(sqlite_conn)?;
```

### `Connection::cypher(query)`

Execute a Cypher query.

```rust
let results = conn.cypher("MATCH (n:Person) RETURN n.name, n.age")?;
```

### `CypherResult`

Query results support iteration and indexing:

```rust
// Iterate
for row in &results {
    let name: String = row.get("n.name")?;
}

// Index access
let first = &results[0];

// Properties
results.len();
results.is_empty();
results.columns();
```

### `Row::get<T>(column)`

Type-safe value extraction:

```rust
let name: String = row.get("name")?;
let age: i64 = row.get("age")?;
let score: f64 = row.get("score")?;
let active: bool = row.get("active")?;
let maybe: Option<String> = row.get("optional")?;
```

## Extension Path

The extension is located automatically. To specify a custom path:

```rust
let conn = Connection::open_with_extension("graph.db", "/path/to/graphqlite.dylib")?;
```

Or set the `GRAPHQLITE_EXTENSION_PATH` environment variable.

## License

MIT
