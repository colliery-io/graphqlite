# Getting Started with Python

This tutorial walks you through installing GraphQLite and building a small social network graph from scratch. By the end you will know how to create nodes and relationships, query the graph with Cypher, explore the graph using built-in API methods, run a graph algorithm, and persist your work to a file.

## What You Will Learn

- Install GraphQLite for Python
- Create nodes and relationships using the `Graph` class
- Inspect graph statistics and explore connections
- Run Cypher queries with parameters
- Compute PageRank to find influential nodes
- Save the graph to a file and reopen it

## Prerequisites

- Python 3.8 or later
- `pip` package manager

## Step 1: Install GraphQLite

```bash
pip install graphqlite
```

Verify the installation:

```python
import graphqlite
print(graphqlite.__version__)  # 0.4.4
```

## Step 2: Create an In-Memory Graph

```python
from graphqlite import Graph

# ':memory:' creates a temporary in-memory database
g = Graph(":memory:")
print(g.stats())
# {'nodes': 0, 'edges': 0}
```

The `Graph` class is the high-level API. It manages the SQLite connection, loads the extension, and initialises the schema for you.

## Step 3: Add Person Nodes

Add three people. Each node has a unique string ID, a dictionary of properties, and an optional label.

```python
g.upsert_node("alice", {"name": "Alice", "age": 30, "city": "London"},  label="Person")
g.upsert_node("bob",   {"name": "Bob",   "age": 25, "city": "Paris"},   label="Person")
g.upsert_node("carol", {"name": "Carol", "age": 35, "city": "London"},  label="Person")

print(g.stats())
# {'nodes': 3, 'edges': 0}
```

`upsert_node` creates the node if the ID is new, or updates its properties if it already exists (merge semantics). This makes it safe to call repeatedly.

Verify a node was stored:

```python
node = g.get_node("alice")
print(node)
# {'id': 'alice', 'label': 'Person', 'properties': {'name': 'Alice', 'age': 30, 'city': 'London'}}
```

## Step 4: Add KNOWS Relationships

Connect the people:

```python
g.upsert_edge("alice", "bob",   {"since": 2020, "strength": "close"}, rel_type="KNOWS")
g.upsert_edge("alice", "carol", {"since": 2018, "strength": "close"}, rel_type="KNOWS")
g.upsert_edge("bob",   "carol", {"since": 2021, "strength": "casual"}, rel_type="KNOWS")

print(g.stats())
# {'nodes': 3, 'edges': 3}
```

Relationships are directed. `alice -> bob` is not the same as `bob -> alice`.

## Step 5: Inspect the Graph

Use built-in methods to explore the graph without writing Cypher:

```python
# Check if specific connections exist
print(g.has_edge("alice", "bob"))    # True
print(g.has_edge("bob",   "alice"))  # False (directed)

# Get Alice's outgoing neighbors
neighbors = g.get_neighbors("alice")
print([n["id"] for n in neighbors])
# ['bob', 'carol']

# Get degree (total connected edges, both directions)
print(g.node_degree("alice"))  # 2
print(g.node_degree("carol"))  # 2  (one in, one out)
```

List all nodes filtered by label:

```python
people = g.get_all_nodes(label="Person")
for p in people:
    print(p["id"], p["properties"]["name"])
# alice Alice
# bob Bob
# carol Carol
```

## Step 6: Query with Cypher

The `query()` method runs a Cypher string and returns a list of dictionaries. For queries without user-supplied data, this is convenient:

```python
results = g.query("""
    MATCH (a:Person)-[:KNOWS]->(b:Person)
    RETURN a.name AS from, b.name AS to, a.city AS city
    ORDER BY a.name
""")

for row in results:
    print(f"{row['from']} ({row['city']}) knows {row['to']}")
# Alice (London) knows Bob
# Alice (London) knows Carol
# Bob (Paris) knows Carol
```

## Step 7: Parameterized Queries

When any part of the query comes from user input, use parameterized queries. Access the underlying `Connection` object via `g.connection`:

```python
# Find everyone in a specific city — city name comes from user input
city = "London"
results = g.connection.cypher(
    "MATCH (p:Person {city: $city}) RETURN p.name AS name, p.age AS age ORDER BY p.age",
    {"city": city}
)

for row in results:
    print(f"{row['name']}, age {row['age']}")
# Alice, age 30
# Carol, age 35
```

Parameters are passed as a Python dictionary and serialised to JSON internally. This protects against injection and handles special characters cleanly. See the [parameterized queries guide](../how-to/parameterized-queries.md) for more detail.

Multi-hop traversal using parameters:

```python
# Who does a given person know transitively (up to 2 hops)?
results = g.connection.cypher(
    """
    MATCH (start:Person {name: $name})-[:KNOWS*1..2]->(other:Person)
    RETURN DISTINCT other.name AS name
    """,
    {"name": "Alice"}
)

print([r["name"] for r in results])
# ['Bob', 'Carol']
```

## Step 8: Run PageRank

Load the graph into the algorithm cache, then run PageRank. PageRank scores nodes by how many high-scoring nodes point to them.

```python
# Load the graph cache (required before algorithms)
g.connection.cypher("RETURN gql_load_graph()")

results = g.pagerank(damping=0.85, iterations=20)

print("PageRank scores:")
for r in sorted(results, key=lambda x: x["score"], reverse=True):
    print(f"  {r['user_id']}: {r['score']:.4f}")
# PageRank scores:
#   carol: 0.2282
#   bob:   0.1847
#   alice: 0.1471
```

Carol scores highest because two people point to her. See [Graph Analytics](./graph-analytics.md) for a full walkthrough of all 15+ algorithms.

## Step 9: Persist to a File

Switching from `:memory:` to a file path makes the graph persistent:

```python
# Save to a file
g_file = Graph("social.db")

g_file.upsert_node("alice", {"name": "Alice", "age": 30, "city": "London"}, label="Person")
g_file.upsert_node("bob",   {"name": "Bob",   "age": 25, "city": "Paris"},  label="Person")
g_file.upsert_node("carol", {"name": "Carol", "age": 35, "city": "London"}, label="Person")
g_file.upsert_edge("alice", "bob",   {"since": 2020}, rel_type="KNOWS")
g_file.upsert_edge("alice", "carol", {"since": 2018}, rel_type="KNOWS")
g_file.upsert_edge("bob",   "carol", {"since": 2021}, rel_type="KNOWS")

print(f"Saved: {g_file.stats()}")
# Saved: {'nodes': 3, 'edges': 3}
```

Reopen it later:

```python
g_reopen = Graph("social.db")
print(g_reopen.stats())
# {'nodes': 3, 'edges': 3}

node = g_reopen.get_node("alice")
print(node["properties"]["name"])
# Alice
```

The database is a standard SQLite file. You can inspect it with any SQLite tool.

## Summary

In this tutorial you:

1. Installed GraphQLite with `pip install graphqlite`
2. Created an in-memory graph using the `Graph` class
3. Added Person nodes with `upsert_node()`
4. Added KNOWS relationships with `upsert_edge()`
5. Explored the graph with `get_neighbors()`, `node_degree()`, and `stats()`
6. Queried with Cypher via `g.query()` and `g.connection.cypher()`
7. Used parameterized queries to safely handle user input
8. Ran PageRank with `g.pagerank()`
9. Persisted the graph to a `.db` file

## Next Steps

- [Building a Knowledge Graph](./knowledge-graph.md) — A more complex domain with multiple node and relationship types
- [Graph Analytics](./graph-analytics.md) — All 15+ algorithms with worked examples
- [Query Patterns (SQL)](./sql-patterns.md) — Advanced Cypher patterns: UNWIND, WITH pipelines, CASE, UNION
- [Python API Reference](../reference/python-api.md) — Complete method documentation for `Graph`, `Connection`, and `GraphManager`
- [Parameterized Queries Guide](../how-to/parameterized-queries.md) — Best practices for safe query construction
