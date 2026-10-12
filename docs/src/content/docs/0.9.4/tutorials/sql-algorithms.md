---
title: Graph Algorithms in SQL
slug: 0.9.4/tutorials/sql-algorithms
---

This tutorial shows how to run every graph algorithm directly from the SQLite CLI and integrate the results with regular SQL. The domain is a citation network of computer science papers.

## What You Will Learn

* Build a citation network in SQL
* Load the graph algorithm cache
* Run all 15+ algorithms via `cypher()` calls
* Extract structured results with `json_each()` and `json_extract()`
* Create SQL views over algorithm results
* Join algorithm output with regular SQL tables
* Cache algorithm results for repeated queries

## Prerequisites

* SQLite 3.x CLI
* GraphQLite extension built or extracted — see [Getting Started (SQL)](../sql-getting-started/) for setup

## Step 1: Build the Citation Network

Save this block as `citation_setup.sql`:

```sql
.load build/graphqlite
.mode column
.headers on

-- Papers (using descriptive IDs as user-facing identifiers)
SELECT cypher('CREATE (p:Paper {title: "Deep Residual Learning",          year: 2016, venue: "CVPR",    field: "Vision"})');
SELECT cypher('CREATE (p:Paper {title: "Attention Is All You Need",       year: 2017, venue: "NeurIPS", field: "NLP"})');
SELECT cypher('CREATE (p:Paper {title: "BERT",                            year: 2018, venue: "NAACL",   field: "NLP"})');
SELECT cypher('CREATE (p:Paper {title: "ImageNet Classification with CNN",year: 2012, venue: "NeurIPS", field: "Vision"})');
SELECT cypher('CREATE (p:Paper {title: "Generative Adversarial Networks", year: 2014, venue: "NeurIPS", field: "Vision"})');
SELECT cypher('CREATE (p:Paper {title: "Word2Vec",                        year: 2013, venue: "NIPS",    field: "NLP"})');
SELECT cypher('CREATE (p:Paper {title: "Graph Convolutional Networks",    year: 2017, venue: "ICLR",    field: "Graphs"})');
SELECT cypher('CREATE (p:Paper {title: "GraphSAGE",                       year: 2017, venue: "NeurIPS", field: "Graphs"})');

-- Citations (citing -> cited)
SELECT cypher('MATCH (a:Paper {title: "Deep Residual Learning"}),
                     (b:Paper {title: "ImageNet Classification with CNN"})
               CREATE (a)-[:CITES]->(b)');

SELECT cypher('MATCH (a:Paper {title: "Attention Is All You Need"}),
                     (b:Paper {title: "Word2Vec"})
               CREATE (a)-[:CITES]->(b)');

SELECT cypher('MATCH (a:Paper {title: "BERT"}),
                     (b:Paper {title: "Attention Is All You Need"})
               CREATE (a)-[:CITES]->(b)');

SELECT cypher('MATCH (a:Paper {title: "BERT"}),
                     (b:Paper {title: "Word2Vec"})
               CREATE (a)-[:CITES]->(b)');

SELECT cypher('MATCH (a:Paper {title: "Graph Convolutional Networks"}),
                     (b:Paper {title: "Word2Vec"})
               CREATE (a)-[:CITES]->(b)');

SELECT cypher('MATCH (a:Paper {title: "GraphSAGE"}),
                     (b:Paper {title: "Graph Convolutional Networks"})
               CREATE (a)-[:CITES]->(b)');

SELECT cypher('MATCH (a:Paper {title: "GraphSAGE"}),
                     (b:Paper {title: "Word2Vec"})
               CREATE (a)-[:CITES]->(b)');

SELECT cypher('MATCH (a:Paper {title: "Generative Adversarial Networks"}),
                     (b:Paper {title: "ImageNet Classification with CNN"})
               CREATE (a)-[:CITES]->(b)');
```

Run it:

```bash
sqlite3 citations.db < citation_setup.sql
```

## Step 2: Load the Graph Cache

All algorithms require the graph to be loaded into memory as a CSR structure. Run this once per session, and again after structural changes (node or edge additions/deletions).

```sql
-- Load graph into algorithm cache
SELECT cypher('RETURN gql_load_graph()');
-- [{"gql_load_graph()":1}]

-- Confirm it is loaded
SELECT cypher('RETURN gql_graph_loaded()');
-- [{"gql_graph_loaded()":1}]
```

Reload after changes:

```sql
SELECT cypher('RETURN gql_reload_graph()');
```

## Step 3: Centrality Algorithms

### PageRank

PageRank ranks papers by the importance of papers that cite them. A paper cited by many well-cited papers scores high.

```sql
-- Raw result
SELECT cypher('RETURN pageRank(0.85, 20)');
```

Extract as a table:

```sql
SELECT
    json_extract(value, '$.user_id')                           AS paper,
    printf('%.4f', json_extract(value, '$.score'))             AS pagerank_score,
    json_extract(value, '$.node_id')                           AS internal_id
FROM json_each(cypher('RETURN pageRank(0.85, 20)'), '$[0].column_0')
ORDER BY json_extract(value, '$.score') DESC;
```

Output:

```
paper                             pagerank_score  internal_id
--------------------------------  --------------  -----------
Word2Vec                          0.2104          6
ImageNet Classification with CNN  0.1837          4
Attention Is All You Need         0.1512          2
Graph Convolutional Networks      0.1244          7
...
```

`Word2Vec` and `ImageNet` score highest — they are cited by multiple downstream papers.

### Degree Centrality

Counts how many papers cite each paper (in-degree) and how many papers each paper cites (out-degree).

```sql
SELECT
    json_extract(value, '$.user_id')    AS paper,
    json_extract(value, '$.in_degree')  AS cited_by,
    json_extract(value, '$.out_degree') AS cites,
    json_extract(value, '$.degree')     AS total
FROM json_each(cypher('RETURN degreeCentrality()'), '$[0].column_0')
ORDER BY json_extract(value, '$.in_degree') DESC;
```

Output:

```
paper                             cited_by  cites  total
--------------------------------  --------  -----  -----
Word2Vec                          3         0      3
ImageNet Classification with CNN  2         0      2
Attention Is All You Need         1         1      2
...
```

### Betweenness Centrality

Measures how often a paper lies on the shortest path between two other papers — a proxy for "gateway" or "bridge" papers.

```sql
SELECT
    json_extract(value, '$.user_id')                           AS paper,
    printf('%.4f', json_extract(value, '$.score'))             AS betweenness
FROM json_each(cypher('RETURN betweennessCentrality()'), '$[0].column_0')
ORDER BY json_extract(value, '$.score') DESC;
```

### Closeness Centrality

```sql
SELECT
    json_extract(value, '$.user_id')                           AS paper,
    printf('%.4f', json_extract(value, '$.score'))             AS closeness
FROM json_each(cypher('RETURN closenessCentrality()'), '$[0].column_0')
ORDER BY json_extract(value, '$.score') DESC;
```

### Eigenvector Centrality

```sql
SELECT
    json_extract(value, '$.user_id')                           AS paper,
    printf('%.4f', json_extract(value, '$.score'))             AS eigenvector
FROM json_each(cypher('RETURN eigenvectorCentrality(100)'), '$[0].column_0')
ORDER BY json_extract(value, '$.score') DESC;
```

## Step 4: Community Detection

### Label Propagation

Detects clusters by iteratively propagating labels through the network.

```sql
-- Raw
SELECT cypher('RETURN labelPropagation(10)');
```

Group by community:

```sql
SELECT
    json_extract(value, '$.community') AS community,
    json_extract(value, '$.user_id')   AS paper
FROM json_each(cypher('RETURN labelPropagation(10)'), '$[0].column_0')
ORDER BY json_extract(value, '$.community'), paper;
```

Using `group_concat` to show communities on one line each:

```sql
WITH community_data AS (
    SELECT
        json_extract(value, '$.community') AS community,
        json_extract(value, '$.user_id')   AS paper
    FROM json_each(cypher('RETURN labelPropagation(10)'), '$[0].column_0')
)
SELECT
    community,
    group_concat(paper, ', ') AS papers,
    count(*)                  AS size
FROM community_data
GROUP BY community
ORDER BY size DESC;
```

### Louvain

Higher-quality community detection using modularity optimisation.

```sql
WITH louvain_data AS (
    SELECT
        json_extract(value, '$.community') AS community,
        json_extract(value, '$.user_id')   AS paper
    FROM json_each(cypher('RETURN louvain(1.0)'), '$[0].column_0')
)
SELECT
    community,
    group_concat(paper, ', ') AS papers,
    count(*)                  AS size
FROM louvain_data
GROUP BY community
ORDER BY size DESC;
```

Try different resolutions to tune granularity:

```sql
-- More communities (finer granularity)
SELECT cypher('RETURN louvain(2.0)');

-- Fewer communities (coarser granularity)
SELECT cypher('RETURN louvain(0.5)');
```

## Step 5: Connected Components

### Weakly Connected Components (WCC)

Finds groups of nodes reachable from one another, ignoring edge direction.

```sql
SELECT
    json_extract(value, '$.component') AS component,
    json_extract(value, '$.user_id')   AS paper
FROM json_each(cypher('RETURN wcc()'), '$[0].column_0')
ORDER BY json_extract(value, '$.component'), paper;
```

Count the number of components and their sizes:

```sql
WITH wcc_data AS (
    SELECT
        json_extract(value, '$.component') AS component,
        json_extract(value, '$.user_id')   AS paper
    FROM json_each(cypher('RETURN wcc()'), '$[0].column_0')
)
SELECT
    component,
    count(*) AS size,
    group_concat(paper, ', ') AS members
FROM wcc_data
GROUP BY component
ORDER BY size DESC;
```

### Strongly Connected Components (SCC)

```sql
WITH scc_data AS (
    SELECT
        json_extract(value, '$.component') AS component,
        json_extract(value, '$.user_id')   AS paper
    FROM json_each(cypher('RETURN scc()'), '$[0].column_0')
)
SELECT component, count(*) AS size, group_concat(paper, ', ') AS members
FROM scc_data
GROUP BY component
ORDER BY size DESC;
```

In a citation graph (a DAG), every node is its own SCC — no paper can both cite and be cited by the same chain. If you add mutual edges, multi-node SCCs appear.

## Step 6: Path Finding

### Dijkstra (Shortest Path)

Find the shortest citation path between two papers.

```sql
SELECT cypher('RETURN dijkstra("BERT", "ImageNet Classification with CNN")');
```

Output:

```json
[{"dijkstra(\"BERT\", \"ImageNet Classification with CNN\")":
  "{\"found\":true,\"distance\":3,\"path\":[\"BERT\",\"Attention Is All You Need\",\"Word2Vec\",\"ImageNet Classification with CNN\"]}"}]
```

Extract fields:

```sql
WITH path_result AS (
    SELECT json_extract(cypher('RETURN dijkstra("BERT", "ImageNet Classification with CNN")'), '$[0].column_0') AS raw
)
SELECT
    json_extract(raw, '$.found')    AS found,
    json_extract(raw, '$.distance') AS hops,
    json_extract(raw, '$.path')     AS path
FROM path_result;
```

### A\* Search

```sql
SELECT cypher('RETURN astar("BERT", "ImageNet Classification with CNN")');
```

With geographic properties for the heuristic:

```sql
SELECT cypher('RETURN astar("node_a", "node_b", "lat", "lon")');
```

### All-Pairs Shortest Paths (APSP)

Computes shortest distances between every pair of nodes. O(n²) — use with caution on large graphs.

```sql
-- Compute all pairs
SELECT
    json_extract(value, '$.source')   AS source,
    json_extract(value, '$.target')   AS target,
    json_extract(value, '$.distance') AS distance
FROM json_each(cypher('RETURN apsp()'), '$[0].column_0')
WHERE json_extract(value, '$.source') <> json_extract(value, '$.target')
ORDER BY json_extract(value, '$.distance') DESC
LIMIT 10;
```

## Step 7: Traversal

### BFS

```sql
-- BFS from "BERT" up to depth 3
SELECT
    json_extract(value, '$.user_id') AS paper,
    json_extract(value, '$.depth')   AS depth,
    json_extract(value, '$.order')   AS visit_order
FROM json_each(cypher('RETURN bfs("BERT", 3)'), '$[0].column_0')
ORDER BY json_extract(value, '$.order');
```

### DFS

```sql
SELECT
    json_extract(value, '$.user_id') AS paper,
    json_extract(value, '$.depth')   AS depth,
    json_extract(value, '$.order')   AS visit_order
FROM json_each(cypher('RETURN dfs("BERT", 4)'), '$[0].column_0')
ORDER BY json_extract(value, '$.order');
```

## Step 8: Similarity

### Node Similarity (Jaccard)

Two papers are similar if they cite many of the same papers.

```sql
SELECT
    json_extract(value, '$.node1')      AS paper1,
    json_extract(value, '$.node2')      AS paper2,
    printf('%.3f', json_extract(value, '$.similarity')) AS jaccard
FROM json_each(cypher('RETURN nodeSimilarity()'), '$[0].column_0')
ORDER BY json_extract(value, '$.similarity') DESC;
```

### KNN

```sql
-- Top 3 papers most similar to BERT
SELECT
    json_extract(value, '$.neighbor')                   AS similar_paper,
    printf('%.3f', json_extract(value, '$.similarity')) AS similarity,
    json_extract(value, '$.rank')                       AS rank
FROM json_each(cypher('RETURN knn("BERT", 3)'), '$[0].column_0')
ORDER BY json_extract(value, '$.rank');
```

### Triangle Count

```sql
SELECT
    json_extract(value, '$.user_id')                            AS paper,
    json_extract(value, '$.triangles')                          AS triangles,
    printf('%.3f', json_extract(value, '$.clustering_coefficient')) AS clustering_coeff
FROM json_each(cypher('RETURN triangleCount()'), '$[0].column_0')
ORDER BY json_extract(value, '$.triangles') DESC;
```

## Step 9: Create SQL Views Over Algorithm Results

Views let you query algorithm output like a table without re-running the algorithm each time.

```sql
-- PageRank view
CREATE VIEW IF NOT EXISTS v_pagerank AS
SELECT
    json_extract(value, '$.node_id')  AS node_id,
    json_extract(value, '$.user_id')  AS paper,
    json_extract(value, '$.score')    AS score
FROM json_each(cypher('RETURN pageRank(0.85, 20)'), '$[0].column_0');

-- Community view
CREATE VIEW IF NOT EXISTS v_communities AS
SELECT
    json_extract(value, '$.node_id')  AS node_id,
    json_extract(value, '$.user_id')  AS paper,
    json_extract(value, '$.community') AS community
FROM json_each(cypher('RETURN labelPropagation(10)'), '$[0].column_0');

-- Degree view
CREATE VIEW IF NOT EXISTS v_degree AS
SELECT
    json_extract(value, '$.user_id')    AS paper,
    json_extract(value, '$.in_degree')  AS in_degree,
    json_extract(value, '$.out_degree') AS out_degree,
    json_extract(value, '$.degree')     AS degree
FROM json_each(cypher('RETURN degreeCentrality()'), '$[0].column_0');
```

Query the views:

```sql
SELECT paper, score FROM v_pagerank ORDER BY score DESC;

SELECT community, count(*) AS size FROM v_communities GROUP BY community;

SELECT p.paper, p.score AS pagerank, d.in_degree AS cited_by
FROM v_pagerank p
JOIN v_degree d ON d.paper = p.paper
ORDER BY p.score DESC;
```

## Step 10: Combine Algorithm Output with Regular SQL Tables

Create a regular metadata table for papers:

```sql
CREATE TABLE IF NOT EXISTS paper_metadata (
    title        TEXT PRIMARY KEY,
    authors      TEXT,
    impact_factor REAL
);

INSERT OR IGNORE INTO paper_metadata VALUES
    ('Deep Residual Learning',          'He, Zhang, Ren, Sun', 9.8),
    ('Attention Is All You Need',       'Vaswani et al.',      9.5),
    ('BERT',                            'Devlin et al.',       9.2),
    ('ImageNet Classification with CNN','Krizhevsky et al.',   9.7),
    ('Generative Adversarial Networks', 'Goodfellow et al.',   9.6),
    ('Word2Vec',                        'Mikolov et al.',      9.0),
    ('Graph Convolutional Networks',    'Kipf, Welling',       8.8),
    ('GraphSAGE',                       'Hamilton et al.',     8.5);
```

Join PageRank with metadata:

```sql
WITH rankings AS (
    SELECT
        json_extract(value, '$.user_id') AS title,
        json_extract(value, '$.score')   AS pagerank
    FROM json_each(cypher('RETURN pageRank(0.85, 20)'), '$[0].column_0')
)
SELECT
    r.title,
    pm.authors,
    printf('%.4f', r.pagerank)    AS pagerank_score,
    pm.impact_factor,
    ROUND(r.pagerank * 10 + pm.impact_factor, 2) AS combined_score
FROM rankings r
JOIN paper_metadata pm ON pm.title = r.title
ORDER BY combined_score DESC;
```

Add community labels to metadata:

```sql
WITH comm AS (
    SELECT
        json_extract(value, '$.user_id')   AS title,
        json_extract(value, '$.community') AS community
    FROM json_each(cypher('RETURN louvain(1.0)'), '$[0].column_0')
)
SELECT
    pm.title,
    pm.authors,
    c.community AS research_cluster
FROM paper_metadata pm
JOIN comm c ON c.title = pm.title
ORDER BY c.community, pm.title;
```

## Step 11: Cache Algorithm Results

For expensive algorithms on large graphs, cache results in a real table:

```sql
-- Cache PageRank results
DROP TABLE IF EXISTS pagerank_cache;
CREATE TABLE pagerank_cache AS
SELECT
    json_extract(value, '$.node_id')  AS node_id,
    json_extract(value, '$.user_id')  AS paper,
    json_extract(value, '$.score')    AS score,
    datetime('now')                   AS computed_at
FROM json_each(cypher('RETURN pageRank(0.85, 20)'), '$[0].column_0');

CREATE INDEX IF NOT EXISTS idx_pagerank_score ON pagerank_cache(score DESC);

-- Fast repeated queries
SELECT paper, score FROM pagerank_cache ORDER BY score DESC LIMIT 5;
```

Export results to CSV for visualisation:

```sql
.mode csv
.output pagerank_results.csv
SELECT paper, score FROM pagerank_cache ORDER BY score DESC;
.output stdout
.mode column
```

## Next Steps

* [Graph Algorithms Reference](../../reference/algorithms/) — Full parameter documentation for every algorithm
* [SQL Interface Reference](../../reference/sql-interface/) — `cypher()` function, `json_each()` patterns, and schema tables
* [Performance](../../explanation/performance/) — Algorithm complexity and scaling guidance
