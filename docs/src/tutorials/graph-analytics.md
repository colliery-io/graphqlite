# Graph Analytics

This tutorial shows how to use GraphQLite's built-in graph algorithms for analysis.

## What You'll Learn

- Run centrality algorithms to find important nodes
- Detect communities in your graph
- Find shortest paths between nodes
- Use algorithm results in your applications

## Setup: Create a Social Network

```python
from graphqlite import Graph

g = Graph(":memory:")

# Create a small social network
people = ["alice", "bob", "carol", "dave", "eve", "frank", "grace", "henry"]
for person in people:
    g.upsert_node(person, {"name": person.title()}, label="Person")

# Create connections (who follows whom)
connections = [
    ("alice", "bob"), ("alice", "carol"), ("alice", "dave"),
    ("bob", "carol"), ("bob", "eve"),
    ("carol", "dave"), ("carol", "eve"), ("carol", "frank"),
    ("dave", "frank"),
    ("eve", "frank"), ("eve", "grace"),
    ("frank", "grace"), ("frank", "henry"),
    ("grace", "henry"),
]
for source, target in connections:
    g.upsert_edge(source, target, {}, rel_type="FOLLOWS")

print(g.stats())  # {'nodes': 8, 'edges': 14}
```

## Centrality: Finding Important Nodes

### PageRank

PageRank identifies nodes that are linked to by other important nodes:

```python
results = g.pagerank(damping=0.85, iterations=20)
for r in sorted(results, key=lambda x: x["score"], reverse=True)[:3]:
    print(f"{r['user_id']}: {r['score']:.4f}")
```

Output:
```
frank: 0.1842
grace: 0.1536
eve: 0.1298
```

Frank is the most "important" because many well-connected people follow him.

### Degree Centrality

Count incoming and outgoing connections:

```python
results = g.degree_centrality()
for r in results:
    print(f"{r['user_id']}: in={r['in_degree']}, out={r['out_degree']}")
```

### Betweenness Centrality

Find nodes that act as bridges between communities:

```python
results = g.query("RETURN betweennessCentrality()")
# Carol and Eve have high betweenness - they connect different groups
```

## Community Detection

### Label Propagation

Find clusters of densely connected nodes:

```python
results = g.community_detection(max_iterations=10)
communities = {}
for r in results:
    label = r["community"]
    if label not in communities:
        communities[label] = []
    communities[label].append(r["user_id"])

for label, members in communities.items():
    print(f"Community {label}: {members}")
```

### Louvain Algorithm

For larger graphs, Louvain provides hierarchical community detection:

```python
results = g.query("RETURN louvain(1.0)")
```

## Path Finding

### Shortest Path

Find the shortest path between two nodes:

```python
path = g.shortest_path("alice", "henry")
print(f"Distance: {path['distance']}")
print(f"Path: {' -> '.join(path['path'])}")
```

Output:
```
Distance: 4
Path: alice -> carol -> frank -> henry
```

### All-Pairs Shortest Paths

Compute distances between all node pairs:

```python
results = g.query("RETURN apsp()")
```

## Connected Components

### Weakly Connected Components

Find groups of nodes that are connected (ignoring edge direction):

```python
results = g.connected_components()
```

### Strongly Connected Components

Find groups where every node can reach every other node:

```python
results = g.query("RETURN scc()")
```

## Using Results in Your Application

Algorithm results are returned as lists of dictionaries, making them easy to process:

```python
# Find the top influencers
influencers = g.pagerank()
top_3 = sorted(influencers, key=lambda x: x["score"], reverse=True)[:3]

# Get full node data for top influencers
for inf in top_3:
    node = g.get_node(inf["user_id"])
    print(f"{node['properties']['name']}: PageRank {inf['score']:.4f}")
```

## Combining Algorithms with Cypher

Use algorithm results to guide Cypher queries:

```python
# Find the most central node
pagerank = g.pagerank()
most_central = max(pagerank, key=lambda x: x["score"])["user_id"]

# Query their connections
results = g.query(f"""
    MATCH (p:Person {{name: '{most_central.title()}'}})-[:FOLLOWS]->(friend)
    RETURN friend.name AS friend
""")
print(f"Top influencer {most_central} follows: {[r['friend'] for r in results]}")
```

## Next Steps

- [Graph Algorithms Reference](../reference/algorithms.md) - Complete algorithm documentation
- [Performance](../explanation/performance.md) - Algorithm performance characteristics
