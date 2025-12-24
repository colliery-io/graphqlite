# GraphQLite

A SQLite extension that adds graph database capabilities using the Cypher query language.

GraphQLite lets you store and query graph data directly in SQLite. You get the simplicity of SQLite (single file, zero configuration, embedded) combined with the expressiveness of Cypher for modeling relationships. It's designed for applications that need graph queries without the overhead of a separate database server.

## Installation

Build from source (requires gcc, bison, flex):

```bash
make extension
```

This creates `build/graphqlite.dylib` (macOS) or `build/graphqlite.so` (Linux).

## Quick Start

```sql
.load build/graphqlite.dylib

-- Create nodes and relationships
SELECT cypher('CREATE (a:Person {name: "Alice", age: 30})');
SELECT cypher('CREATE (b:Person {name: "Bob", age: 25})');
SELECT cypher('
    MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"})
    CREATE (a)-[:KNOWS]->(b)
');

-- Query the graph
SELECT cypher('MATCH (a:Person)-[:KNOWS]->(b) RETURN a.name, b.name');
```

## Cypher Support

GraphQLite supports a substantial subset of Cypher:

**Patterns**: Nodes `(n:Label {prop: value})`, relationships `-[:TYPE]->`, variable-length paths `[*1..3]`, bidirectional matching.

**Clauses**: MATCH, OPTIONAL MATCH, CREATE, MERGE, SET, DELETE, WITH, UNWIND, RETURN (with ORDER BY, SKIP, LIMIT).

**Expressions**: Properties, aggregations (count, sum, min, max, avg, collect), list operations, CASE expressions, path functions, EXISTS predicates, list comprehensions (all, any, none, single, reduce).

## Graph Algorithms

Built-in algorithms return JSON that integrates with SQLite's json_each():

```sql
-- PageRank (damping factor, iterations)
SELECT cypher('RETURN pageRank(0.85, 20)');

-- Community detection via label propagation
SELECT cypher('RETURN labelPropagation(10)');

-- Query community membership
SELECT cypher('MATCH (n:Person) RETURN n.name, communityOf(n)');

-- Use results in SQL
SELECT json_extract(value, '$.node_id') as id,
       json_extract(value, '$.score') as score
FROM json_each(cypher('RETURN pageRank()'));
```

## Storage Model

GraphQLite uses a typed property graph model stored in regular SQLite tables. Nodes and edges are stored with typed property tables (text, integer, real, boolean), making it possible to query the underlying data with plain SQL when needed. The schema is created automatically when the extension loads.

## Language Bindings

### Python

```bash
pip install graphqlite
```

```python
from graphqlite import Connection

conn = Connection(":memory:")
conn.cypher("CREATE (n:Person {name: 'Alice'})")
for row in conn.cypher("MATCH (n:Person) RETURN n.name"):
    print(row[0])
```

### Rust

```toml
[dependencies]
graphqlite = "0.1"
```

```rust
use graphqlite::Connection;

let conn = Connection::open_in_memory()?;
conn.cypher("CREATE (n:Person {name: 'Alice'})")?;
for row in conn.cypher("MATCH (n:Person) RETURN n.name")? {
    println!("{}", row.get::<String>(0)?);
}
```

## Examples

The `examples/` directory contains tutorials organized by language:

```bash
# SQL tutorials
sqlite3 < examples/sql/01_getting_started.sql

# Python tutorial
python examples/python/knowledge_graph.py

# Rust tutorial
cd graphqlite-rs && cargo run --example knowledge_graph
```

## Testing

```bash
make test              # Run test suite
make test-functional   # SQL integration tests
make performance       # Performance benchmarks
```

## Performance

Tested on graphs up to 500K nodes / 5M edges. Simple traversals complete in 1-2ms, aggregations in under 500ms, and graph algorithms (PageRank, label propagation) in 1-5 seconds depending on iteration count.

---

[MIT License](LICENSE)
