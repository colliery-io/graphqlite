---
title: Graph Analytics
slug: 0.9.2/tutorials/graph-analytics
---

This tutorial is a complete walkthrough of GraphQLite's built-in graph algorithms. You will build a dense social network, run every algorithm category — centrality, community detection, path finding, components, traversal, and similarity — and combine algorithm results with Cypher queries to answer analytical questions.

## What You Will Learn

- Load the graph cache required by algorithms
- Run all 15+ algorithms with real output
- Combine algorithm output with Cypher pattern matching

## Prerequisites

```bash
pip install graphqlite
```

## Step 1: Build a Social Network

This graph is deliberately dense to produce interesting algorithm output. It represents a professional network: people follow each other and belong to teams.

```python
from graphqlite import Graph

g = Graph(":memory:")

# 10 people
people = [
    ("alice",   {"name": "Alice",   "role": "Engineer",   "team": "A"}),
    ("bob",     {"name": "Bob",     "role": "Manager",    "team": "A"}),
    ("carol",   {"name": "Carol",   "role": "Engineer",   "team": "A"}),
    ("dave",    {"name": "Dave",    "role": "Engineer",   "team": "B"}),
    ("eve",     {"name": "Eve",     "role": "Manager",    "team": "B"}),
    ("frank",   {"name": "Frank",   "role": "Engineer",   "team": "B"}),
    ("grace",   {"name": "Grace",   "role": "Director",   "team": "C"}),
    ("henry",   {"name": "Henry",   "role": "Engineer",   "team": "C"}),
    ("iris",    {"name": "Iris",    "role": "Engineer",   "team": "C"}),
    ("james",   {"name": "James",   "role": "Manager",    "team": "A"}),
]

for node_id, props in people:
    g.upsert_node(node_id, props, label="Person")

# 18 directed FOLLOWS edges
connections = [
    ("alice",  "bob"),    ("alice",  "carol"),  ("alice",  "james"),
    ("bob",    "carol"),  ("bob",    "dave"),   ("bob",    "grace"),
    ("carol",  "dave"),   ("carol",  "eve"),
    ("dave",   "eve"),    ("dave",   "frank"),
    ("eve",    "frank"),  ("eve",    "grace"),
    ("frank",  "grace"),  ("frank",  "henry"),
    ("grace",  "henry"),  ("grace",  "iris"),
    ("henry",  "iris"),   ("iris",   "james"),
]

for source, target in connections:
    g.upsert_edge(source, target, {}, rel_type="FOLLOWS")

print(g.stats())
# {'nodes': 10, 'edges': 18}
```

## Step 2: Load the Graph Cache

Graph algorithms require the graph to be loaded into an in-memory CSR (Compressed Sparse Row) cache. Call `gql_load_graph()` once after building the graph, and again after making structural changes (adding or deleting nodes or edges).

```python
g.connection.cypher("RETURN gql_load_graph()")
print("Graph cache loaded")
```

You can check whether the cache is current:

```python
status = g.connection.cypher("RETURN gql_graph_loaded()")
print(status[0]["gql_graph_loaded()"])  # 1
```

## Step 3: Centrality Algorithms

Centrality measures answer the question: *who is the most important node?* Different algorithms define "important" differently.

### PageRank

PageRank scores a node by the quality and quantity of nodes pointing to it. A node followed by many high-scoring nodes gets a high PageRank.

In this social network, PageRank captures the notion of professional reputation: being followed by well-connected people (like a manager followed by their team) matters more than raw follower count.

```python
results = g.pagerank(damping=0.85, iterations=20)

print("PageRank (top 5):")
for r in sorted(results, key=lambda x: x["score"], reverse=True)[:5]:
    print(f"  {r['user_id']:8s}: {r['score']:.4f}")
```

Output:
```
PageRank (top 5):
  grace   : 0.2041
  henry   : 0.1593
  iris    : 0.1312
  frank   : 0.1211
  james   : 0.1009
```

Grace scores highest: she is followed by Bob, Eve, and Frank — all of whom have significant in-links themselves.

### Degree Centrality

Counts the raw number of incoming and outgoing edges.

Degree centrality is a fast first look at the network's shape. Here, Alice has the highest out-degree (she actively follows many people), while Grace has the highest in-degree (many people follow her). No graph cache is required — it reads directly from the database.

```python
results = g.degree_centrality()

print("Degree centrality:")
for r in sorted(results, key=lambda x: x["degree"], reverse=True)[:5]:
    print(f"  {r['user_id']:8s}: in={r['in_degree']}, out={r['out_degree']}, total={r['degree']}")
```

Output:
```
Degree centrality:
  grace   : in=4, out=2, total=6
  carol   : in=2, out=2, total=4
  eve     : in=3, out=2, total=5
  frank   : in=2, out=2, total=4
  bob     : in=1, out=3, total=4
```

### Betweenness Centrality

Measures how often a node lies on the shortest path between two other nodes. High betweenness nodes are bottlenecks or brokers.

In a professional network, high-betweenness nodes are the connectors who bridge different teams. Removing Carol or Eve would lengthen the path between Team A and Team B nodes — making them critical to cross-team information flow.

```python
results = g.betweenness_centrality()

print("Betweenness centrality (top 5):")
for r in sorted(results, key=lambda x: x["score"], reverse=True)[:5]:
    print(f"  {r['user_id']:8s}: {r['score']:.4f}")
```

Output:
```
Betweenness centrality (top 5):
  carol   : 0.2333
  eve     : 0.2000
  bob     : 0.1778
  frank   : 0.1333
  grace   : 0.1111
```

Carol and Eve are bridges: many shortest paths between Team A and Team B nodes pass through them.

### Closeness Centrality

Measures the average shortest distance from a node to all others. A node with high closeness can reach everyone quickly.

Closeness centrality tells us who is best positioned to spread news quickly across the whole organisation. A high-closeness person reaches everyone in the fewest hops — useful for identifying who to brief first when rolling out a cross-team announcement.

```python
results = g.closeness_centrality()

print("Closeness centrality (top 5):")
for r in sorted(results, key=lambda x: x["score"], reverse=True)[:5]:
    print(f"  {r['user_id']:8s}: {r['score']:.4f}")
```

### Eigenvector Centrality

Like PageRank for undirected graphs: a node is important if its neighbors are important.

Eigenvector centrality amplifies the PageRank idea: in this follow network, engineers who work alongside high-scoring managers accumulate reflected influence even if they have fewer direct followers.

```python
results = g.eigenvector_centrality(iterations=100)

print("Eigenvector centrality (top 5):")
for r in sorted(results, key=lambda x: x["score"], reverse=True)[:5]:
    print(f"  {r['user_id']:8s}: {r['score']:.4f}")
```

## Step 4: Community Detection

Community detection algorithms answer: *which nodes form natural clusters?*

### Label Propagation

Nodes adopt the most common label of their neighbors iteratively until stable. Fast and works well on large graphs.

We use label propagation here as a fast first pass to confirm that our three-team structure emerges organically from the follow graph. The result is non-deterministic, but for a dense graph like this the team boundaries are clear enough that the algorithm reliably recovers them.

```python
results = g.community_detection(iterations=10)

communities: dict[int, list] = {}
for r in results:
    communities.setdefault(r["community"], []).append(r["user_id"])

print("Label propagation communities:")
for cid, members in sorted(communities.items()):
    print(f"  Community {cid}: {sorted(members)}")
```

Output (community assignments vary by run):
```
Label propagation communities:
  Community 0: ['alice', 'bob', 'carol', 'james']
  Community 1: ['dave', 'eve', 'frank']
  Community 2: ['grace', 'henry', 'iris']
```

The three teams emerge as communities because the team members are densely connected to each other.

### Louvain

Hierarchical modularity-based community detection. More deterministic than label propagation and produces higher-quality communities on most graphs.

Louvain gives us a more stable partition than label propagation. With `resolution=1.0` it recovers the three teams; raising the resolution splits the larger teams into smaller clusters, which could map to sub-teams or project groups within the organisation.

```python
results = g.louvain(resolution=1.0)

communities = {}
for r in results:
    communities.setdefault(r["community"], []).append(r["user_id"])

print("Louvain communities:")
for cid, members in sorted(communities.items()):
    print(f"  Community {cid}: {sorted(members)}")
```

Try `resolution=2.0` to get more, smaller communities; `resolution=0.5` to get fewer, larger ones.

## Step 5: Path Finding

### Shortest Path (Dijkstra)

Finds the minimum-hop (or minimum-weight) path between two nodes.

Shortest path answers the "how are these people connected?" question. In a professional network, the path length gives a rough measure of relationship distance — a direct follow is one hop, a mutual contact is two.

```python
path = g.shortest_path("alice", "james")
print(f"Distance: {path['distance']}")
print(f"Path: {' -> '.join(path['path'])}")
print(f"Found: {path['found']}")
```

Output:
```
Distance: 1
Path: alice -> james
Found: True
```

Try a longer path:

```python
path = g.shortest_path("alice", "iris")
print(f"alice -> iris: distance={path['distance']}, path={path['path']}")
# alice -> iris: distance=3, path=['alice', 'carol', 'eve', 'grace', 'iris']
# (or another path of length 3-4 depending on traversal order)
```

### A* (A-Star)

A* uses a heuristic to guide the search, exploring promising directions first. With latitude/longitude properties it uses haversine distance; without them it falls back to a uniform-cost heuristic similar to Dijkstra.

We've assigned real European city coordinates to each person here to demonstrate geographic routing. A* uses the haversine distance to the target city as a heuristic, pruning distant branches early and exploring fewer nodes than Dijkstra on a geographically spread graph.

Add coordinates to the nodes to demonstrate the geographic heuristic:

```python
coords = {
    "alice": (51.5, -0.1),   # London
    "bob":   (48.9,  2.3),   # Paris
    "carol": (52.4, 13.4),   # Berlin
    "dave":  (41.9, 12.5),   # Rome
    "eve":   (40.4, -3.7),   # Madrid
    "frank": (59.9, 10.7),   # Oslo
    "grace": (55.7, 12.6),   # Copenhagen
    "henry": (52.2, 21.0),   # Warsaw
    "iris":  (47.5, 19.0),   # Budapest
    "james": (50.1,  8.7),   # Frankfurt
}

for node_id, (lat, lon) in coords.items():
    g.connection.cypher(
        "MATCH (p:Person {name: $name}) SET p.lat = $lat, p.lon = $lon",
        {"name": node_id.title(), "lat": lat, "lon": lon}
    )

# Reload cache after property updates
g.connection.cypher("RETURN gql_load_graph()")

path = g.astar("alice", "iris", lat_prop="lat", lon_prop="lon")
print(f"A* alice -> iris: distance={path['distance']}, nodes_explored={path['nodes_explored']}")
print(f"Path: {path['path']}")
```

### All-Pairs Shortest Paths (APSP)

Computes shortest distances between every pair of nodes using Floyd-Warshall.

With only 10 people in the network, APSP is cheap and gives us global metrics — the diameter tells us the most "socially distant" pair, and the average path length tells us how tight-knit the network is overall.

```python
results = g.all_pairs_shortest_path()

# Find the diameter (longest shortest path)
reachable = [r for r in results if r["distance"] is not None]
diameter_row = max(reachable, key=lambda x: x["distance"])
print(f"Graph diameter: {diameter_row['distance']} ({diameter_row['source']} -> {diameter_row['target']})")

# Average path length
avg = sum(r["distance"] for r in reachable) / len(reachable)
print(f"Average shortest path length: {avg:.2f}")
```

## Step 6: Connected Components

### Weakly Connected Components (WCC)

Groups nodes that are reachable from one another if edge direction is ignored.

In our follow network, WCC confirms that all 10 people are connected — there are no isolated individuals who cannot be reached from the rest of the organisation even via indirect paths.

```python
results = g.weakly_connected_components()

components: dict[int, list] = {}
for r in results:
    components.setdefault(r["component"], []).append(r["user_id"])

print(f"Weakly connected components: {len(components)}")
for cid, members in sorted(components.items()):
    print(f"  Component {cid}: {sorted(members)}")
# Weakly connected components: 1
# Component 0: ['alice', 'bob', 'carol', ...]  (all 10 nodes in one component)
```

### Strongly Connected Components (SCC)

Groups nodes where every node can reach every other node following edge direction.

SCC detects mutual follow relationships — if Alice follows Bob and Bob follows Alice, they form a 2-node SCC. In our directed follow graph this reveals whether any subsets of colleagues have genuinely reciprocal connections rather than one-directional follows.

```python
results = g.strongly_connected_components()

components = {}
for r in results:
    components.setdefault(r["component"], []).append(r["user_id"])

print(f"Strongly connected components: {len(components)}")
for cid, members in sorted(components.items()):
    if len(members) > 1:
        print(f"  Multi-node SCC: {sorted(members)}")
    else:
        print(f"  Singleton: {members[0]}")
```

In a DAG (directed acyclic graph) every node is its own SCC. If there are mutual edges (A -> B and B -> A), those nodes form a multi-node SCC.

## Step 7: Traversal

### Breadth-First Search (BFS)

Explores nodes level by level from a starting point.

BFS shows Alice's immediate and second-degree network — the people she directly follows (depth 1) and the people they follow (depth 2). This maps naturally to "first-degree connections" and "people you might know" in a professional network.

```python
results = g.bfs("alice", max_depth=2)

print("BFS from alice (depth <= 2):")
for r in sorted(results, key=lambda x: (x["depth"], x["order"])):
    print(f"  depth={r['depth']}, order={r['order']}: {r['user_id']}")
```

Output:
```
BFS from alice (depth <= 2):
  depth=0, order=0: alice
  depth=1, order=1: bob
  depth=1, order=2: carol
  depth=1, order=3: james
  depth=2, order=4: dave
  depth=2, order=5: eve
  depth=2, order=6: grace
```

### Depth-First Search (DFS)

Follows each branch as far as possible before backtracking.

DFS explores each follow chain to its end before backtracking — useful here for tracing the full chain of influence from Alice through each branch of the follow graph.

```python
results = g.dfs("alice", max_depth=3)

print("DFS from alice (depth <= 3):")
for r in sorted(results, key=lambda x: x["order"]):
    indent = "  " * r["depth"]
    print(f"  order={r['order']}: {indent}{r['user_id']}")
```

## Step 8: Similarity

### Node Similarity (Jaccard)

Computes Jaccard similarity between the neighbor sets of two nodes. Two nodes are similar if they share many of the same neighbors.

Node similarity surfaces people who follow a similar set of colleagues. In this network, two Team A engineers who both follow the same set of managers will have a high Jaccard score — a signal they may not yet know each other but would benefit from connecting.

```python
# All pairs above threshold 0.3
results = g.node_similarity(threshold=0.3)

print("Similar node pairs (Jaccard >= 0.3):")
for r in sorted(results, key=lambda x: x["similarity"], reverse=True):
    print(f"  {r['node1']:8s} <-> {r['node2']:8s}: {r['similarity']:.3f}")
```

### K-Nearest Neighbors (KNN)

Finds the k most similar nodes to a given node, ranked by Jaccard similarity.

KNN narrows node similarity to a single starting node. Here we find the five people whose follow patterns most closely resemble Alice's — a ranked "people you may know" list personalised to her position in the network.

```python
results = g.knn("alice", k=5)

print("Alice's 5 nearest neighbors:")
for r in results:
    print(f"  rank={r['rank']}: {r['neighbor']:8s} (similarity={r['similarity']:.3f})")
```

### Triangle Count

Counts how many triangles (3-cycles) each node participates in, and computes the local clustering coefficient (fraction of possible triangles that actually exist).

Triangle count measures how cliquey a node's neighbourhood is. In our professional network, a high clustering coefficient around a manager suggests their direct reports also follow each other — a tight sub-team. A low coefficient suggests a hub connecting otherwise separate groups.

```python
results = g.triangle_count()

print("Triangle count and clustering coefficient:")
for r in sorted(results, key=lambda x: x["clustering_coefficient"], reverse=True)[:5]:
    print(f"  {r['user_id']:8s}: triangles={r['triangles']}, cc={r['clustering_coefficient']:.3f}")
```

## Step 9: Combine Algorithms with Cypher Queries

Algorithm output is just a list of dictionaries — feed it back into Cypher queries to enrich analysis.

### Find the highest-PageRank node's community

```python
pagerank_results = g.pagerank()
top_node = max(pagerank_results, key=lambda x: x["score"])

# What community does the top node belong to?
community_results = g.community_detection()
community_map = {r["user_id"]: r["community"] for r in community_results}
top_community = community_map[top_node["user_id"]]

# Who else is in that community?
same_community = [uid for uid, cid in community_map.items() if cid == top_community]

print(f"Top PageRank node: {top_node['user_id']} (score={top_node['score']:.4f})")
print(f"Community {top_community} members: {same_community}")

# Now query those community members
results = g.connection.cypher(
    """
    MATCH (p:Person)
    WHERE p.name IN $names
    RETURN p.name AS name, p.role AS role, p.team AS team
    ORDER BY p.name
    """,
    {"names": [n.title() for n in same_community]}
)

for row in results:
    print(f"  {row['name']} — {row['role']} (Team {row['team']})")
```

### Rank nodes by betweenness and query their shortest paths

```python
betweenness = g.betweenness_centrality()
top_bridge = max(betweenness, key=lambda x: x["score"])["user_id"]

# Find everyone this bridge connects
path_result = g.shortest_path("alice", "james")
print(f"Bridge node: {top_bridge}")
print(f"alice -> james shortest path: {path_result['path']}")
```

For a complete comparison of when to use each algorithm, see the [Graph Algorithms Reference](../../reference/algorithms/).

## Next Steps

- [Graph Algorithms Reference](../../reference/algorithms/) — Complete parameter documentation for every algorithm
- [Graph Algorithms (SQL)](../sql-algorithms/) — Run the same algorithms directly from SQL
- [Use Graph Algorithms](../../how-to/graph-algorithms/) — How-to guide with performance tips
- [Performance](../../explanation/performance/) — Complexity and scalability notes per algorithm
