---
title: Cypher Clauses
slug: 0.9.4/reference/cypher-clauses
---

## MATCH

**Syntax**

```
MATCH pattern [WHERE condition]
```

Reads nodes and relationships matching the given pattern. Binds variables to matched entities. Multiple comma-separated patterns in one `MATCH` form a cross-product constrained by shared variables. A `WHERE` clause immediately following `MATCH` filters before joining subsequent clauses.

**Pattern forms**

| Pattern | Example |
|---------|---------|
| Node | `(n:Label)` |
| Relationship | `(a)-[r:TYPE]->(b)` |
| Undirected relationship | `(a)-[r:TYPE]-(b)` |
| Variable-length | `(a)-[*1..3]->(b)` |
| Named path | `p = (a)-[*]->(b)` |
| Multiple labels | `(n:Person:Employee)` |
| Property filter | `(n:Person {name: 'Alice'})` |

**Examples**

```cypher
MATCH (n:Person) RETURN n.name
```

```cypher
MATCH (a:Person)-[:KNOWS]->(b:Person)
WHERE a.name = 'Alice'
RETURN b.name
```

```cypher
MATCH p = (a)-[*1..3]->(b)
RETURN nodes(p)
```

***

## OPTIONAL MATCH

**Syntax**

```
OPTIONAL MATCH pattern [WHERE condition]
```

Left outer join. Rows from preceding clauses are preserved even when no match exists. Unmatched variables are bound to `null`.

**Example**

```cypher
MATCH (n:Person)
OPTIONAL MATCH (n)-[:HAS_PET]->(p:Pet)
RETURN n.name, p.name
```

***

## CREATE

**Syntax**

```
CREATE pattern
```

Creates nodes and/or relationships. Variables introduced in `CREATE` are available in subsequent clauses.

**Examples**

```cypher
CREATE (n:Person {name: 'Alice', age: 30})
```

```cypher
MATCH (a:Person {name: 'Alice'}), (b:Person {name: 'Bob'})
CREATE (a)-[:KNOWS {since: 2020}]->(b)
```

***

## MERGE

**Syntax**

```
MERGE pattern
[ON CREATE SET assignment [, assignment ...]]
[ON MATCH SET assignment [, assignment ...]]
```

Matches the pattern or creates it if no match exists. `ON CREATE SET` executes only on creation; `ON MATCH SET` executes only when the pattern already exists. Both subclauses are optional and independent.

**Examples**

```cypher
MERGE (n:Person {name: 'Alice'})
ON CREATE SET n.created = datetime()
ON MATCH SET n.updated = datetime()
```

```cypher
MERGE (a:Person {name: 'Bob'})-[:KNOWS]->(b:Person {name: 'Carol'})
```

***

## SET

**Syntax**

```
SET item [, item ...]
```

**Assignment forms**

| Form | Behavior |
|------|----------|
| `n.prop = expr` | Set or overwrite one property |
| `n = {map}` | Replace all properties with the map; unlisted properties are removed |
| `n += {map}` | Merge map into existing properties; unlisted properties are kept |
| `n:Label` | Add a label to a node |

**Examples**

```cypher
MATCH (n:Person {name: 'Alice'}) SET n.age = 31
```

```cypher
MATCH (n:Person {name: 'Alice'}) SET n = {name: 'Alice', age: 31}
```

```cypher
MATCH (n:Person {name: 'Alice'}) SET n += {age: 31, city: 'NYC'}
```

```cypher
MATCH (n:Person {name: 'Alice'}) SET n:Employee
```

***

## REMOVE

**Syntax**

```
REMOVE item [, item ...]
```

**Item forms**

| Form | Behavior |
|------|----------|
| `n.prop` | Delete a property from a node or relationship |
| `n:Label` | Remove a label from a node |

**Examples**

```cypher
MATCH (n:Person {name: 'Alice'}) REMOVE n.age
```

```cypher
MATCH (n:Employee) REMOVE n:Employee
```

***

## DELETE

**Syntax**

```
DELETE expr [, expr ...]
```

Deletes nodes or relationships. Attempting to delete a node that still has relationships raises an error. Use `DETACH DELETE` to cascade.

**Examples**

```cypher
MATCH (n:Temp) DELETE n
```

```cypher
MATCH (a)-[r:OLD]->(b) DELETE r
```

***

## DETACH DELETE

**Syntax**

```
DETACH DELETE expr [, expr ...]
```

Deletes a node and all its incident relationships in one operation.

**Example**

```cypher
MATCH (n:Person {name: 'Alice'}) DETACH DELETE n
```

***

## RETURN

**Syntax**

```
RETURN [DISTINCT] expr [AS alias] [, ...]
[ORDER BY expr [ASC|DESC] [, ...]]
[SKIP expr]
[LIMIT expr]
```

Projects query results into the result set. `*` expands to all in-scope variables.

**Modifiers**

| Modifier | Description |
|----------|-------------|
| `DISTINCT` | Remove duplicate rows from output |
| `AS alias` | Assign a column name |
| `ORDER BY expr [ASC\|DESC]` | Sort; default direction is `ASC` |
| `SKIP n` | Skip the first `n` rows |
| `LIMIT n` | Return at most `n` rows |
| `*` | Return all variables in scope |

**Examples**

```cypher
MATCH (n:Person)
RETURN n.name AS name, n.age
ORDER BY n.age DESC
LIMIT 10
```

```cypher
MATCH (n:Person) RETURN DISTINCT n.city
```

```cypher
MATCH (n) RETURN *
```

***

## WITH

**Syntax**

```
WITH [DISTINCT] expr [AS alias] [, ...]
[ORDER BY expr [ASC|DESC] [, ...]]
[SKIP expr]
[LIMIT expr]
[WHERE condition]
```

Pipelines intermediate results between query parts. Variables not listed in `WITH` go out of scope. Aggregation in `WITH` collapses rows; `WHERE` after `WITH` filters the aggregated result.

**Examples**

```cypher
MATCH (n:Person)-[:KNOWS]->(m)
WITH n, count(m) AS friends
WHERE friends > 3
RETURN n.name, friends
```

```cypher
MATCH (n:Person)
WITH n ORDER BY n.age LIMIT 5
MATCH (n)-[:KNOWS]->(m)
RETURN n.name, m.name
```

***

## WHERE

**Syntax**

```
WHERE condition
```

Filters rows. Appears after `MATCH`, `OPTIONAL MATCH`, or `WITH`. Supports all comparison operators, boolean operators, string predicates, `IS NULL`, `IS NOT NULL`, `IN`, and pattern predicates.

**Predicate forms**

| Predicate | Example |
|-----------|---------|
| Comparison | `n.age > 25` |
| String | `n.name STARTS WITH 'Al'` |
| Regex | `n.name =~ 'Al.*'` |
| Null check | `n.email IS NOT NULL` |
| List membership | `n.role IN ['admin', 'mod']` |
| Pattern existence | `(n)-[:KNOWS]->(m)` |
| Negated pattern | `NOT (n)-[:BLOCKED]->(m)` |
| exists\{} | `exists{(n)-[:KNOWS]->(m)}` |

**Examples**

```cypher
MATCH (n:Person)
WHERE n.age > 25 AND n.city = 'NYC'
RETURN n.name
```

```cypher
MATCH (n:Person)
WHERE (n)-[:KNOWS]->(:Person {name: 'Bob'})
RETURN n.name
```

***

## UNWIND

**Syntax**

```
UNWIND expr AS variable
```

Expands a list into one row per element. A `null` list produces zero rows. An empty list produces zero rows.

**Examples**

```cypher
UNWIND [1, 2, 3] AS x RETURN x
```

```cypher
MATCH (n:Person)
UNWIND labels(n) AS lbl
RETURN n.name, lbl
```

***

## FOREACH

**Syntax**

```
FOREACH (variable IN list | update_clause [update_clause ...])
```

Executes mutation clauses (`CREATE`, `MERGE`, `SET`, `REMOVE`, `DELETE`) for each element of a list. Variables introduced inside `FOREACH` are scoped to the body and not available outside. Nested `FOREACH` is not supported.

**Examples**

```cypher
FOREACH (name IN ['Alice', 'Bob', 'Carol'] |
  CREATE (:Person {name: name})
)
```

```cypher
MATCH p = (a:Person)-[:KNOWS*]->(b:Person)
FOREACH (n IN nodes(p) | SET n.visited = true)
```

***

## UNION / UNION ALL

**Syntax**

```
query
UNION [ALL]
query
```

Combines results from two queries. Column names and count must match. `UNION` deduplicates rows; `UNION ALL` preserves all rows including duplicates.

**Example**

```cypher
MATCH (n:Person) RETURN n.name AS name
UNION
MATCH (n:Company) RETURN n.name AS name
```

***

## LOAD CSV WITH HEADERS FROM

**Syntax**

```
LOAD CSV WITH HEADERS FROM 'file:///path/to/file.csv' AS row
[FIELDTERMINATOR char]
```

Reads a CSV file and binds each row as a map keyed by header names. All values are strings; use `toInteger()`, `toFloat()`, or `toBoolean()` as needed.

**Example**

```cypher
LOAD CSV WITH HEADERS FROM 'file:///data/people.csv' AS row
CREATE (:Person {name: row.name, age: toInteger(row.age)})
```

***

## FROM (Multi-graph)

**Syntax**

```
FROM 'db_path'
MATCH ...
```

GraphQLite extension clause. Queries a different SQLite database file. The target database must have the GraphQLite schema initialized.

**Example**

```cypher
FROM 'social.db'
MATCH (n:Person)
RETURN n.name
```

***

## Pattern Predicates in WHERE

A pattern used as a boolean expression inside `WHERE` evaluates to `true` if at least one match exists. Supports full pattern syntax including property filters, relationship type filters, and variable-length paths.

```cypher
WHERE (a)-[:KNOWS]->(b)
WHERE NOT (a)-[:BLOCKED]->(b)
WHERE (a)-[:KNOWS*1..3]->(b)
WHERE exists{(a)-[:KNOWS]->(:Person {active: true})}
```

Pattern predicates can reference variables bound in preceding `MATCH` clauses.
