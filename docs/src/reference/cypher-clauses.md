# Cypher Clauses

## Reading Clauses

### MATCH

Find patterns in the graph:

```cypher
MATCH (n:Person) RETURN n
MATCH (a)-[:KNOWS]->(b) RETURN a, b
MATCH (n:Person {name: 'Alice'}) RETURN n
```

### Shortest Path Patterns

Find shortest paths between nodes:

```cypher
// Find a single shortest path
MATCH p = shortestPath((a:Person {name: 'Alice'})-[*]-(b:Person {name: 'Bob'}))
RETURN p, length(p)

// Find all shortest paths (all paths with minimum length)
MATCH p = allShortestPaths((a:Person)-[*]-(b:Person))
WHERE a.name = 'Alice' AND b.name = 'Bob'
RETURN p

// With relationship type filter
MATCH p = shortestPath((a)-[:KNOWS*]->(b))
RETURN nodes(p), relationships(p)

// With length constraints
MATCH p = shortestPath((a)-[*..10]->(b))
RETURN p
```

### OPTIONAL MATCH

Like MATCH, but returns NULL for non-matches (left join semantics):

```cypher
MATCH (p:Person)
OPTIONAL MATCH (p)-[:MANAGES]->(e)
RETURN p.name, e.name
```

### WHERE

Filter results:

```cypher
MATCH (n:Person)
WHERE n.age > 21 AND n.city = 'NYC'
RETURN n
```

## Writing Clauses

### CREATE

Create nodes and relationships:

```cypher
CREATE (n:Person {name: 'Alice', age: 30})
CREATE (a)-[:KNOWS {since: 2020}]->(b)
```

### MERGE

Create if not exists, match if exists:

```cypher
MERGE (n:Person {name: 'Alice'})
ON CREATE SET n.created = timestamp()
ON MATCH SET n.updated = timestamp()
```

### SET

Update properties:

```cypher
MATCH (n:Person {name: 'Alice'})
SET n.age = 31, n.city = 'LA'
```

Add labels:

```cypher
MATCH (n:Person {name: 'Alice'})
SET n:Employee
```

### REMOVE

Remove properties:

```cypher
MATCH (n:Person {name: 'Alice'})
REMOVE n.temporary_field
```

Remove labels:

```cypher
MATCH (n:Person:Employee {name: 'Alice'})
REMOVE n:Employee
```

### DELETE

Delete nodes (must have no relationships):

```cypher
MATCH (n:Person {name: 'Alice'})
DELETE n
```

### DETACH DELETE

Delete nodes and all their relationships:

```cypher
MATCH (n:Person {name: 'Alice'})
DETACH DELETE n
```

## Composing Clauses

### WITH

Chain query parts, aggregation, and filtering:

```cypher
MATCH (p:Person)-[:WORKS_AT]->(c:Company)
WITH c, count(p) as employee_count
WHERE employee_count > 10
RETURN c.name, employee_count
```

### UNWIND

Expand a list into rows:

```cypher
UNWIND [1, 2, 3] AS x
RETURN x

UNWIND $names AS name
CREATE (n:Person {name: name})
```

### FOREACH

Iterate and perform updates:

```cypher
MATCH p = (start)-[*]->(end)
FOREACH (n IN nodes(p) | SET n.visited = true)
```

### LOAD CSV

Import data from CSV files:

```cypher
// With headers (access columns by name)
LOAD CSV WITH HEADERS FROM 'file:///people.csv' AS row
CREATE (n:Person {name: row.name, age: toInteger(row.age)})

// Without headers (access columns by index)
LOAD CSV FROM 'file:///data.csv' AS row
CREATE (n:Item {id: row[0], value: row[1]})

// Custom field terminator
LOAD CSV WITH HEADERS FROM 'file:///data.tsv' AS row FIELDTERMINATOR '\t'
CREATE (n:Record {field1: row.col1})
```

**Note**: File paths are relative to the current working directory. Use `file:///` prefix for local files.

## Multi-Graph Queries

### FROM Clause

Query specific graphs when using GraphManager (multi-graph support):

```cypher
// Query a specific graph
MATCH (n:Person) FROM social
RETURN n.name

// Combined with other clauses
MATCH (p:Person) FROM social
WHERE p.age > 21
RETURN p.name, graph(p) AS source_graph
```

The `graph()` function returns which graph a node came from.

## Combining Results

### UNION

Combine results from multiple queries, removing duplicates:

```cypher
MATCH (n:Person) WHERE n.city = 'NYC' RETURN n.name
UNION
MATCH (n:Person) WHERE n.age > 50 RETURN n.name
```

### UNION ALL

Combine results keeping all rows (including duplicates):

```cypher
MATCH (a:Person)-[:KNOWS]->(b) RETURN b.name AS connection
UNION ALL
MATCH (a:Person)-[:WORKS_WITH]->(b) RETURN b.name AS connection
```

## Return Clause

### RETURN

Specify what to return:

```cypher
MATCH (n:Person) RETURN n
MATCH (n:Person) RETURN n.name, n.age
MATCH (n:Person) RETURN n.name AS name
```

### DISTINCT

Remove duplicates:

```cypher
MATCH (n:Person)-[:KNOWS]->(m)
RETURN DISTINCT m.city
```

### ORDER BY

Sort results:

```cypher
MATCH (n:Person)
RETURN n.name, n.age
ORDER BY n.age DESC, n.name ASC
```

### SKIP and LIMIT

Pagination:

```cypher
MATCH (n:Person)
RETURN n
ORDER BY n.name
SKIP 10
LIMIT 5
```

## Aggregation

Use aggregate functions in RETURN or WITH:

```cypher
MATCH (p:Person)-[:WORKS_AT]->(c:Company)
RETURN c.name, count(p), avg(p.salary), collect(p.name)
```

See [Functions Reference](./cypher-functions.md) for all aggregate functions.
