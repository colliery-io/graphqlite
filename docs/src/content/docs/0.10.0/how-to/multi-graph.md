---
title: Working with Multiple Graphs
slug: 0.10.0/how-to/multi-graph
---

GraphQLite supports managing and querying across multiple graph databases. This is useful for:

* **Separation of concerns**: Keep different data domains in separate graphs.
* **Access control**: Different graphs can have different file-level permissions.
* **Performance**: Smaller, focused graphs are faster to query and analyze.
* **Cross-domain queries**: Join relationships that span different datasets.

## Python: GraphManager

The `GraphManager` class (accessed via the `graphs()` factory function) manages multiple graph databases stored as separate SQLite files in a directory.

### Creating and Opening Graphs

```python
from graphqlite import graphs

with graphs("./data") as gm:
    # Create new graphs (raises FileExistsError if already exists)
    social = gm.create("social")
    products = gm.create("products")

    # Populate them
    social.upsert_node("alice", {"name": "Alice", "age": 30, "user_id": "u1"}, "Person")
    social.upsert_node("bob", {"name": "Bob", "age": 25, "user_id": "u2"}, "Person")
    social.upsert_edge("alice", "bob", {"since": 2020}, "KNOWS")

    products.upsert_node("phone", {"name": "iPhone 15", "price": 999}, "Product")
    products.upsert_node("laptop", {"name": "MacBook Pro", "price": 1999}, "Product")

    print(gm.list())       # ['products', 'social']
    print(len(gm))         # 2
    print("social" in gm)  # True
```

All connections are closed automatically when the `with` block exits.

### Opening Existing Graphs

```python
from graphqlite import graphs

with graphs("./data") as gm:
    # Open an existing graph (raises FileNotFoundError if missing)
    social = gm.open("social")

    # Open, creating if it doesn't exist
    cache = gm.open_or_create("cache")

    for row in social.query("MATCH (n:Person) RETURN n.name ORDER BY n.name"):
        print(row["n.name"])
```

### Listing and Dropping Graphs

```python
from graphqlite import graphs

with graphs("./data") as gm:
    # List all graph names in the directory
    for name in gm.list():
        g = gm.open(name)
        print(f"{name}: {g.stats()}")

    # Delete a graph and its database file permanently
    gm.drop("cache")
```

`drop()` deletes the `.db` file on disk. There is no undo.

## Cross-Graph Queries

GraphQLite can query across multiple graphs in a single Cypher statement using the `FROM` clause.

### The FROM Clause

Attach one or more graphs and reference them by name in `MATCH` patterns:

```python
from graphqlite import graphs

with graphs("./data") as gm:
    social = gm.open_or_create("social")
    social.upsert_node("alice", {"name": "Alice", "user_id": "u1"}, "Person")

    purchases = gm.open_or_create("purchases")
    purchases.upsert_node("order1", {"user_id": "u1", "total": 99.99, "item": "Phone"}, "Order")

    # GraphManager commits open graphs before running cross-graph queries
    result = gm.query(
        """
        MATCH (p:Person) FROM social
        WHERE p.user_id = 'u1'
        RETURN p.name, graph(p) AS source
        """,
        graphs=["social"]
    )

    for row in result:
        print(f"{row['p.name']} is from graph: {row['source']}")
```

The `graphs` parameter tells `gm.query()` which databases to attach before running the query.

### The graph() Function

`graph(node)` returns the name of the graph that the node comes from. Use it to identify results in multi-graph queries:

```python
result = gm.query(
    """
    MATCH (n) FROM social
    RETURN n.name, graph(n) AS source_graph
    """,
    graphs=["social"]
)

for row in result:
    print(f"{row['n.name']} lives in {row['source_graph']}")
```

### Cross-Graph Queries with Parameters

Pass parameters to cross-graph queries the same way as single-graph queries:

```python
result = gm.query(
    "MATCH (n:Person {user_id: $uid}) FROM social RETURN n.name",
    graphs=["social"],
    params={"uid": "u1"}
)
```

### Raw SQL Cross-Graph Queries

For low-level access, `query_sql()` attaches the named graphs and runs raw SQL. The attached graph's tables are prefixed with the graph name:

```python
# Count nodes in the social graph
result = gm.query_sql(
    "SELECT COUNT(*) AS node_count FROM social.nodes",
    graphs=["social"]
)
print(f"Social graph has {result[0][0]} nodes")

# Join across graphs with raw SQL
result = gm.query_sql(
    """
    SELECT s.user_id, COUNT(p.rowid) AS order_count
    FROM social.node_props_text s
    JOIN purchases.node_props_text p ON s.value = p.value
    WHERE s.key = 'user_id' AND p.key = 'user_id'
    GROUP BY s.user_id
    """,
    graphs=["social", "purchases"]
)
```

`query_sql()` is useful for analytics that go beyond what Cypher exposes, such as aggregations across multiple graph schemas at once.

### Important: Commit Before Cross-Graph Queries

`GraphManager` automatically commits all open graph connections before running cross-graph queries with `query()` or `query_sql()`. If you are using the underlying connections directly, commit first:

```python
social.connection.execute("COMMIT")
result = gm.query("MATCH (n) FROM social RETURN n", graphs=["social"])
```

## Rust: GraphManager

The Rust API mirrors the Python API closely.

```rust
use graphqlite::graphs;

fn main() -> graphqlite::Result<()> {
    let mut gm = graphs("./data")?;

    // Create graphs
    {
        let social = gm.create("social")?;
        social.query("CREATE (n:Person {name: 'Alice', user_id: 'u1'})")?;
        social.query("CREATE (n:Person {name: 'Bob', user_id: 'u2'})")?;
        social.query(
            "MATCH (a:Person {name: 'Alice'}), (b:Person {name: 'Bob'}) \
             CREATE (a)-[:KNOWS {since: 2020}]->(b)"
        )?;
    }

    {
        let products = gm.create("products")?;
        products.query("CREATE (n:Product {name: 'Phone', sku: 'p1', price: 999})")?;
    }

    // List all graphs
    for name in gm.list()? {
        println!("Graph: {}", name);
    }

    // Open an existing graph
    let social = gm.open_graph("social")?;
    let stats = social.stats()?;
    println!("Social: {} nodes, {} edges", stats.nodes, stats.edges);

    // Cross-graph query using FROM clause
    let result = gm.query(
        "MATCH (n:Person) FROM social RETURN n.name AS name ORDER BY n.name",
        &["social"],
    )?;

    for row in &result {
        println!("Person: {}", row.get::<String>("name")?);
    }

    // Raw SQL cross-graph query
    let counts = gm.query_sql(
        "SELECT COUNT(*) FROM social.nodes",
        &["social"],
    )?;

    // Open or create (idempotent)
    let _cache = gm.open_or_create("cache")?;

    // Drop a graph
    gm.drop("products")?;

    // Error handling for missing graphs
    match gm.open_graph("nonexistent") {
        Err(graphqlite::Error::GraphNotFound { name, available }) => {
            println!("'{}' not found. Available: {:?}", name, available);
        }
        _ => {}
    }

    Ok(())
}
```

### Rust GraphManager Methods

| Method | Description |
|--------|-------------|
| `gm.create("name")` | Create a new graph; errors if it already exists |
| `gm.open_graph("name")` | Open an existing graph; errors if missing |
| `gm.open_or_create("name")` | Open or create idempotently |
| `gm.list()?` | Returns `Vec<String>` of graph names |
| `gm.exists("name")` | Returns `bool` |
| `gm.drop("name")?` | Delete graph and its file |
| `gm.query(cypher, graphs)?` | Cross-graph Cypher query |
| `gm.query_sql(sql, graphs)?` | Cross-graph raw SQL query |

## Using ATTACH Directly

For complete control, attach databases manually using SQLite's `ATTACH` mechanism:

```python
import sqlite3
import graphqlite

# Build each graph
conn1 = sqlite3.connect("social.db")
graphqlite.load(conn1)
conn1.execute("SELECT cypher('CREATE (n:Person {name: \"Alice\"})')")
conn1.commit()
conn1.close()

conn2 = sqlite3.connect("products.db")
graphqlite.load(conn2)
conn2.execute("SELECT cypher('CREATE (n:Product {name: \"Phone\"})')")
conn2.commit()
conn2.close()

# Query across both
coordinator = sqlite3.connect(":memory:")
graphqlite.load(coordinator)
coordinator.execute("ATTACH DATABASE 'social.db' AS social")
coordinator.execute("ATTACH DATABASE 'products.db' AS products")

result = coordinator.execute(
    "SELECT cypher('MATCH (n:Person) FROM social RETURN n.name')"
).fetchall()
print(result)
```

## Best Practices

1. **Use the context manager.** `with graphs(...) as gm:` ensures all connections are closed and any pending transactions are flushed.

2. **Commit before cross-graph queries.** GraphManager handles this automatically, but manual connections do not.

3. **Use valid SQL identifiers for graph names.** Graph names become SQLite database aliases (`ATTACH ... AS name`). Use lowercase letters, digits, and underscores only — no hyphens or spaces.

4. **Keep graphs focused.** Design each graph around a single domain or service boundary. Cross-graph queries are read-only for the attached graphs.

5. **Use the same extension version.** All attached graphs should be queried with the same version of the GraphQLite extension to avoid schema incompatibilities.

## Limitations

* Cross-graph `FROM` clause queries are read-only for attached graphs.
* The `FROM` clause is only supported inside `MATCH` patterns.
* SQLite supports a maximum of approximately 10 simultaneously attached databases.
* Graph names must be valid SQL identifiers (alphanumeric and underscores).
