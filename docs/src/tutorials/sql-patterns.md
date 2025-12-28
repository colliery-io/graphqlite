# Query Patterns in SQL

This tutorial covers common MATCH patterns for traversing graphs using SQL.

## Setup

```sql
.load build/graphqlite.dylib
.mode column
.headers on

-- Build a social network
SELECT cypher('CREATE (a:Person {name: "Alice"})');
SELECT cypher('CREATE (b:Person {name: "Bob"})');
SELECT cypher('CREATE (c:Person {name: "Charlie"})');
SELECT cypher('CREATE (d:Person {name: "Diana"})');
SELECT cypher('CREATE (e:Person {name: "Eve"})');

SELECT cypher('MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"}) CREATE (a)-[:FOLLOWS]->(b)');
SELECT cypher('MATCH (a:Person {name: "Alice"}), (c:Person {name: "Charlie"}) CREATE (a)-[:FOLLOWS]->(c)');
SELECT cypher('MATCH (b:Person {name: "Bob"}), (c:Person {name: "Charlie"}) CREATE (b)-[:FOLLOWS]->(c)');
SELECT cypher('MATCH (b:Person {name: "Bob"}), (d:Person {name: "Diana"}) CREATE (b)-[:FOLLOWS]->(d)');
SELECT cypher('MATCH (c:Person {name: "Charlie"}), (e:Person {name: "Eve"}) CREATE (c)-[:FOLLOWS]->(e)');
SELECT cypher('MATCH (d:Person {name: "Diana"}), (e:Person {name: "Eve"}) CREATE (d)-[:FOLLOWS]->(e)');
```

## Direct Connections

### Outgoing relationships

```sql
-- Who does Alice follow?
SELECT cypher('
    MATCH (a:Person {name: "Alice"})-[:FOLLOWS]->(b)
    RETURN b.name
');
```

### Incoming relationships

```sql
-- Who follows Charlie?
SELECT cypher('
    MATCH (a)-[:FOLLOWS]->(b:Person {name: "Charlie"})
    RETURN a.name
');
```

## Multi-Hop Patterns

### Two hops

```sql
-- Friends of friends (people Alice's follows follow)
SELECT cypher('
    MATCH (a:Person {name: "Alice"})-[:FOLLOWS]->()-[:FOLLOWS]->(c)
    RETURN DISTINCT c.name
');
```

### Variable length paths

```sql
-- Everyone reachable from Alice in 1-3 hops
SELECT cypher('
    MATCH (a:Person {name: "Alice"})-[:FOLLOWS*1..3]->(b)
    RETURN DISTINCT b.name
');
```

## Aggregation

### Count connections

```sql
-- Follower counts
SELECT cypher('
    MATCH (a:Person)-[:FOLLOWS]->(b:Person)
    RETURN b.name, count(a) as followers
    ORDER BY followers DESC
');
```

### Collect into list

```sql
-- Group followers by person
SELECT cypher('
    MATCH (a:Person)-[:FOLLOWS]->(b:Person)
    RETURN b.name, collect(a.name) as followed_by
');
```

## Complex Patterns

### Multiple relationship types

```sql
-- Find mutual follows
SELECT cypher('
    MATCH (a:Person)-[:FOLLOWS]->(b:Person)-[:FOLLOWS]->(a)
    RETURN a.name, b.name
');
```

### OPTIONAL MATCH

```sql
-- All people and their followers (NULL if none)
SELECT cypher('
    MATCH (p:Person)
    OPTIONAL MATCH (follower)-[:FOLLOWS]->(p)
    RETURN p.name, count(follower) as follower_count
');
```

### Filter with WHERE

```sql
-- People followed by more than one person
SELECT cypher('
    MATCH (a:Person)-[:FOLLOWS]->(b:Person)
    WITH b, count(a) as followers
    WHERE followers > 1
    RETURN b.name, followers
');
```

## Working with Results in SQL

### Extract JSON fields

```sql
SELECT
    json_extract(value, '$.a.name') as person,
    json_extract(value, '$.b.name') as follows
FROM json_each(
    cypher('MATCH (a:Person)-[:FOLLOWS]->(b) RETURN a, b')
);
```

### Join with regular tables

```sql
-- Assuming you have a users table
WITH graph_data AS (
    SELECT
        json_extract(value, '$.name') as name,
        json_extract(value, '$.followers') as followers
    FROM json_each(
        cypher('MATCH (a)-[:FOLLOWS]->(b) RETURN b.name as name, count(a) as followers')
    )
)
SELECT u.email, g.followers
FROM users u
JOIN graph_data g ON u.username = g.name;
```

## Next Steps

- [Graph Algorithms in SQL](./sql-algorithms.md) - PageRank and community detection
- [SQL Interface Reference](../reference/sql-interface.md) - Complete SQL reference
