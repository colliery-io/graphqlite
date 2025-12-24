# GraphQLite Python

Python bindings for GraphQLite, a SQLite extension that adds graph database capabilities using Cypher.

## Installation

```bash
pip install graphqlite
```

## Quick Start

```python
from graphqlite import connect

# Open a database
db = connect("graph.db")

# Create nodes
db.cypher("CREATE (a:Person {name: 'Alice', age: 30})")
db.cypher("CREATE (b:Person {name: 'Bob', age: 25})")

# Create relationships
db.cypher("""
    MATCH (a:Person {name: 'Alice'}), (b:Person {name: 'Bob'})
    CREATE (a)-[:KNOWS]->(b)
""")

# Query the graph
results = db.cypher("MATCH (a:Person)-[:KNOWS]->(b) RETURN a.name, b.name")
for row in results:
    print(f"{row['a.name']} knows {row['b.name']}")
```

## API

### `connect(database, extension_path=None)`

Open a new database connection with GraphQLite support.

```python
db = connect("graph.db")          # File database
db = connect(":memory:")          # In-memory database
```

### `wrap(conn)`

Wrap an existing SQLite connection.

```python
import sqlite3
conn = sqlite3.connect("graph.db")
db = wrap(conn)
```

### `Connection.cypher(query)`

Execute a Cypher query and return results.

```python
results = db.cypher("MATCH (n:Person) RETURN n.name, n.age")
for row in results:
    print(row["n.name"], row["n.age"])
```

### `CypherResult`

Results are returned as an iterable of dictionaries:

```python
results = db.cypher("MATCH (n) RETURN n.name")
len(results)           # Number of rows
results[0]             # First row as dict
results.columns        # Column names
results.to_list()      # All rows as list
```

## Extension Path

The extension is located automatically. To specify a custom path:

```python
db = connect("graph.db", extension_path="/path/to/graphqlite.dylib")
```

Or set the `GRAPHQLITE_EXTENSION_PATH` environment variable.

## License

MIT
