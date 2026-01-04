# Working with Multiple Graphs

GraphQLite supports managing and querying across multiple graph databases. This is useful for:

- **Separation of concerns**: Keep different data domains in separate graphs
- **Access control**: Different graphs can have different permissions
- **Performance**: Smaller, focused graphs can be faster to query
- **Cross-domain queries**: Query relationships across different datasets

## Using GraphManager (Python)

The `GraphManager` class manages multiple graph databases in a directory:

```python
from graphqlite import graphs

# Create a manager for a directory
with graphs("./data") as gm:
    # Create graphs
    social = gm.create("social")
    products = gm.create("products")

    # Add data to each graph
    social.upsert_node("alice", {"name": "Alice", "age": 30}, "Person")
    social.upsert_node("bob", {"name": "Bob", "age": 25}, "Person")
    social.upsert_edge("alice", "bob", {"since": 2020}, "KNOWS")

    products.upsert_node("phone", {"name": "iPhone", "price": 999}, "Product")
    products.upsert_node("laptop", {"name": "MacBook", "price": 1999}, "Product")

    # List all graphs
    print(gm.list())  # ['products', 'social']

    # Check if a graph exists
    if "social" in gm:
        print("Social graph exists")
```

### Opening Existing Graphs

```python
from graphqlite import graphs

with graphs("./data") as gm:
    # Open an existing graph
    social = gm.open("social")

    # Or create if it doesn't exist
    cache = gm.open_or_create("cache")

    # Query the graph
    result = social.query("MATCH (n:Person) RETURN n.name")
    for row in result:
        print(row["n.name"])
```

### Dropping Graphs

```python
from graphqlite import graphs

with graphs("./data") as gm:
    # Delete a graph and its file
    gm.drop("cache")
```

## Cross-Graph Queries

GraphQLite supports querying across multiple graphs using the `FROM` clause:

```python
from graphqlite import graphs

with graphs("./data") as gm:
    # Create and populate graphs
    social = gm.create("social")
    social.upsert_node("alice", {"name": "Alice", "user_id": "u1"}, "Person")

    purchases = gm.create("purchases")
    purchases.upsert_node("order1", {"user_id": "u1", "total": 99.99}, "Order")

    # Cross-graph query using FROM clause
    result = gm.query(
        """
        MATCH (p:Person) FROM social
        WHERE p.user_id = 'u1'
        RETURN p.name, graph(p) AS source
        """,
        graphs=["social"]
    )

    for row in result:
        print(f"{row['p.name']} from {row['source']}")
```

### The `graph()` Function

Use the `graph()` function to identify which graph a node comes from:

```python
result = gm.query(
    "MATCH (n:Person) FROM social RETURN n.name, graph(n) AS source_graph",
    graphs=["social"]
)
```

### Raw SQL Cross-Graph Queries

For advanced use cases, you can execute raw SQL across attached graphs:

```python
result = gm.query_sql(
    "SELECT COUNT(*) FROM social.nodes",
    graphs=["social"]
)
print(f"Node count: {result[0][0]}")
```

## Using GraphManager (Rust)

The Rust API provides similar functionality:

```rust
use graphqlite::{graphs, GraphManager};

fn main() -> graphqlite::Result<()> {
    let mut gm = graphs("./data")?;

    // Create graphs
    gm.create("social")?;
    gm.create("products")?;

    // List graphs
    for name in gm.list()? {
        println!("Graph: {}", name);
    }

    // Open and use a graph
    let social = gm.open_graph("social")?;
    social.query("CREATE (n:Person {name: 'Alice'})")?;

    // Cross-graph query
    let result = gm.query(
        "MATCH (n:Person) FROM social RETURN n.name",
        &["social"]
    )?;

    for row in &result {
        println!("{}", row.get::<String>("n.name")?);
    }

    // Drop a graph
    gm.drop("products")?;

    Ok(())
}
```

## Direct SQL with ATTACH

You can also work with multiple graphs directly using SQLite's ATTACH:

```python
import sqlite3
import graphqlite

# Create separate graph databases
conn1 = sqlite3.connect("social.db")
graphqlite.load(conn1)
conn1.execute("SELECT cypher('CREATE (n:Person {name: \"Alice\"})')")
conn1.close()

conn2 = sqlite3.connect("products.db")
graphqlite.load(conn2)
conn2.execute("SELECT cypher('CREATE (n:Product {name: \"Phone\"})')")
conn2.close()

# Query across both
coordinator = sqlite3.connect(":memory:")
graphqlite.load(coordinator)
coordinator.execute("ATTACH DATABASE 'social.db' AS social")
coordinator.execute("ATTACH DATABASE 'products.db' AS products")

result = coordinator.execute(
    "SELECT cypher('MATCH (n:Person) FROM social RETURN n.name')"
).fetchone()
print(result[0])
```

## Best Practices

1. **Use GraphManager for convenience**: It handles extension loading, connection caching, and cleanup automatically.

2. **Commit before cross-graph queries**: GraphManager automatically commits open graph connections before cross-graph queries to ensure data visibility.

3. **Keep graphs focused**: Design your graphs around specific domains or use cases for better performance and maintainability.

4. **Use meaningful names**: Graph names become SQLite database aliases, so use valid SQL identifiers.

5. **Handle errors gracefully**: Check for `FileNotFoundError` when opening graphs that might not exist.

## Limitations

- Cross-graph queries are read-only for the attached graphs
- The `FROM` clause only applies to `MATCH` patterns
- Graph names must be valid SQL identifiers (alphanumeric, underscores)
- Maximum of ~10 attached databases (SQLite limit)
