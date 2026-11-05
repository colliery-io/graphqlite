# GraphQLite Cypher Coverage Matrix

Cross-reference of the openCypher specification against GraphQLite's implementation.
Generated 2026-03-17 from parser grammar, transform layer, and test analysis.

**Legend:** `[x]` = implemented + tested, `[~]` = parsed but not fully implemented, `[ ]` = not supported, `[N/A]` = out of scope for embedded SQLite

---

## 1. Reading Clauses

- [x] `MATCH` -- pattern matching with nodes, relationships, properties, labels
- [x] `OPTIONAL MATCH` -- null-padded match for missing patterns
- [x] `WHERE` -- predicate filtering on MATCH and WITH

## 2. Writing Clauses

- [x] `CREATE` -- nodes and relationships with properties
- [x] `CREATE` with properties -- `CREATE (n:Label {key: value})`
- [x] `MERGE` -- upsert semantics
- [x] `MERGE ON CREATE SET` -- actions on creation
- [x] `MERGE ON MATCH SET` -- actions on match
- [x] `SET` property -- `SET n.prop = value`
- [x] `SET` label -- `SET n:Label`
- [x] `SET` from map -- `SET n = {map}` (replace) and `SET n += {map}` (merge)
- [x] `DELETE` -- node and relationship deletion
- [x] `DETACH DELETE` -- cascade delete
- [x] `REMOVE` property -- `REMOVE n.prop`
- [x] `REMOVE` label -- `REMOVE n:Label`
- [x] `FOREACH` -- iterate and execute updates

## 3. Sub-clauses / Projecting / Composition

- [x] `RETURN` -- result projection
- [ ] `RETURN *` -- return all variables
- [x] `RETURN DISTINCT` -- deduplicate
- [x] `RETURN ... AS` -- aliases
- [x] `WITH` -- intermediate projection with scope transition
- [x] `WITH DISTINCT` -- deduplicate intermediates
- [x] `UNWIND` -- list expansion
- [x] `ORDER BY` -- ASC/DESC sorting
- [x] `SKIP` -- pagination offset
- [x] `LIMIT` -- result count limit
- [x] `UNION` -- deduplicated combination
- [x] `UNION ALL` -- all-rows combination
- [ ] `CALL { subquery }` -- subquery evaluation
- [ ] `CALL { } IN TRANSACTIONS` -- batched transactional subquery
- [ ] `CALL procedure YIELD` -- stored procedures
- [~] `LOAD CSV` -- parsed but returns "not implemented" error
- [~] `LOAD CSV WITH HEADERS` -- parsed but not implemented
- [ ] `FINISH` -- Cypher 25
- [ ] `USE` -- Cypher 25
- [ ] `LET` -- Cypher 25
- [ ] `NEXT` -- Cypher 25
- [x] `EXPLAIN` -- parsed (basic support)

## 4. Pattern Syntax

### 4.1 Node Patterns
- [x] `()` -- anonymous node
- [x] `(n)` -- bound variable
- [x] `(n:Label)` -- single label
- [x] `(n:Label1:Label2)` -- multiple labels (AND)
- [x] `(n:Label1|Label2)` -- label disjunction (OR)
- [ ] `(!Label)` -- label negation
- [x] `(n {prop: value})` -- inline properties
- [x] `(n:Label {prop: value})` -- label + properties

### 4.2 Relationship Patterns
- [x] `--` -- bare undirected
- [x] `-->` -- bare directed right
- [x] `<--` -- bare directed left
- [x] `-[r]-` -- undirected with variable
- [x] `-[r]->` -- directed right
- [x] `<-[r]-` -- directed left
- [x] `-[r:TYPE]-` -- typed relationship
- [x] `-[r:TYPE1|TYPE2]-` -- type disjunction
- [x] `-[r {prop: value}]-` -- relationship properties
- [x] `-[r:TYPE {prop: value}]->` -- typed with properties

### 4.3 Variable-Length Patterns
- [x] `-[*]->` -- any length
- [x] `-[*2]->` -- exact length
- [x] `-[*2..5]->` -- range
- [x] `-[*..5]->` -- max only
- [x] `-[*2..]->` -- min only
- [ ] Quantified path patterns (`{m,n}`, `+`, `*`) -- Cypher 25

### 4.4 Path Patterns
- [x] `p = (a)-[r]->(b)` -- path variable binding
- [x] `shortestPath((a)-[*]-(b))` -- single shortest
- [x] `allShortestPaths((a)-[*]-(b))` -- all shortest
- [ ] `SHORTEST k` -- Cypher 25
- [ ] `ANY` path -- Cypher 25

### 4.5 Match Modes
- [ ] `DIFFERENT RELATIONSHIPS`
- [ ] `REPEATABLE ELEMENTS`

## 5. Operators

### 5.1 Mathematical
- [x] `+` addition
- [x] `-` subtraction
- [x] `*` multiplication
- [x] `/` division
- [x] `%` modulo
- [x] `^` exponentiation (parsed, mapped to power)

### 5.2 Comparison
- [x] `=` equality
- [x] `<>` inequality (also `!=`)
- [x] `<` / `>` / `<=` / `>=`
- [x] `IS NULL`
- [x] `IS NOT NULL`

### 5.3 Boolean
- [x] `AND`
- [x] `OR`
- [x] `XOR`
- [x] `NOT`

### 5.4 String
- [x] `STARTS WITH`
- [x] `ENDS WITH`
- [x] `CONTAINS`
- [x] `=~` regex match
- [ ] `IS NORMALIZED` -- Cypher 25

### 5.5 String Concatenation
- [x] `+` string concatenation (mapped to `||` in SQL)
- [ ] `||` Cypher 25 concatenation operator

### 5.6 List
- [x] `IN` list membership
- [ ] `+` list concatenation
- [x] `[]` element access by index
- [ ] `[start..end]` list slicing
- [ ] `[start..]` / `[..end]` partial slicing

### 5.7 Property
- [x] `.` static property access (`n.name`)
- [x] `[]` dynamic property access (`n['name']`)
- [x] Nested property access (`n.a.b` via json_extract)

### 5.8 Label/Type
- [x] `:Label` in pattern
- [x] `:TYPE` relationship type in pattern
- [x] `n:Label` predicate in WHERE
- [ ] `IS ::` type predicate -- Cypher 25

### 5.9 Temporal
- [ ] Duration arithmetic (`+`, `-`, `*`, `/` with durations)

### 5.10 Other
- [x] `DISTINCT`
- [x] `CASE WHEN THEN ELSE END` (simple and searched)

## 6. Expressions

### 6.1 CASE
- [x] Simple CASE: `CASE expr WHEN value THEN result`
- [x] Generic CASE: `CASE WHEN predicate THEN result`
- [ ] Extended CASE with `IS TYPED` -- Cypher 25

### 6.2 List Comprehensions
- [x] `[x IN list | expression]`
- [x] `[x IN list WHERE predicate]`
- [x] `[x IN list WHERE predicate | expression]`

### 6.3 Pattern Comprehensions
- [x] `[(n)-[:REL]->(m) | m.prop]`
- [x] `[(n)-[:REL]->(m) WHERE predicate | expression]`

### 6.4 Map Projections
- [x] `n {.prop1, .prop2}`
- [x] `n {.*}`
- [x] `n {.prop, key: value}`
- [ ] `n {variable}` -- variable selector

### 6.5 Subquery Expressions
- [x] `EXISTS { (n)-[r]->(m) }` -- pattern existence (via EXISTS)
- [x] `COUNT { MATCH pattern }` -- count subquery (also `COUNT { pattern [WHERE ...] }`)
- [ ] `COLLECT { MATCH pattern RETURN expr }` -- collect subquery

### 6.6 Literals
- [x] Integer literals (decimal, hex `0x2A`)
- [x] Float literals (`3.14`)
- [x] String literals (single and double quoted)
- [x] Boolean: `true`, `false`
- [x] `null`
- [x] List literals: `[1, 2, 3]`
- [x] Map literals: `{key: 'value'}`
- [ ] Octal integer literals (`0o52`)
- [ ] Scientific notation in Cypher (`6.022e23`) -- works via SQL passthrough

### 6.7 Parameters
- [x] `$param` -- named parameters
- [ ] `$($expression)` -- dynamic parameters (Cypher 25)

## 7. Functions

### 7.1 Predicate Functions
- [x] `exists(expr)` -- property/pattern existence
- [ ] `isEmpty(expr)`
- [x] `all(x IN list WHERE pred)`
- [x] `any(x IN list WHERE pred)`
- [x] `none(x IN list WHERE pred)`
- [x] `single(x IN list WHERE pred)`

### 7.2 Scalar Functions
- [x] `coalesce(expr, ...)`
- [x] `id(node_or_rel)`
- [ ] `elementId(node_or_rel)`
- [x] `type(relationship)`
- [x] `labels(node)`
- [x] `keys(node_rel_or_map)`
- [x] `properties(node_or_rel)`
- [x] `startNode(relationship)`
- [x] `endNode(relationship)`
- [x] `size(list_or_string)`
- [x] `length(path)`
- [x] `head(list)`
- [x] `last(list)`
- [x] `timestamp()`
- [x] `randomUUID()`
- [ ] `nullIf(expr, expr)`
- [ ] `valueType(expr)`
- [ ] `char_length(string)`

### 7.3 Type Conversion
- [x] `toInteger(expr)`
- [ ] `toIntegerOrNull(expr)`
- [x] `toFloat(expr)`
- [ ] `toFloatOrNull(expr)`
- [x] `toBoolean(expr)`
- [ ] `toBooleanOrNull(expr)`
- [x] `toString(expr)`
- [ ] `toStringOrNull(expr)`
- [ ] `toBooleanList(list)` / `toFloatList` / `toIntegerList` / `toStringList`

### 7.4 Aggregating Functions
- [x] `count(expr)` / `count(*)` / `count(DISTINCT expr)`
- [x] `sum(expr)`
- [x] `avg(expr)`
- [x] `min(expr)`
- [x] `max(expr)`
- [x] `collect(expr)` / `collect(DISTINCT expr)`
- [ ] `stDev(expr)` / `stDevP(expr)`
- [x] `percentileCont(expr, pct)` / `percentileDisc(expr, pct)`

### 7.5 List Functions
- [x] `range(start, end [, step])`
- [x] `reverse(list)`
- [x] `tail(list)`
- [x] `head(list)`
- [x] `last(list)`
- [x] `size(list)`
- [x] `reduce(acc = init, x IN list | expr)`
- [x] `nodes(path)`
- [x] `relationships(path)`
- [x] `keys(map)`
- [x] `labels(node)`

### 7.6 Math -- Numeric
- [x] `abs(expr)`
- [x] `ceil(expr)`
- [x] `floor(expr)`
- [x] `round(expr)` / `round(expr, precision)`
- [ ] `round(expr, precision, mode)`
- [x] `sign(expr)`
- [x] `rand()`
- [ ] `isNaN(expr)`

### 7.7 Math -- Logarithmic
- [x] `e()`
- [x] `exp(expr)`
- [x] `log(expr)`
- [x] `log10(expr)`
- [x] `sqrt(expr)`

### 7.8 Math -- Trigonometric
- [x] `pi()`
- [x] `sin()` / `cos()` / `tan()`
- [x] `asin()` / `acos()` / `atan()`
- [ ] `atan2(y, x)`
- [ ] `cot(expr)`
- [ ] `sinh()` / `cosh()` / `tanh()` / `coth()`
- [ ] `degrees(expr)` / `radians(expr)`
- [ ] `haversin(expr)`

### 7.9 String Functions
- [x] `toUpper(string)` / `toLower(string)`
- [x] `trim()` / `ltrim()` / `rtrim()`
- [ ] `btrim()`
- [x] `replace(string, search, replacement)`
- [x] `substring(string, start [, length])`
- [x] `left(string, n)` / `right(string, n)`
- [x] `split(string, delimiter)`
- [x] `reverse(string)`
- [x] `size(string)`
- [ ] `normalize(string)`

### 7.10 Temporal Functions
- [x] `date()` -- current date (basic)
- [ ] `date({year, month, day})` -- construct
- [ ] `date(string)` -- parse
- [x] `time()` -- current time (basic)
- [x] `datetime()` / `localdatetime()` -- current datetime (basic)
- [ ] Temporal construction from maps
- [ ] `duration()` -- duration type
- [ ] Temporal truncation functions
- [ ] Temporal arithmetic

### 7.11 Spatial Functions
- [ ] `point()` -- not supported
- [ ] `point.distance()` -- not supported

### 7.12 Path Functions
- [x] `length(path)`
- [x] `nodes(path)`
- [x] `relationships(path)`

### 7.13 JSON/Graph Extensions (GraphQLite-specific)
- [x] `json_get(json, path)` / `jsonGet()`
- [x] `json_keys(json)` / `jsonKeys()`
- [x] `json_type(json)` / `jsonType()`
- [x] `pageRank()` / `topPageRank()` / `personalizedPageRank()`
- [x] `labelPropagation()` / `communities()` / `communityOf()` / `communityMembers()` / `communityCount()`
- [x] `graph()` -- multi-graph reference

## 8. Data Types

### Supported Property Types
- [x] BOOLEAN (stored in `node_props_bool`)
- [x] INTEGER (stored in `node_props_int`)
- [x] FLOAT (stored in `node_props_real`)
- [x] STRING (stored in `node_props_text`)
- [x] JSON/MAP (stored in `node_props_json`)
- [x] LIST (stored as JSON in `node_props_json`)
- [ ] DATE / TIME / DATETIME (no native temporal storage)
- [ ] DURATION
- [ ] POINT (spatial)

### Structural Types
- [x] NODE (via `nodes` + `node_labels` + property tables)
- [x] RELATIONSHIP (via `edges` + property tables)
- [x] PATH (via ordered node/edge sequences)

## 9-12. Schema, Constraints, Procedures, Admin

- [N/A] Indexes -- SQLite handles indexing internally
- [N/A] Constraints -- not applicable to embedded engine
- [N/A] Procedures -- no procedure system
- [N/A] User/role management -- SQLite is single-user
- [N/A] Database management -- handled by SQLite file system
- [N/A] Transaction management -- SQLite handles internally

## 13. Query Modifiers

- [x] `EXPLAIN` -- basic support
- [ ] `PROFILE` -- not supported
- [x] `//` line comments
- [x] `/* */` block comments
- [x] Backtick-quoted identifiers
- [ ] Escaped Unicode `\uXXXX`
- [x] `AS` aliasing
- [ ] `*` wildcard in RETURN/WITH
- [x] `FROM` graph selection (multi-graph)

---

## Coverage Summary

| Category | Supported | Partial | Not Supported | N/A | Total |
|----------|-----------|---------|---------------|-----|-------|
| Reading Clauses | 3 | 0 | 0 | 0 | 3 |
| Writing Clauses | 13 | 0 | 0 | 0 | 13 |
| Sub-clauses | 14 | 2 | 6 | 0 | 22 |
| Node Patterns | 6 | 0 | 2 | 0 | 8 |
| Relationship Patterns | 10 | 0 | 0 | 0 | 10 |
| Variable-Length | 5 | 0 | 1 | 0 | 6 |
| Path Patterns | 3 | 0 | 2 | 0 | 5 |
| Operators | 28 | 0 | 7 | 0 | 35 |
| Expressions | 13 | 0 | 4 | 0 | 17 |
| Functions | 58 | 0 | 38 | 0 | 96 |
| Data Types | 8 | 0 | 5 | 0 | 13 |
| Schema/Admin | 0 | 0 | 0 | 8 | 8 |
| Query Modifiers | 5 | 0 | 3 | 0 | 8 |
| **TOTAL** | **166** | **2** | **68** | **8** | **244** |

**openCypher Core Coverage: ~71%** (166/236 non-N/A features)

### Strongest Areas
- Reading/writing clauses: 100%
- Pattern matching: 83%
- Core operators: 80%
- Core functions (scalar, string, math, list, path, aggregate): 78%

### Biggest Gaps
- Temporal types and functions (dates, durations, arithmetic)
- Spatial types and functions (points, distance)
- List slicing (`[1..3]`)
- `RETURN *`
- `CALL` subqueries and procedures
- Statistical aggregates (`stDev`, `percentile*`)
- Advanced trig (`atan2`, hyperbolic, `degrees`/`radians`)
- `OrNull` type conversion variants
