# Getting Started

This tutorial walks you through installing GraphQLite and running your first graph queries.

## What You'll Learn

- Install GraphQLite for Python
- Create nodes and relationships
- Query the graph with Cypher
- Use the high-level Graph API

## Prerequisites

- Python 3.8 or later
- pip package manager

## Step 1: Install GraphQLite

```bash
pip install graphqlite
```

## Step 2: Create Your First Graph

Open a Python shell and create an in-memory graph:

```python
from graphqlite import Graph

# Create an in-memory graph database
g = Graph(":memory:")
```

## Step 3: Add Nodes

Add some people to your graph:

```python
g.upsert_node("alice", {"name": "Alice", "age": 30}, label="Person")
g.upsert_node("bob", {"name": "Bob", "age": 25}, label="Person")
g.upsert_node("carol", {"name": "Carol", "age": 35}, label="Person")

print(g.stats())  # {'nodes': 3, 'edges': 0}
```

Each node has:
- A unique ID (`"alice"`, `"bob"`, `"carol"`)
- Properties (key-value pairs like `name` and `age`)
- A label (`Person`)

## Step 4: Create Relationships

Connect the nodes with relationships:

```python
g.upsert_edge("alice", "bob", {"since": 2020}, rel_type="KNOWS")
g.upsert_edge("alice", "carol", {"since": 2018}, rel_type="KNOWS")
g.upsert_edge("bob", "carol", {"since": 2021}, rel_type="KNOWS")

print(g.stats())  # {'nodes': 3, 'edges': 3}
```

## Step 5: Query with Cypher

Find all people that Alice knows:

```python
results = g.query("""
    MATCH (a:Person {name: 'Alice'})-[:KNOWS]->(friend)
    RETURN friend.name AS name, friend.age AS age
""")

for row in results:
    print(f"{row['name']} is {row['age']} years old")
```

Output:
```
Bob is 25 years old
Carol is 35 years old
```

## Step 6: Explore the Graph

Use built-in methods to explore:

```python
# Get Alice's neighbors
neighbors = g.get_neighbors("alice")
print([n["id"] for n in neighbors])  # ['bob', 'carol']

# Check if an edge exists
print(g.has_edge("alice", "bob"))  # True
print(g.has_edge("bob", "alice"))  # False (directed edge)

# Get node degree
print(g.node_degree("alice"))  # {'in': 0, 'out': 2, 'total': 2}
```

## Next Steps

- [Building a Knowledge Graph](./knowledge-graph.md) - A more complex tutorial
- [Graph Analytics](./graph-analytics.md) - Use graph algorithms
- [Cypher Reference](../reference/cypher.md) - Full language reference
