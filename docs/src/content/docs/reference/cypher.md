---
title: Cypher Support Reference
---

GraphQLite implements a substantial subset of openCypher. This page is a quick-reference index; details are in the sub-pages.

## Clauses

| Clause | Status | Notes |
|--------|--------|-------|
| `MATCH` | Supported | Node, relationship, variable-length, named path patterns |
| `OPTIONAL MATCH` | Supported | Left outer join semantics |
| `CREATE` | Supported | Nodes and relationships |
| `MERGE` | Supported | `ON CREATE SET`, `ON MATCH SET` |
| `SET` | Supported | Property assign, map replace (`=`), map merge (`+=`), label add |
| `REMOVE` | Supported | Property removal, label removal |
| `DELETE` | Supported | Nodes and relationships |
| `DETACH DELETE` | Supported | Cascading edge removal |
| `RETURN` | Supported | `AS`, `DISTINCT`, `ORDER BY`, `LIMIT`, `SKIP`, `*` |
| `WITH` | Supported | Aggregation, filtering, projection between clauses |
| `WHERE` | Supported | All predicates; pattern predicates |
| `UNWIND` | Supported | List expansion |
| `FOREACH` | Supported | Mutation inside list iteration |
| `UNION` / `UNION ALL` | Supported | |
| `LOAD CSV [WITH HEADERS] FROM` | Supported | Local files, `FIELDTERMINATOR`, `linenumber()`, `file()` |
| `FROM` | Supported | Multi-graph queries (GraphQLite extension) |
| `CALL {}` subqueries | Supported | Importing `WITH`, writes in the body |
| `CALL procedure` | Supported | Graph algorithms: `CALL pageRank() YIELD node, score` |
| `CREATE` / `DROP` / `SHOW INDEX` | Supported | A record only: each property is indexed already |
| `CREATE` / `DROP` / `SHOW CONSTRAINT` | Supported | Node `IS UNIQUE` and `IS NOT NULL` |
| `EXPLAIN` / `PROFILE` | Supported | `PROFILE` runs the query and gives the plan, rows and time |
| Label disjunction | Supported | `(n:A\|B)` in `MATCH` |
| Map projection | Supported | `n {.name, .*, k: expr}` |
| `CASE` in `SET` | **Not supported** | Use `CASE` in `RETURN`/`WITH` instead |
| Relationship indexes and constraints | **Not supported** | |

## Functions

| Category | Functions |
|----------|-----------|
| String | `toUpper`, `toLower`, `trim`, `ltrim`, `rtrim`, `btrim`, `substring`, `replace`, `reverse`, `left`, `right`, `split`, `toString`, `size`, `isEmpty`, `char_length`, `character_length` |
| Math | `abs`, `ceil`, `floor`, `round`, `sqrt`, `sign`, `log`, `log10`, `exp`, `e`, `pi`, `rand`, `toInteger`, `toFloat` |
| Trigonometry | `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `degrees`, `radians`, `cot`, `haversin`, `sinh`, `cosh`, `tanh`, `coth`, `isNaN` |
| List | `size`, `head`, `tail`, `last`, `range`, `collect`, `keys`, `reduce`, `[expr FOR x IN list [WHERE cond]]` |
| Aggregation | `count`, `sum`, `avg`, `min`, `max`, `collect`, `stdev`, `stdevp` |
| Entity | `id`, `elementId`, `labels`, `type`, `properties`, `startNode`, `endNode`, `nodes`, `relationships`, `length` |
| Type conversion | `toString`, `toInteger`, `toFloat`, `toBoolean`, `toStringOrNull`, `toIntegerOrNull`, `toFloatOrNull`, `toBooleanOrNull`, `valueType` |
| Temporal | `date`, `time`, `datetime`, `localdatetime`, `duration`, `datetime.fromepoch`, `datetime.fromepochmillis`, `duration.inDays`, `duration.inSeconds`, `date.truncate` |
| Spatial | `point`, `distance`, `point.withinBBox` |
| Predicate | `exists`, `coalesce`, `nullIf` |
| CASE | `CASE WHEN … THEN … END`, `CASE expr WHEN v THEN … END` |
| Graph algorithms | `pageRank`, `labelPropagation`, `louvain`, `dijkstra`, `astar`, `degreeCentrality`, `betweennessCentrality`, `closenessCentrality`, `eigenvectorCentrality`, `weaklyConnectedComponents`, `stronglyConnectedComponents`, `bfs`, `dfs`, `nodeSimilarity`, `knn`, `triangleCount`, `apsp`, `shortestPath` |

## Operators

| Category | Operators |
|----------|-----------|
| Arithmetic | `+`, `-`, `*`, `/`, `%` |
| Comparison | `=`, `<>`, `<`, `>`, `<=`, `>=` |
| Boolean | `AND`, `OR`, `NOT`, `XOR` |
| String | `STARTS WITH`, `ENDS WITH`, `CONTAINS`, `=~` (regex) |
| List | `IN`, `+` (concat), `[index]`, `[start..end]` (slice) |
| Null | `IS NULL`, `IS NOT NULL` |
| Property access | `.` (dot notation), `['key']` (subscript) |

## Not Supported

- `CASE` expressions in `SET` values
- Relationship property indexes and relationship constraints
- Procedures other than the graph algorithms

## Sub-pages

- [Clauses](../cypher-clauses/) — syntax, description, and examples for every clause
- [Functions](../cypher-functions/) — signature, return type, and example for every function
- [Operators](../cypher-operators/) — all operators with precedence table
