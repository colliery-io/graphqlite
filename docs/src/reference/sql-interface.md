# SQL Interface

GraphQLite works as a standard SQLite extension, providing the `cypher()` function.

## Loading the Extension

### SQLite CLI

```bash
sqlite3 graph.db
.load /path/to/graphqlite
```

Or with automatic extension loading:

```bash
sqlite3 -cmd ".load /path/to/graphqlite" graph.db
```

### Programmatically

```sql
SELECT load_extension('/path/to/graphqlite');
```

## The cypher() Function

### Basic Usage

```sql
SELECT cypher('MATCH (n) RETURN n.name');
```

### With Parameters

```sql
SELECT cypher(
    'MATCH (n:Person {name: $name}) RETURN n',
    '{"name": "Alice"}'
);
```

### Return Format

The `cypher()` function returns results as JSON:

```sql
SELECT cypher('MATCH (n:Person) RETURN n.name, n.age');
-- Returns: [{"n.name": "Alice", "n.age": 30}, {"n.name": "Bob", "n.age": 25}]
```

## Working with Results

### Extract Values with JSON Functions

```sql
SELECT json_extract(value, '$.n.name') AS name
FROM json_each(cypher('MATCH (n:Person) RETURN n'));
```

### Algorithm Results

```sql
SELECT
    json_extract(value, '$.node_id') AS id,
    json_extract(value, '$.score') AS score
FROM json_each(cypher('RETURN pageRank()'))
ORDER BY score DESC
LIMIT 10;
```

### Join with Regular Tables

```sql
-- Assuming you have a regular 'users' table
SELECT u.email, json_extract(g.value, '$.degree')
FROM users u
JOIN json_each(cypher('RETURN degreeCentrality()')) g
    ON u.id = json_extract(g.value, '$.user_id');
```

## Write Operations

```sql
-- Create nodes
SELECT cypher('CREATE (n:Person {name: "Alice", age: 30})');

-- Create relationships
SELECT cypher('
    MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"})
    CREATE (a)-[:KNOWS]->(b)
');

-- Update properties
SELECT cypher('
    MATCH (n:Person {name: "Alice"})
    SET n.age = 31
');

-- Delete
SELECT cypher('
    MATCH (n:Person {name: "Alice"})
    DETACH DELETE n
');
```

## Schema Tables

GraphQLite creates these tables automatically:

### nodes

```sql
SELECT * FROM nodes;
-- id, user_id, label
```

### edges

```sql
SELECT * FROM edges;
-- id, source_id, target_id, label (relationship type)
```

### Property Tables

```sql
SELECT * FROM node_props_text;   -- id, node_id, key, value
SELECT * FROM node_props_int;    -- id, node_id, key, value
SELECT * FROM node_props_real;   -- id, node_id, key, value
SELECT * FROM node_props_bool;   -- id, node_id, key, value

SELECT * FROM edge_props_text;   -- id, edge_id, key, value
SELECT * FROM edge_props_int;    -- id, edge_id, key, value
SELECT * FROM edge_props_real;   -- id, edge_id, key, value
SELECT * FROM edge_props_bool;   -- id, edge_id, key, value
```

## Direct SQL Access

You can query the underlying tables directly for debugging or advanced use cases:

```sql
-- Count nodes by label
SELECT label, COUNT(*) FROM nodes GROUP BY label;

-- Find nodes with a specific property
SELECT n.user_id, p.value
FROM nodes n
JOIN node_props_text p ON n.id = p.node_id
WHERE p.key = 'name';

-- Find edges between specific nodes
SELECT e.*, source.user_id AS from_id, target.user_id AS to_id
FROM edges e
JOIN nodes source ON e.source_id = source.id
JOIN nodes target ON e.target_id = target.id;
```

## Transaction Support

GraphQLite respects SQLite transactions:

```sql
BEGIN;
SELECT cypher('CREATE (a:Person {name: "Alice"})');
SELECT cypher('CREATE (b:Person {name: "Bob"})');
SELECT cypher('MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"}) CREATE (a)-[:KNOWS]->(b)');
COMMIT;
```

Or rollback on error:

```sql
BEGIN;
SELECT cypher('CREATE (n:Person {name: "Test"})');
ROLLBACK;  -- Node is not created
```
