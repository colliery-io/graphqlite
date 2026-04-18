# GraphQLite

GraphQLite is a SQLite extension that brings graph database capabilities to SQLite using the Cypher query language. Load it into any SQLite connection and immediately start creating nodes, traversing relationships, running graph algorithms, and expressing complex graph patterns — all without a separate database server, network configuration, or migration scripts. Your graph lives in a single `.db` file alongside the rest of your application data.

## Quick Example

**Python**

```python
from graphqlite import Graph

g = Graph(":memory:")

# Add nodes
g.upsert_node("alice", {"name": "Alice", "age": 30}, label="Person")
g.upsert_node("bob",   {"name": "Bob",   "age": 25}, label="Person")
g.upsert_node("carol", {"name": "Carol", "age": 35}, label="Person")

# Add relationships
g.upsert_edge("alice", "bob",   {"since": 2020}, rel_type="KNOWS")
g.upsert_edge("alice", "carol", {"since": 2018}, rel_type="KNOWS")

# Query with Cypher (parameterized)
results = g.connection.cypher(
    "MATCH (a:Person {name: $name})-[:KNOWS]->(friend) RETURN friend.name, friend.age",
    {"name": "Alice"}
)
for row in results:
    print(f"{row['friend.name']} — age {row['friend.age']}")
# Bob — age 25
# Carol — age 35

# Run a graph algorithm
for r in sorted(g.pagerank(), key=lambda x: x["score"], reverse=True):
    print(f"{r['user_id']}: {r['score']:.4f}")
```

**SQL (sqlite3 CLI)**

```sql
.load build/graphqlite

SELECT cypher('CREATE (a:Person {name: "Alice", age: 30})');
SELECT cypher('CREATE (b:Person {name: "Bob",   age: 25})');
SELECT cypher('
    MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"})
    CREATE (a)-[:KNOWS {since: 2020}]->(b)
');

-- Query
SELECT cypher('MATCH (a:Person)-[:KNOWS]->(b) RETURN a.name, b.name');

-- Run PageRank
SELECT
    json_extract(value, '$.user_id') AS person,
    printf('%.4f', json_extract(value, '$.score')) AS score
FROM json_each(cypher('RETURN pageRank(0.85, 20)'))
ORDER BY score DESC;
```

## Feature Highlights

| Feature | Details |
|---|---|
| **Cypher query language** | MATCH, CREATE, MERGE, SET, DELETE, WITH, UNWIND, FOREACH, UNION, LOAD CSV, OPTIONAL MATCH, variable-length paths, pattern predicates, and more |
| **15+ graph algorithms** | PageRank, degree/betweenness/closeness/eigenvector centrality, label propagation, Louvain, Dijkstra, A\*, APSP, BFS, DFS, WCC, SCC, node similarity, KNN, triangle count |
| **Three interfaces** | Python (`pip install graphqlite`), Rust (`graphqlite` crate), and raw SQL via the `cypher()` function |
| **Zero dependencies** | Only requires SQLite — no server, no daemon, no Docker |
| **Embedded operation** | Graphs live in `.db` files; no network, no port, no configuration |
| **Typed property storage** | EAV model with separate tables for text, integer, real, boolean, and JSON properties |
| **Parameterized queries** | First-class support for `$param` substitution — safe by design |
| **Transactions** | Full SQLite transaction support; reads and writes are ACID |

## How This Documentation Is Organized

This documentation follows the [Diátaxis](https://diataxis.fr/) framework:

- **[Tutorials](./tutorials/getting-started.md)** — Step-by-step lessons that build something real. Start here if you are new to GraphQLite.
  - [Getting Started (Python)](./tutorials/getting-started.md)
  - [Getting Started (SQL)](./tutorials/sql-getting-started.md)
  - [Query Patterns (SQL)](./tutorials/sql-patterns.md)
  - [Building a Knowledge Graph](./tutorials/knowledge-graph.md)
  - [Graph Analytics](./tutorials/graph-analytics.md)
  - [Graph Algorithms (SQL)](./tutorials/sql-algorithms.md)
  - [Building a GraphRAG System](./tutorials/graphrag.md)

- **[How-to Guides](./how-to/installation.md)** — Practical guides for specific tasks such as installation, multi-graph management, parameterized queries, and using GraphQLite alongside other SQLite extensions.

- **[Reference](./reference/cypher.md)** — Complete technical descriptions of supported Cypher syntax, built-in functions and operators, all 15+ graph algorithms, and the Python and Rust APIs.

- **[Explanation](./explanation/architecture.md)** — Background reading on architecture, the storage model, query dispatch, and performance characteristics.

## Version and License

Current version: **0.4.4** — MIT License

Source code and issue tracker: [https://github.com/colliery-io/graphqlite](https://github.com/colliery-io/graphqlite)
