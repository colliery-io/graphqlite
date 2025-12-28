# Use with Other Extensions

GraphQLite works alongside other SQLite extensions. This guide shows how to combine them.

## Loading Multiple Extensions

### Method 1: Use graphqlite.load()

```python
import sqlite3
import graphqlite

conn = sqlite3.connect("combined.db")
graphqlite.load(conn)

# Now load other extensions
conn.enable_load_extension(True)
conn.load_extension("other_extension")
conn.enable_load_extension(False)
```

### Method 2: Access Underlying Connection

```python
import graphqlite
import sqlite_vec  # Example: vector search extension

db = graphqlite.connect("combined.db")
sqlite_vec.load(db.sqlite_connection)  # Access underlying sqlite3.Connection
```

## Example: GraphQLite + sqlite-vec

Combine graph queries with vector similarity search:

```python
import sqlite3
import graphqlite
import sqlite_vec

# Create connection and load both extensions
conn = sqlite3.connect("knowledge.db")
graphqlite.load(conn)
sqlite_vec.load(conn)

# Create graph nodes
conn.execute("SELECT cypher('CREATE (n:Document {id: \"doc1\", title: \"Introduction\"})')")

# Store embeddings in a vector table
conn.execute("""
    CREATE VIRTUAL TABLE IF NOT EXISTS embeddings USING vec0(
        doc_id TEXT PRIMARY KEY,
        embedding FLOAT[384]
    )
""")

# Query: find similar documents, then get their graph neighbors
similar_docs = conn.execute("""
    SELECT doc_id FROM embeddings
    WHERE embedding MATCH ?
    LIMIT 5
""", [query_embedding]).fetchall()

for (doc_id,) in similar_docs:
    # Get related nodes from graph
    related = conn.execute(f"""
        SELECT cypher('
            MATCH (d:Document {{id: "{doc_id}"}})-[:RELATED_TO]->(other)
            RETURN other.title
        ')
    """).fetchall()
```

## In-Memory Database Considerations

In-memory databases are connection-specific. All extensions must share the same connection:

```python
# Correct: single connection, multiple extensions
conn = sqlite3.connect(":memory:")
graphqlite.load(conn)
other_extension.load(conn)
# Both extensions share the same in-memory database

# Wrong: separate connections don't share data
conn1 = sqlite3.connect(":memory:")
conn2 = sqlite3.connect(":memory:")
# conn1 and conn2 are completely separate databases!
```

## Extension Loading Order

Generally, load GraphQLite first, then other extensions. This ensures the graph schema is created before any dependent operations.

```python
conn = sqlite3.connect("db.sqlite")

# 1. Load GraphQLite first
graphqlite.load(conn)

# 2. Load other extensions
conn.enable_load_extension(True)
conn.load_extension("extension2")
conn.load_extension("extension3")
conn.enable_load_extension(False)
```

## Troubleshooting

### Extension conflicts

If extensions conflict, try loading them in different orders or check for table name collisions.

### Missing tables

Ensure GraphQLite is loaded before querying graph tables. The schema is created on first load.

### Transaction issues

Some extensions may have different transaction semantics. If you encounter issues, try committing between operations:

```python
graphqlite.load(conn)
conn.commit()

other_extension.load(conn)
conn.commit()
```
