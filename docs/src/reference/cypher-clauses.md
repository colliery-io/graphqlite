# Cypher Clauses

## Reading Clauses

### MATCH

Find patterns in the graph:

```cypher
MATCH (n:Person) RETURN n
MATCH (a)-[:KNOWS]->(b) RETURN a, b
MATCH (n:Person {name: 'Alice'}) RETURN n
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
