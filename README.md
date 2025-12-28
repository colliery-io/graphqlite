
# GraphQLite

<p align="center">
    <img src="docs/assets/logo.png" alt="GraphQLite" width="256">
</p>

An SQLite extension that adds graph database capabilities using the Cypher query language.

Store and query graph data directly in SQLite—combining the simplicity of a single-file, zero-config embedded database with Cypher's expressive power for modeling relationships.

## Installation

```bash
pip install graphqlite        # Python
cargo add graphqlite          # Rust
```

## Quick Start

```python
from graphqlite import Graph

g = Graph(":memory:")
g.upsert_node("alice", {"name": "Alice", "age": 30}, label="Person")
g.upsert_node("bob", {"name": "Bob", "age": 25}, label="Person")
g.upsert_edge("alice", "bob", {"since": 2020}, rel_type="KNOWS")

# Query with Cypher
results = g.query("MATCH (a:Person)-[:KNOWS]->(b) RETURN a.name, b.name")

# Built-in graph algorithms
g.pagerank()
g.louvain()
g.dijkstra("alice", "bob")
```

## Features

- **Cypher queries** — MATCH, CREATE, MERGE, SET, DELETE, WITH, UNWIND, RETURN
- **Graph algorithms** — PageRank, Louvain, Dijkstra, BFS/DFS, connected components, and more
- **Zero configuration** — Works with any SQLite database, no server required
- **Multiple bindings** — Python, Rust, and raw SQL interfaces

## Documentation

**[Full Documentation](https://colliery-io.github.io/graphqlite/)** — Tutorials, how-to guides, and API reference

## Examples

```bash
# SQL tutorials
sqlite3 < examples/sql/01_getting_started.sql

# GraphRAG with HotpotQA dataset
cd examples/llm-graphrag
uv sync && uv run python ingest.py
uv run python rag.py "Were Scott Derrickson and Ed Wood of the same nationality?"
```

## License

[MIT](LICENSE)
