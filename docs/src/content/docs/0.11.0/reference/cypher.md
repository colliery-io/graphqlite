---
title: Cypher Support Reference
slug: 0.11.0/reference/cypher
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
| `LOAD CSV WITH HEADERS FROM` | Supported | Local file paths |
| `FROM` | Supported | Multi-graph queries (GraphQLite extension) |
| `CALL {}` subqueries | **Not supported** | |
| `CALL procedure` | **Not supported** | No procedure registry |
| `CREATE INDEX` | **Not supported** | Schema is managed automatically |
| `CASE` in `SET` | **Not supported** | Use `CASE` in `RETURN`/`WITH` instead |
| Nested `FOREACH` | **Not supported** | |
| `EXPLAIN` / `PROFILE` | **Not supported** | |

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

* `CALL {}` correlated subqueries
* `CALL procedure(...)` procedure invocations
* `CREATE INDEX ON :Label(prop)`
* `EXPLAIN` / `PROFILE`
* `CASE` expressions on the left-hand side of `SET`
* Nested `FOREACH`

## Sub-pages

* [Clauses](../cypher-clauses/) — syntax, description, and examples for every clause
* [Functions](../cypher-functions/) — signature, return type, and example for every function
* [Operators](../cypher-operators/) — all operators with precedence table
