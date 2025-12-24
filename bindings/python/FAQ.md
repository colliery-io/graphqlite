# GraphQLite Python FAQ

Common issues and solutions when using GraphQLite Python bindings.

## Cypher Query Issues

### Q: My nodes exist in the database but `MATCH (n) RETURN n` returns nothing

**Symptom**: You insert nodes and can verify they exist with raw SQL (`SELECT * FROM nodes`), but Cypher queries like `MATCH (n) RETURN n` return empty results.

**Cause**: Property values containing newlines (`\n`), carriage returns (`\r`), or tabs (`\t`) can break Cypher query parsing.

**Solution**: Escape or replace these characters before storing:

```python
def escape_for_cypher(value: str) -> str:
    """Escape a string for use in Cypher property values."""
    return (value
        .replace("\\", "\\\\")
        .replace("'", "\\'")
        .replace("\n", " ")
        .replace("\r", " ")
        .replace("\t", " "))
```

**Example**:
```python
# This will cause issues:
db.cypher("CREATE (n:Note {text: 'Line1\nLine2'})")

# Do this instead:
text = "Line1\nLine2".replace("\n", " ")
db.cypher(f"CREATE (n:Note {{text: '{text}'}})")
```

## Extension Loading

### Q: Extension not found error

**Symptom**: `FileNotFoundError: GraphQLite extension not found`

**Solutions**:

1. Build the extension first: `make extension RELEASE=1`
2. Set the path explicitly:
   ```python
   db = connect("graph.db", extension_path="/path/to/graphqlite.dylib")
   ```
3. Set environment variable:
   ```bash
   export GRAPHQLITE_EXTENSION_PATH=/path/to/graphqlite.dylib
   ```

### Q: Using with other SQLite extensions (sqlite-vec, etc.)

GraphQLite works alongside other SQLite extensions. Load GraphQLite first, then other extensions:

```python
import graphqlite
import sqlite_vec

# Method 1: Use graphqlite.connect(), then load other extensions
db = graphqlite.connect("graph.db")
sqlite_vec.load(db._conn)  # Access underlying sqlite3.Connection

# Method 2: Use graphqlite.load() on existing connection
import sqlite3
conn = sqlite3.connect("graph.db")
graphqlite.load(conn)
sqlite_vec.load(conn)
```

## In-Memory Databases

### Q: Data disappears when using `:memory:`

In-memory databases are connection-specific. If you're loading multiple extensions, ensure they all use the same connection object. Don't create new connections expecting to see data from another.

```python
# Correct: single connection, multiple extensions
conn = sqlite3.connect(":memory:")
graphqlite.load(conn)
sqlite_vec.load(conn)
# Both extensions share the same in-memory database

# Wrong: separate connections don't share data
conn1 = sqlite3.connect(":memory:")
conn2 = sqlite3.connect(":memory:")
# conn1 and conn2 are completely separate databases
```
