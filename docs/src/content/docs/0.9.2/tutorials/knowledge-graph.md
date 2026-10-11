---
title: Building a Knowledge Graph
slug: 0.9.2/tutorials/knowledge-graph
---

This tutorial builds a research publication knowledge graph in Python. The domain includes researchers, academic papers, and research topics, connected by authorship, citation, and topic membership. You will model the schema, populate it with parameterized writes, query it with Cypher, apply graph algorithms to discover influential papers and research clusters, and maintain the graph over time.

## What You Will Build

A knowledge graph with:

**Node labels**
- `Researcher` — scientists and academics
- `Paper` — published works
- `Topic` — research areas

**Relationship types**
- `AUTHORED` — Researcher authored Paper
- `CITES` — Paper cites Paper
- `IN_TOPIC` — Paper belongs to a Topic
- `COLLABORATES` — Researcher co-authored with Researcher (derived)

## What You Will Learn

- Design a multi-type node schema
- Use parameterized queries for all writes
- Find co-authors, citation chains, and research clusters
- Apply PageRank and community detection
- Update and delete graph elements
- Persist the graph to a file and reopen it

## Prerequisites

```bash
pip install graphqlite
```

## Step 1: Create the Graph

```python
from graphqlite import Graph

g = Graph("research.db")
```

## Step 2: Add Researchers

Use parameterized `CREATE` statements via `g.connection.cypher()` for all writes. This ensures special characters in names or affiliations never break the query.

```python
researchers = [
    {"id": "r_alice",   "name": "Alice Nakamura",   "affiliation": "MIT",        "h_index": 28},
    {"id": "r_bob",     "name": "Bob Osei",          "affiliation": "Stanford",   "h_index": 19},
    {"id": "r_carol",   "name": "Carol Petrov",      "affiliation": "Cambridge",  "h_index": 35},
    {"id": "r_dave",    "name": "Dave Fontaine",     "affiliation": "MIT",        "h_index": 12},
    {"id": "r_eve",     "name": "Eve Svensson",      "affiliation": "ETH Zurich", "h_index": 22},
]

for r in researchers:
    g.connection.cypher(
        """
        CREATE (r:Researcher {
            id:          $id,
            name:        $name,
            affiliation: $affiliation,
            h_index:     $h_index
        })
        """,
        r
    )

print(g.stats())
# {'nodes': 5, 'edges': 0}
```

## Step 3: Add Topics

```python
topics = [
    {"id": "t_ml",      "name": "Machine Learning",         "field": "Computer Science"},
    {"id": "t_nlp",     "name": "Natural Language Processing","field": "Computer Science"},
    {"id": "t_bio",     "name": "Computational Biology",     "field": "Biology"},
    {"id": "t_graphs",  "name": "Graph Theory",             "field": "Mathematics"},
]

for t in topics:
    g.connection.cypher(
        "CREATE (t:Topic {id: $id, name: $name, field: $field})",
        t
    )
```

## Step 4: Add Papers

```python
papers = [
    {"id": "p_attention",  "title": "Attention Is All You Need",      "year": 2017, "citations": 80000},
    {"id": "p_bert",       "title": "BERT: Pre-training of Deep Bidirectional Transformers", "year": 2018, "citations": 50000},
    {"id": "p_gnn",        "title": "Semi-Supervised Classification with GCN", "year": 2017, "citations": 18000},
    {"id": "p_pagerank",   "title": "The PageRank Citation Ranking",  "year": 1998, "citations": 15000},
    {"id": "p_word2vec",   "title": "Distributed Representations of Words", "year": 2013, "citations": 28000},
    {"id": "p_alphafold",  "title": "Highly Accurate Protein Structure Prediction", "year": 2021, "citations": 12000},
    {"id": "p_graphsage",  "title": "Inductive Representation Learning on Large Graphs", "year": 2017, "citations": 9000},
]

for p in papers:
    g.connection.cypher(
        """
        CREATE (p:Paper {
            id:        $id,
            title:     $title,
            year:      $year,
            citations: $citations
        })
        """,
        p
    )

print(g.stats())
# {'nodes': 16, 'edges': 0}
```

## Step 5: Add Relationships

### Authorship

```python
authorships = [
    ("r_alice", "p_attention"),
    ("r_alice", "p_bert"),
    ("r_bob",   "p_bert"),
    ("r_bob",   "p_gnn"),
    ("r_carol", "p_pagerank"),
    ("r_carol", "p_graphsage"),
    ("r_dave",  "p_gnn"),
    ("r_dave",  "p_graphsage"),
    ("r_eve",   "p_word2vec"),
    ("r_eve",   "p_alphafold"),
    ("r_alice", "p_word2vec"),
]

for researcher_id, paper_id in authorships:
    g.connection.cypher(
        """
        MATCH (r:Researcher {id: $researcher_id}), (p:Paper {id: $paper_id})
        CREATE (r)-[:AUTHORED]->(p)
        """,
        {"researcher_id": researcher_id, "paper_id": paper_id}
    )
```

### Citation graph

```python
citations = [
    ("p_bert",       "p_attention"),   # BERT cites Attention
    ("p_gnn",        "p_pagerank"),    # GCN cites PageRank
    ("p_graphsage",  "p_gnn"),         # GraphSAGE cites GCN
    ("p_graphsage",  "p_pagerank"),    # GraphSAGE cites PageRank
    ("p_alphafold",  "p_attention"),   # AlphaFold cites Attention
    ("p_bert",       "p_word2vec"),    # BERT cites Word2Vec
    ("p_attention",  "p_word2vec"),    # Attention cites Word2Vec
]

for citing_id, cited_id in citations:
    g.connection.cypher(
        """
        MATCH (a:Paper {id: $citing_id}), (b:Paper {id: $cited_id})
        CREATE (a)-[:CITES]->(b)
        """,
        {"citing_id": citing_id, "cited_id": cited_id}
    )
```

### Topic membership

```python
topic_memberships = [
    ("p_attention",  "t_nlp"),
    ("p_attention",  "t_ml"),
    ("p_bert",       "t_nlp"),
    ("p_bert",       "t_ml"),
    ("p_gnn",        "t_ml"),
    ("p_gnn",        "t_graphs"),
    ("p_pagerank",   "t_graphs"),
    ("p_word2vec",   "t_nlp"),
    ("p_alphafold",  "t_bio"),
    ("p_alphafold",  "t_ml"),
    ("p_graphsage",  "t_ml"),
    ("p_graphsage",  "t_graphs"),
]

for paper_id, topic_id in topic_memberships:
    g.connection.cypher(
        """
        MATCH (p:Paper {id: $paper_id}), (t:Topic {id: $topic_id})
        CREATE (p)-[:IN_TOPIC]->(t)
        """,
        {"paper_id": paper_id, "topic_id": topic_id}
    )

print(g.stats())
# {'nodes': 16, 'edges': 30}
```

## Step 6: Query Patterns

### Find co-authors of a researcher

```python
results = g.connection.cypher(
    """
    MATCH (r:Researcher {id: $researcher_id})-[:AUTHORED]->(p:Paper)<-[:AUTHORED]-(coauthor:Researcher)
    WHERE coauthor.id <> $researcher_id
    RETURN DISTINCT coauthor.name AS coauthor, collect(p.title) AS shared_papers
    ORDER BY coauthor
    """,
    {"researcher_id": "r_alice"}
)

for row in results:
    print(f"{row['coauthor']}: {row['shared_papers']}")
# Bob Osei: ['BERT: Pre-training of Deep Bidirectional Transformers']
# Eve Svensson: ['Distributed Representations of Words']
```

### Follow the citation chain from a paper

```python
results = g.connection.cypher(
    """
    MATCH (p:Paper {id: $paper_id})-[:CITES*1..3]->(cited:Paper)
    RETURN DISTINCT cited.title AS title, cited.year AS year, cited.citations AS citation_count
    ORDER BY citation_count DESC
    """,
    {"paper_id": "p_bert"}
)

print("Papers cited by BERT (up to 3 hops):")
for row in results:
    print(f"  {row['title']} ({row['year']}) — {row['citation_count']:,} citations")
# Papers cited by BERT (up to 3 hops):
#   Distributed Representations of Words (2013) — 28,000 citations
#   Attention Is All You Need (2017) — 80,000 citations
#   The PageRank Citation Ranking (1998) — 15,000 citations
```

### Find all papers in a research topic

```python
results = g.connection.cypher(
    """
    MATCH (p:Paper)-[:IN_TOPIC]->(t:Topic {name: $topic})
    RETURN p.title AS title, p.year AS year, p.citations AS citations
    ORDER BY p.citations DESC
    """,
    {"topic": "Machine Learning"}
)

for row in results:
    print(f"{row['title']} — {row['citations']:,}")
# Attention Is All You Need — 80,000
# BERT: Pre-training of Deep Bidirectional Transformers — 50,000
# ...
```

### Find researchers working on overlapping topics

```python
results = g.connection.cypher(
    """
    MATCH (r1:Researcher)-[:AUTHORED]->(p1:Paper)-[:IN_TOPIC]->(t:Topic)<-[:IN_TOPIC]-(p2:Paper)<-[:AUTHORED]-(r2:Researcher)
    WHERE r1.id < r2.id
    RETURN DISTINCT r1.name AS researcher1, r2.name AS researcher2, collect(DISTINCT t.name) AS shared_topics
    ORDER BY r1.name
    """
)

for row in results:
    print(f"{row['researcher1']} & {row['researcher2']}: {row['shared_topics']}")
```

### Aggregate: papers per topic with average citation count

```python
results = g.connection.cypher(
    """
    MATCH (p:Paper)-[:IN_TOPIC]->(t:Topic)
    RETURN t.name AS topic, count(p) AS paper_count, round(avg(p.citations), 0) AS avg_citations
    ORDER BY avg_citations DESC
    """
)

for row in results:
    print(f"{row['topic']}: {row['paper_count']} papers, avg {row['avg_citations']:,.0f} citations")
```

## Step 7: Graph Algorithms

### PageRank — find influential papers

PageRank on a citation graph surfaces papers that are frequently cited by other high-impact papers.

```python
# Load the algorithm cache
g.connection.cypher("RETURN gql_load_graph()")

results = g.pagerank(damping=0.85, iterations=20)

print("Most influential papers by PageRank:")
for r in sorted(results, key=lambda x: x["score"], reverse=True)[:5]:
    node = g.get_node(r["user_id"])
    if node and node["label"] == "Paper":
        title = node["properties"].get("title", r["user_id"])
        print(f"  {title}: {r['score']:.4f}")
```

Expected top results: "Attention Is All You Need", "The PageRank Citation Ranking", and "Distributed Representations of Words" score highest because they are cited by multiple downstream papers.

### Community detection — discover research clusters

```python
results = g.community_detection(iterations=10)

communities: dict[int, list] = {}
for r in results:
    node = g.get_node(r["user_id"])
    if node:
        label = r["community"]
        communities.setdefault(label, []).append(
            (node["label"], node["properties"].get("name") or node["properties"].get("title", r["user_id"]))
        )

print("\nResearch communities:")
for community_id, members in sorted(communities.items()):
    print(f"\nCommunity {community_id}:")
    for label, name in sorted(members):
        print(f"  [{label}] {name}")
```

The NLP/ML cluster (Attention, BERT, Word2Vec) and the graph algorithms cluster (GCN, GraphSAGE, PageRank) typically separate into distinct communities.

### Betweenness centrality — find bridging papers

Papers with high betweenness centrality link different research areas.

```python
raw = g.connection.cypher("RETURN betweennessCentrality()")
import json
centrality = json.loads(raw[0]["betweennessCentrality()"])

print("\nTop bridging papers/researchers (betweenness centrality):")
for r in sorted(centrality, key=lambda x: x["score"], reverse=True)[:4]:
    node = g.get_node(r["user_id"])
    if node:
        name = node["properties"].get("name") or node["properties"].get("title", r["user_id"])
        print(f"  {name}: {r['score']:.4f}")
```

## Step 8: Update Properties

Use `SET` to update existing node and relationship properties:

```python
# Update a researcher's h-index
g.connection.cypher(
    "MATCH (r:Researcher {id: $id}) SET r.h_index = $h_index",
    {"id": "r_alice", "h_index": 31}
)

# Update a paper's citation count
g.connection.cypher(
    "MATCH (p:Paper {id: $id}) SET p.citations = $citations, p.last_updated = $date",
    {"id": "p_attention", "citations": 85000, "date": "2025-03-01"}
)

# Verify
node = g.get_node("r_alice")
print(node["properties"]["h_index"])  # 31
```

## Step 9: Delete Relationships

Remove a relationship without deleting the nodes:

```python
# A paper is reclassified out of a topic
g.connection.cypher(
    """
    MATCH (p:Paper {id: $paper_id})-[r:IN_TOPIC]->(t:Topic {id: $topic_id})
    DELETE r
    """,
    {"paper_id": "p_gnn", "topic_id": "t_graphs"}
)

# Re-add to a different topic
g.connection.cypher(
    """
    MATCH (p:Paper {id: $paper_id}), (t:Topic {id: $topic_id})
    CREATE (p)-[:IN_TOPIC]->(t)
    """,
    {"paper_id": "p_gnn", "topic_id": "t_nlp"}
)
```

Delete a node and all its relationships:

```python
g.connection.cypher(
    "MATCH (p:Paper {id: $id}) DETACH DELETE p",
    {"id": "p_graphsage"}
)

print(g.stats())
# {'nodes': 15, 'edges': ...}  (reduced)
```

## Step 10: Persist and Reopen

The graph is already persisted to `research.db` because that is what you passed to `Graph()`. Close and reopen:

```python
# Close the graph (optional — Python closes on garbage collection)
del g

# Reopen
g2 = Graph("research.db")
print(g2.stats())

results = g2.connection.cypher(
    "MATCH (r:Researcher {name: $name})-[:AUTHORED]->(p:Paper) RETURN p.title AS title",
    {"name": "Alice Nakamura"}
)
for row in results:
    print(row["title"])
```

The database is a standard SQLite file. You can attach it to other SQLite databases, back it up with `cp`, or inspect it with any SQLite browser.

## Next Steps

- [Graph Analytics](../graph-analytics/) — A full walkthrough of all 15+ algorithms on a dense social network
- [Graph Algorithms Reference](../../reference/algorithms/) — Complete algorithm parameter documentation
- [Python API Reference](../../reference/python-api/) — `Graph`, `Connection`, and `GraphManager` API reference
- [Parameterized Queries Guide](../../how-to/parameterized-queries/) — Why and how to always use parameters
