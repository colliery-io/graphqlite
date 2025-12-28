# Getting Started with SQL

This tutorial shows how to use GraphQLite directly from the SQLite command line.

## Prerequisites

- SQLite3 CLI installed
- GraphQLite extension built (`make extension`)

## Step 1: Load the Extension

```bash
sqlite3 my_graph.db
```

```sql
.load build/graphqlite.dylib
-- On Linux: .load build/graphqlite.so
-- On Windows: .load build/graphqlite.dll
```

## Step 2: Create Nodes

```sql
-- Create people
SELECT cypher('CREATE (a:Person {name: "Alice", age: 30})');
SELECT cypher('CREATE (b:Person {name: "Bob", age: 25})');
SELECT cypher('CREATE (c:Person {name: "Charlie", age: 35})');
```

## Step 3: Create Relationships

```sql
-- Alice knows Bob
SELECT cypher('
    MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"})
    CREATE (a)-[:KNOWS]->(b)
');

-- Bob knows Charlie
SELECT cypher('
    MATCH (b:Person {name: "Bob"}), (c:Person {name: "Charlie"})
    CREATE (b)-[:KNOWS]->(c)
');
```

## Step 4: Query the Graph

```sql
-- Find all people
SELECT cypher('MATCH (p:Person) RETURN p.name, p.age');

-- Find relationships
SELECT cypher('MATCH (a:Person)-[:KNOWS]->(b:Person) RETURN a.name, b.name');

-- Friends of friends
SELECT cypher('
    MATCH (a:Person {name: "Alice"})-[:KNOWS]->()-[:KNOWS]->(fof)
    RETURN fof.name
');
```

## Step 5: Using Parameters

```sql
-- Safer queries with parameters
SELECT cypher(
    'MATCH (p:Person {name: $name}) RETURN p.age',
    '{"name": "Alice"}'
);
```

## Complete Example

Save this as `getting_started.sql`:

```sql
.load build/graphqlite.dylib

-- Create nodes
SELECT cypher('CREATE (a:Person {name: "Alice", age: 30})');
SELECT cypher('CREATE (b:Person {name: "Bob", age: 25})');
SELECT cypher('CREATE (c:Person {name: "Charlie", age: 35})');

-- Create relationships
SELECT cypher('
    MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"})
    CREATE (a)-[:KNOWS]->(b)
');
SELECT cypher('
    MATCH (b:Person {name: "Bob"}), (c:Person {name: "Charlie"})
    CREATE (b)-[:KNOWS]->(c)
');

-- Query
SELECT 'All people:';
SELECT cypher('MATCH (p:Person) RETURN p.name, p.age');

SELECT '';
SELECT 'Who knows who:';
SELECT cypher('MATCH (a:Person)-[:KNOWS]->(b:Person) RETURN a.name, b.name');

SELECT '';
SELECT 'Friends of friends:';
SELECT cypher('
    MATCH (a:Person {name: "Alice"})-[:KNOWS]->()-[:KNOWS]->(fof)
    RETURN fof.name
');
```

Run it:

```bash
sqlite3 < getting_started.sql
```

## Next Steps

- [Query Patterns](./sql-patterns.md) - More complex pattern matching
- [Graph Algorithms in SQL](./sql-algorithms.md) - PageRank, communities
