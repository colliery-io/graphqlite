---
title: Using GraphQLite with Other SQLite Extensions
slug: 0.10.0/how-to/other-extensions
---

GraphQLite is a standard SQLite extension and can share a connection with other SQLite extensions, including `sqlite-vec` (vector search), `sqlite-fts5` (full-text search), and others.

## Loading GraphQLite into an Existing Connection

If you already have a `sqlite3.Connection`, use `graphqlite.load()` to add GraphQLite functions to it:

```python
import sqlite3
import graphqlite

conn = sqlite3.connect("combined.db")
graphqlite.load(conn)

# Both the graph functions and the raw database are now available on conn
conn.execute("SELECT cypher('CREATE (n:Page {title: \"Home\"})')")
conn.execute("SELECT * FROM nodes")
```

## Wrapping an Existing Connection

`graphqlite.wrap()` loads GraphQLite into an existing `sqlite3.Connection` and returns a `Connection` object that exposes the `cypher()` method:

```python
import sqlite3
import graphqlite

raw_conn = sqlite3.connect("combined.db")
conn = graphqlite.wrap(raw_conn)

# Use the graphqlite Connection API
conn.cypher("CREATE (n:Page {title: 'Home'})")
results = conn.cypher("MATCH (n:Page) RETURN n.title")

# Access the underlying sqlite3.Connection for raw SQL
raw_conn.execute("SELECT COUNT(*) FROM nodes").fetchone()
```

## Loading Multiple Extensions

Load GraphQLite first (it creates the graph schema tables), then load other extensions:

```python
import sqlite3
import graphqlite

conn = sqlite3.connect("combined.db")

# 1. Load GraphQLite (creates schema tables)
graphqlite.load(conn)

# 2. Load other extensions
conn.enable_load_extension(True)
conn.load_extension("/path/to/other_extension")
conn.enable_load_extension(False)
```

This order matters: GraphQLite creates `nodes`, `edges`, and related tables on first load. If another extension has conflicting table names, loading GraphQLite first lets you detect the conflict early.

## Example: GraphQLite + sqlite-vec

Combine graph traversal with vector similarity search. This pattern is the foundation of GraphRAG systems: find semantically similar documents with vectors, then expand to related content via graph edges.

```python
import sqlite3
import graphqlite
import sqlite_vec
import json

# Create and configure the connection
conn = sqlite3.connect("knowledge.db")
graphqlite.load(conn)
sqlite_vec.load(conn)

# Create graph nodes (documents)
conn.execute("SELECT cypher('CREATE (n:Document {doc_id: \"doc1\", title: \"Introduction to Graphs\"})')")
conn.execute("SELECT cypher('CREATE (n:Document {doc_id: \"doc2\", title: \"Graph Algorithms\"})')")
conn.execute("SELECT cypher('CREATE (n:Document {doc_id: \"doc3\", title: \"Vector Search\"})')")

# Link related documents in the graph
conn.execute("""
    SELECT cypher('
        MATCH (a:Document {doc_id: "doc1"}), (b:Document {doc_id: "doc2"})
        CREATE (a)-[:RELATED_TO {strength: 0.9}]->(b)
    ')
""")
conn.execute("""
    SELECT cypher('
        MATCH (b:Document {doc_id: "doc2"}), (c:Document {doc_id: "doc3"})
        CREATE (b)-[:RELATED_TO {strength: 0.7}]->(c)
    ')
""")

# Create a vector table for document embeddings
conn.execute("""
    CREATE VIRTUAL TABLE IF NOT EXISTS doc_embeddings
    USING vec0(
        doc_id TEXT PRIMARY KEY,
        embedding FLOAT[4]
    )
""")

# Insert mock embeddings (replace with real model output)
embeddings = {
    "doc1": [0.1, 0.2, 0.9, 0.3],
    "doc2": [0.1, 0.3, 0.8, 0.4],
    "doc3": [0.9, 0.1, 0.1, 0.8],
}
for doc_id, emb in embeddings.items():
    conn.execute(
        "INSERT INTO doc_embeddings(doc_id, embedding) VALUES (?, ?)",
        [doc_id, json.dumps(emb)]
    )
conn.commit()

# --- Query: vector search + graph expansion ---

# Step 1: Find the most similar document to a query vector
query_embedding = json.dumps([0.1, 0.25, 0.85, 0.35])
similar = conn.execute("""
    SELECT doc_id
    FROM doc_embeddings
    WHERE embedding MATCH ?
      AND k = 2
    ORDER BY distance
""", [query_embedding]).fetchall()

print("Similar documents:", [row[0] for row in similar])

# Step 2: Expand to graph neighbors for each similar document
for (doc_id,) in similar:
    related = conn.execute(f"""
        SELECT cypher('
            MATCH (d:Document {{doc_id: "{doc_id}"}})-[:RELATED_TO]->(other:Document)
            RETURN other.doc_id AS id, other.title AS title
        ')
    """).fetchall()
    if related:
        print(f"  {doc_id} is related to:")
        for (row_json,) in related:
            row = json.loads(row_json)
            print(f"    - {row['id']}: {row['title']}")
```

## Sharing Connections: In-Memory Databases

In-memory SQLite databases are private to a single connection. All extensions that need to share data must use the same `conn` object:

```python
# Correct: one connection, multiple extensions
conn = sqlite3.connect(":memory:")
graphqlite.load(conn)
sqlite_vec.load(conn)
# Both extensions operate on the same in-memory database

# Wrong: two separate connections, two separate databases
conn1 = sqlite3.connect(":memory:")
conn2 = sqlite3.connect(":memory:")
# conn1 and conn2 cannot see each other's data
```

For file-based databases, this restriction does not apply — separate connections can open the same file, subject to SQLite locking rules.

## Extension Loading Order

In general:

1. **GraphQLite first.** It creates the graph schema (`nodes`, `edges`, property tables) on first load. Other extensions that reference these tables will find them already present.
2. **Other extensions next.** Load them with `enable_load_extension(True)` / `load_extension()` / `enable_load_extension(False)`.
3. **Commit between loads** if any extension creates tables, to ensure schema visibility:

   ```python
   graphqlite.load(conn)
   conn.commit()

   conn.enable_load_extension(True)
   conn.load_extension("my_extension")
   conn.enable_load_extension(False)
   conn.commit()
   ```

## Using GraphQLite with the Python Graph API Alongside Other Extensions

When you use `graphqlite.Graph` or `graphqlite.connect()`, access the underlying `sqlite3.Connection` to load additional extensions:

```python
import graphqlite
import sqlite_vec

g = graphqlite.Graph("knowledge.db")

# Access the raw sqlite3.Connection
raw_conn = g.connection.sqlite_connection
sqlite_vec.load(raw_conn)

# Now use both APIs on the same connection
g.upsert_node("doc1", {"title": "Introduction"}, "Document")
raw_conn.execute("CREATE VIRTUAL TABLE IF NOT EXISTS vecs USING vec0(embedding FLOAT[4])")
```

## Troubleshooting

### Extension conflicts

If two extensions register functions with the same name, the later-loaded extension wins. Check for collisions between GraphQLite's functions (`cypher`, `regexp`, `gql_load_graph`, etc.) and the functions registered by your other extension.

### Missing tables after loading

Ensure GraphQLite was loaded before any query that references graph tables. The schema is created lazily on first load; if the connection is closed and reopened, `graphqlite.load()` must be called again.

### Transaction isolation

Some extensions use their own transaction management. If you encounter "table is locked" errors, commit between extension operations:

```python
graphqlite.load(conn)
conn.execute("SELECT cypher('CREATE (n:Test {v: 1})')")
conn.commit()   # Flush GraphQLite writes

# Now safe to use the other extension
conn.execute("INSERT INTO vec_table VALUES (?)", ["..."])
conn.commit()
```
