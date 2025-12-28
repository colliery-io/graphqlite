# Graph Algorithms in SQL

This tutorial shows how to run graph algorithms and work with their results in SQL.

## Setup: Citation Network

```sql
.load build/graphqlite.dylib
.mode column
.headers on

-- Create papers
SELECT cypher('CREATE (p:Paper {title: "Foundations"})');
SELECT cypher('CREATE (p:Paper {title: "Methods"})');
SELECT cypher('CREATE (p:Paper {title: "Applications"})');
SELECT cypher('CREATE (p:Paper {title: "Survey"})');
SELECT cypher('CREATE (p:Paper {title: "Analysis"})');
SELECT cypher('CREATE (p:Paper {title: "Review"})');

-- Create citations (citing paper -> cited paper)
SELECT cypher('MATCH (a:Paper {title: "Methods"}), (b:Paper {title: "Foundations"}) CREATE (a)-[:CITES]->(b)');
SELECT cypher('MATCH (a:Paper {title: "Applications"}), (b:Paper {title: "Foundations"}) CREATE (a)-[:CITES]->(b)');
SELECT cypher('MATCH (a:Paper {title: "Applications"}), (b:Paper {title: "Methods"}) CREATE (a)-[:CITES]->(b)');
SELECT cypher('MATCH (a:Paper {title: "Survey"}), (b:Paper {title: "Foundations"}) CREATE (a)-[:CITES]->(b)');
SELECT cypher('MATCH (a:Paper {title: "Survey"}), (b:Paper {title: "Methods"}) CREATE (a)-[:CITES]->(b)');
SELECT cypher('MATCH (a:Paper {title: "Survey"}), (b:Paper {title: "Applications"}) CREATE (a)-[:CITES]->(b)');
SELECT cypher('MATCH (a:Paper {title: "Analysis"}), (b:Paper {title: "Methods"}) CREATE (a)-[:CITES]->(b)');
SELECT cypher('MATCH (a:Paper {title: "Review"}), (b:Paper {title: "Survey"}) CREATE (a)-[:CITES]->(b)');
```

## PageRank

Find influential papers based on citation structure.

### Basic usage

```sql
SELECT cypher('RETURN pageRank(0.85, 20)');
```

### Extract as table

```sql
SELECT
    json_extract(value, '$.node_id') as id,
    json_extract(value, '$.user_id') as paper_id,
    printf('%.4f', json_extract(value, '$.score')) as score
FROM json_each(cypher('RETURN pageRank(0.85, 20)'))
ORDER BY json_extract(value, '$.score') DESC;
```

### Join with node properties

```sql
WITH rankings AS (
    SELECT
        json_extract(value, '$.node_id') as node_id,
        json_extract(value, '$.score') as score
    FROM json_each(cypher('RETURN pageRank(0.85, 20)'))
)
SELECT
    n.user_id as paper,
    printf('%.4f', r.score) as influence
FROM rankings r
JOIN nodes n ON n.id = r.node_id
ORDER BY r.score DESC;
```

## Community Detection

### Label Propagation

```sql
SELECT cypher('RETURN labelPropagation(10)');
```

### Group by community

```sql
WITH communities AS (
    SELECT
        json_extract(value, '$.node_id') as node_id,
        json_extract(value, '$.community') as community
    FROM json_each(cypher('RETURN labelPropagation(10)'))
)
SELECT
    c.community,
    group_concat(n.user_id) as papers
FROM communities c
JOIN nodes n ON n.id = c.node_id
GROUP BY c.community;
```

### Louvain

```sql
SELECT cypher('RETURN louvain(1.0)');
```

## Centrality Metrics

### Degree Centrality

```sql
SELECT
    json_extract(value, '$.user_id') as paper,
    json_extract(value, '$.in_degree') as cited_by,
    json_extract(value, '$.out_degree') as cites
FROM json_each(cypher('RETURN degreeCentrality()'))
ORDER BY json_extract(value, '$.in_degree') DESC;
```

### Betweenness Centrality

```sql
SELECT
    json_extract(value, '$.user_id') as paper,
    printf('%.4f', json_extract(value, '$.score')) as betweenness
FROM json_each(cypher('RETURN betweennessCentrality()'))
ORDER BY json_extract(value, '$.score') DESC;
```

## Path Finding

### Shortest Path

```sql
SELECT cypher('RETURN dijkstra("Review", "Foundations")');
```

Result shows path and distance:
```json
{"distance": 3, "path": ["Review", "Survey", "Foundations"]}
```

## Combining Algorithms with Queries

### Find most influential paper's citations

```sql
-- Get top paper by PageRank
WITH top_paper AS (
    SELECT json_extract(value, '$.user_id') as paper_id
    FROM json_each(cypher('RETURN pageRank()'))
    ORDER BY json_extract(value, '$.score') DESC
    LIMIT 1
)
-- Find what it cites
SELECT cypher(
    'MATCH (p:Paper {title: "' || paper_id || '"})-[:CITES]->(cited) RETURN cited.title'
)
FROM top_paper;
```

### Export for visualization

```sql
-- Export nodes
.mode csv
.output nodes.csv
SELECT
    json_extract(value, '$.node_id') as id,
    json_extract(value, '$.user_id') as label,
    json_extract(value, '$.score') as pagerank
FROM json_each(cypher('RETURN pageRank()'));

-- Export edges
.output edges.csv
SELECT
    source_id, target_id, label as type
FROM edges;
.output stdout
```

## Performance Tips

1. **Limit output** for large graphs:
   ```sql
   SELECT * FROM json_each(cypher('RETURN pageRank()')) LIMIT 100;
   ```

2. **Create views** for repeated queries:
   ```sql
   CREATE VIEW paper_influence AS
   SELECT
       json_extract(value, '$.node_id') as node_id,
       json_extract(value, '$.score') as score
   FROM json_each(cypher('RETURN pageRank()'));
   ```

3. **Index algorithm results** if needed repeatedly:
   ```sql
   CREATE TABLE pagerank_cache AS
   SELECT * FROM json_each(cypher('RETURN pageRank()'));
   CREATE INDEX idx_pagerank ON pagerank_cache(json_extract(value, '$.score'));
   ```

## Next Steps

- [Graph Algorithms Reference](../reference/algorithms.md) - All available algorithms
- [Performance Guide](../explanation/performance.md) - Algorithm performance characteristics
