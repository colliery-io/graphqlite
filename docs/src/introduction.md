# GraphQLite

GraphQLite is an SQLite extension that adds graph database capabilities using the Cypher query language.

Store and query graph data directly in SQLite—combining the simplicity of a single-file, zero-config embedded database with Cypher's expressive power for modeling relationships. Perfect for applications that need graph queries without a separate database server, or for local development and learning without standing up additional infrastructure.

## Key Features

- **Cypher query language** - Use the industry-standard graph query language
- **Zero configuration** - Works with any SQLite database
- **Embedded** - No separate server process required
- **15+ graph algorithms** - PageRank, shortest paths, community detection, and more
- **Multiple bindings** - Python, Rust, and raw SQL interfaces

## Quick Example

```python
from graphqlite import Graph

g = Graph(":memory:")
g.upsert_node("alice", {"name": "Alice", "age": 30}, label="Person")
g.upsert_node("bob", {"name": "Bob", "age": 25}, label="Person")
g.upsert_edge("alice", "bob", {"since": 2020}, rel_type="KNOWS")

results = g.query("MATCH (a:Person)-[:KNOWS]->(b) RETURN a.name, b.name")
```

## How This Documentation is Organized

This documentation follows the [Diátaxis](https://diataxis.fr/) framework:

- **[Tutorials](./tutorials/getting-started.md)** - Step-by-step lessons to get you started
- **[How-to Guides](./how-to/installation.md)** - Practical guides for specific tasks
- **[Reference](./reference/cypher.md)** - Technical descriptions of Cypher support, APIs, and algorithms
- **[Explanation](./explanation/architecture.md)** - Background and design decisions
