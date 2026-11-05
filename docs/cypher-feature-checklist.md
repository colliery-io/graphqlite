# Complete Cypher Language Feature Checklist

Comprehensive taxonomy of all Cypher language features based on the openCypher specification
(openCypher 9) and Neo4j Cypher Manual (Cypher 25). This document covers the full spec for
comparison purposes.

---

## 1. Reading Clauses

- [ ] `MATCH` -- specify patterns to search for in the graph
- [ ] `OPTIONAL MATCH` -- like MATCH but uses nulls for missing parts
- [ ] `WHERE` -- filter/constrain patterns in MATCH or pipe results from WITH

## 2. Writing Clauses

- [ ] `CREATE` -- create nodes and relationships
- [ ] `CREATE` with properties -- `CREATE (n:Label {key: value})`
- [ ] `MERGE` -- ensure a pattern exists (upsert)
- [ ] `MERGE ON CREATE SET` -- actions when MERGE creates
- [ ] `MERGE ON MATCH SET` -- actions when MERGE finds existing
- [ ] `SET` -- update properties on nodes/relationships
- [ ] `SET` label -- add labels to nodes (`SET n:Label`)
- [ ] `SET` property -- set single property (`SET n.prop = value`)
- [ ] `SET` properties from map -- `SET n = {map}` or `SET n += {map}`
- [ ] `DELETE` -- delete nodes and relationships
- [ ] `DETACH DELETE` -- delete node and all its relationships
- [ ] `REMOVE` -- remove properties from nodes/relationships
- [ ] `REMOVE` label -- remove labels from nodes (`REMOVE n:Label`)
- [ ] `FOREACH` -- iterate over a list and execute update operations

## 3. Sub-clauses / Projecting / Composition

- [ ] `RETURN` -- define what to include in query result set
- [ ] `RETURN *` -- return all bound variables
- [ ] `RETURN DISTINCT` -- deduplicate results
- [ ] `RETURN ... AS` -- alias return columns
- [ ] `WITH` -- chain query parts, piping results
- [ ] `WITH DISTINCT` -- deduplicate intermediate results
- [ ] `UNWIND` -- expand a list into a sequence of rows
- [ ] `ORDER BY` -- sort output (ASC/DESC)
- [ ] `SKIP` / `OFFSET` -- skip first N rows
- [ ] `LIMIT` -- constrain number of output rows
- [ ] `UNION` -- combine results from multiple queries, deduplicated
- [ ] `UNION ALL` -- combine results retaining duplicates
- [ ] `CALL { subquery }` -- evaluate a subquery
- [ ] `CALL { } IN TRANSACTIONS` -- batched transactional subquery
- [ ] `CALL procedure YIELD` -- invoke stored procedure
- [ ] `LOAD CSV` -- import data from CSV files
- [ ] `LOAD CSV WITH HEADERS` -- CSV import with column headers
- [ ] `FINISH` -- execute query with no result output
- [ ] `USE` -- select which graph/database to query
- [ ] `LET` -- bind values to variables (Cypher 25)
- [ ] `FILTER` -- independent filtering clause (Cypher 25)
- [ ] `NEXT` -- sequential query chaining (Cypher 25)

## 4. Pattern Syntax

### 4.1 Node Patterns
- [ ] `()` -- anonymous node
- [ ] `(n)` -- bound node variable
- [ ] `(n:Label)` -- node with label
- [ ] `(n:Label1:Label2)` -- node with multiple labels (AND)
- [x] `(n:Label1|Label2)` -- node with label disjunction (OR)
- [ ] `(!Label)` -- label negation
- [ ] `(n {prop: value})` -- node with inline properties
- [ ] `(n:Label {prop: value})` -- node with label and properties

### 4.2 Relationship Patterns
- [ ] `-[r]-` -- undirected relationship
- [ ] `-[r]->` -- right-directed relationship
- [ ] `<-[r]-` -- left-directed relationship
- [ ] `-[r:TYPE]-` -- relationship with type
- [ ] `-[r:TYPE1|TYPE2]-` -- relationship type disjunction
- [ ] `-[r {prop: value}]-` -- relationship with properties
- [ ] `-[r:TYPE {prop: value}]->` -- typed relationship with properties

### 4.3 Variable-Length Patterns
- [ ] `-[*]->` -- any length
- [ ] `-[*2]->` -- exact length
- [ ] `-[*2..5]->` -- length range
- [ ] `-[*..5]->` -- max length
- [ ] `-[*2..]->` -- min length
- [ ] Quantified path patterns: `{m,n}`, `+`, `*` (Cypher 25)

### 4.4 Path Patterns
- [ ] `p = (a)-[r]->(b)` -- bind path to variable
- [ ] `shortestPath((a)-[*]-(b))` -- single shortest path
- [ ] `allShortestPaths((a)-[*]-(b))` -- all shortest paths
- [ ] `SHORTEST k` -- k shortest paths (Cypher 25)
- [ ] `ALL SHORTEST` -- all shortest (Cypher 25)
- [ ] `SHORTEST k GROUPS` -- shortest groups (Cypher 25)
- [ ] `ANY` -- any path (Cypher 25)
- [ ] Non-linear patterns (equijoins) (Cypher 25)

### 4.5 Match Modes
- [ ] `DIFFERENT RELATIONSHIPS` (default)
- [ ] `REPEATABLE ELEMENTS`

## 5. Operators

### 5.1 Mathematical Operators
- [ ] `+` -- addition
- [ ] `-` -- subtraction
- [ ] `*` -- multiplication
- [ ] `/` -- division
- [ ] `%` -- modulo
- [ ] `^` -- exponentiation

### 5.2 Comparison Operators
- [ ] `=` -- equality
- [ ] `<>` -- inequality
- [ ] `<` -- less than
- [ ] `>` -- greater than
- [ ] `<=` -- less than or equal
- [ ] `>=` -- greater than or equal
- [ ] `IS NULL`
- [ ] `IS NOT NULL`

### 5.3 Boolean Operators
- [ ] `AND`
- [ ] `OR`
- [ ] `XOR`
- [ ] `NOT`

### 5.4 String Operators
- [ ] `STARTS WITH`
- [ ] `ENDS WITH`
- [ ] `CONTAINS`
- [ ] `=~` -- regular expression match
- [ ] `IS NORMALIZED` / `IS NOT NORMALIZED` (Cypher 25)

### 5.5 String Concatenation
- [ ] `+` -- string concatenation
- [ ] `||` -- string concatenation (Cypher 25)

### 5.6 List Operators
- [ ] `IN` -- list membership
- [ ] `+` -- list concatenation
- [ ] `||` -- list concatenation (Cypher 25)
- [ ] `[]` -- element access by index
- [ ] `[start..end]` -- list slicing
- [ ] `[start..]` -- slice from start
- [ ] `[..end]` -- slice to end

### 5.7 Property Operators
- [ ] `.` -- static property access (`n.name`)
- [ ] `[]` -- dynamic property access (`n['name']`)

### 5.8 Label/Type Operators
- [ ] `:Label` -- label check in pattern
- [ ] `:TYPE` -- relationship type check in pattern
- [ ] `n:Label` -- label predicate in WHERE
- [ ] `IS ::` -- type predicate (Cypher 25)
- [ ] `IS NOT ::` -- negated type predicate (Cypher 25)

### 5.9 Temporal Operators
- [ ] `+` -- add duration to temporal
- [ ] `-` -- subtract duration from temporal
- [ ] `*` -- multiply duration by number
- [ ] `/` -- divide duration by number

### 5.10 Other Operators
- [ ] `DISTINCT` -- deduplicate values
- [ ] `CASE ... WHEN ... THEN ... ELSE ... END`

## 6. Expressions

### 6.1 CASE Expressions
- [ ] Simple CASE: `CASE expr WHEN value THEN result ... ELSE default END`
- [ ] Generic CASE: `CASE WHEN predicate THEN result ... ELSE default END`
- [ ] Extended CASE with `IS NULL` (Cypher 25)
- [ ] Extended CASE with `IS TYPED` / `IS NOT TYPED` (Cypher 25)
- [ ] Extended CASE with comparison operators (Cypher 25)

### 6.2 List Comprehensions
- [ ] `[x IN list | expression]` -- map over list
- [ ] `[x IN list WHERE predicate]` -- filter list
- [ ] `[x IN list WHERE predicate | expression]` -- filter and map

### 6.3 Pattern Comprehensions
- [ ] `[(n)-[:REL]->(m) | m.prop]` -- match pattern and project
- [ ] `[(n)-[:REL]->(m) WHERE predicate | expression]` -- with filter

### 6.4 Map Projections
- [ ] `n {.prop1, .prop2}` -- select specific properties
- [ ] `n {.*}` -- all properties
- [ ] `n {.prop, key: value}` -- mix of selectors and literal entries
- [ ] `n {variable}` -- variable selector

### 6.5 Subquery Expressions
- [x] `EXISTS { MATCH pattern }` -- existential subquery
- [x] `COUNT { MATCH pattern }` -- count subquery (also `COUNT { pattern [WHERE ...] }`)
- [ ] `COLLECT { MATCH pattern RETURN expr }` -- collect subquery

### 6.6 Literals
- [ ] Integer literals: `42`, `0x2A`, `052`, `0o52`
- [ ] Float literals: `3.14`, `6.022e23`
- [ ] String literals: `'single'`, `"double"`
- [ ] Boolean literals: `true`, `false`
- [ ] `null`
- [ ] List literals: `[1, 2, 3]`
- [ ] Map literals: `{key: 'value', key2: 42}`

### 6.7 Parameters
- [ ] `$param` -- parameter reference
- [ ] `$($expression)` -- dynamic parameter (Cypher 25)

## 7. Functions

### 7.1 Predicate Functions
- [ ] `exists(expr)` -- property exists or pattern match exists
- [ ] `isEmpty(expr)` -- LIST, MAP, or STRING is empty
- [ ] `all(x IN list WHERE predicate)` -- all elements satisfy predicate
- [ ] `any(x IN list WHERE predicate)` -- at least one element satisfies
- [ ] `none(x IN list WHERE predicate)` -- no elements satisfy
- [ ] `single(x IN list WHERE predicate)` -- exactly one element satisfies

### 7.2 Scalar Functions
- [ ] `coalesce(expr, expr, ...)` -- first non-null value
- [ ] `id(node_or_rel)` -- internal ID (deprecated)
- [ ] `elementId(node_or_rel)` -- element identifier
- [ ] `type(relationship)` -- relationship type as string
- [ ] `labels(node)` -- list of labels
- [ ] `keys(node_rel_or_map)` -- list of property keys
- [ ] `properties(node_or_rel)` -- map of properties
- [ ] `startNode(relationship)` -- start node
- [ ] `endNode(relationship)` -- end node
- [ ] `size(list_or_string)` -- length of list or string
- [ ] `length(path)` -- length of path
- [ ] `head(list)` -- first element
- [ ] `last(list)` -- last element
- [ ] `timestamp()` -- milliseconds since epoch
- [ ] `randomUUID()` -- generate random UUID
- [ ] `nullIf(expr, expr)` -- null if parameters equal
- [ ] `valueType(expr)` -- string describing value type
- [ ] `char_length(string)` / `character_length(string)` -- character count

### 7.3 Type Conversion Functions
- [ ] `toInteger(expr)` -- convert to integer
- [ ] `toIntegerOrNull(expr)` -- convert to integer or null
- [ ] `toFloat(expr)` -- convert to float
- [ ] `toFloatOrNull(expr)` -- convert to float or null
- [ ] `toBoolean(expr)` -- convert to boolean
- [ ] `toBooleanOrNull(expr)` -- convert to boolean or null
- [ ] `toString(expr)` -- convert to string
- [ ] `toStringOrNull(expr)` -- convert to string or null
- [ ] `toBooleanList(list)` -- convert list elements to boolean
- [ ] `toFloatList(list)` -- convert list elements to float
- [ ] `toIntegerList(list)` -- convert list elements to integer
- [ ] `toStringList(list)` -- convert list elements to string

### 7.4 Aggregating Functions
- [ ] `count(expr)` -- count values
- [ ] `count(*)` -- count rows
- [ ] `count(DISTINCT expr)` -- count distinct values
- [ ] `sum(expr)` -- sum of numeric values
- [ ] `avg(expr)` -- average of numeric values
- [ ] `min(expr)` -- minimum value
- [ ] `max(expr)` -- maximum value
- [ ] `collect(expr)` -- collect values into list
- [ ] `collect(DISTINCT expr)` -- collect distinct values
- [ ] `stDev(expr)` -- sample standard deviation
- [ ] `stDevP(expr)` -- population standard deviation
- [ ] `percentileCont(expr, percentile)` -- percentile (continuous/interpolated)
- [x] `percentileDisc(expr, percentile)` -- percentile (discrete/nearest)

### 7.5 List Functions
- [ ] `range(start, end [, step])` -- generate integer list
- [ ] `reverse(list)` -- reverse list
- [ ] `tail(list)` -- all but first element
- [ ] `head(list)` -- first element
- [ ] `last(list)` -- last element
- [ ] `size(list)` -- number of elements
- [ ] `reduce(acc = initial, x IN list | expression)` -- fold/reduce over list
- [ ] `nodes(path)` -- list of nodes in path
- [ ] `relationships(path)` -- list of relationships in path
- [ ] `keys(map_or_node_or_rel)` -- list of keys
- [ ] `labels(node)` -- list of node labels

### 7.6 Mathematical Functions -- Numeric
- [ ] `abs(expr)` -- absolute value
- [ ] `ceil(expr)` -- ceiling
- [ ] `floor(expr)` -- floor
- [ ] `round(expr)` -- round to nearest integer
- [ ] `round(expr, precision)` -- round to decimal places
- [ ] `round(expr, precision, mode)` -- round with rounding mode
- [ ] `sign(expr)` -- signum (-1, 0, 1)
- [ ] `rand()` -- random float [0, 1)
- [ ] `isNaN(expr)` -- check for NaN

### 7.7 Mathematical Functions -- Logarithmic
- [ ] `e()` -- Euler's number
- [ ] `exp(expr)` -- e^x
- [ ] `log(expr)` -- natural logarithm
- [ ] `log10(expr)` -- base-10 logarithm
- [ ] `sqrt(expr)` -- square root

### 7.8 Mathematical Functions -- Trigonometric
- [ ] `pi()` -- pi constant
- [ ] `sin(expr)` -- sine
- [ ] `cos(expr)` -- cosine
- [ ] `tan(expr)` -- tangent
- [ ] `asin(expr)` -- arcsine
- [ ] `acos(expr)` -- arccosine
- [ ] `atan(expr)` -- arctangent
- [ ] `atan2(y, x)` -- arctangent of y/x
- [ ] `cot(expr)` -- cotangent
- [ ] `sin(expr)` -- sine
- [ ] `sinh(expr)` -- hyperbolic sine
- [ ] `cosh(expr)` -- hyperbolic cosine
- [ ] `tanh(expr)` -- hyperbolic tangent
- [ ] `coth(expr)` -- hyperbolic cotangent
- [ ] `degrees(expr)` -- radians to degrees
- [ ] `radians(expr)` -- degrees to radians
- [ ] `haversin(expr)` -- haversine

### 7.9 String Functions
- [ ] `toString(expr)` -- convert to string
- [ ] `toUpper(string)` / `upper(string)` -- uppercase
- [ ] `toLower(string)` / `lower(string)` -- lowercase
- [ ] `trim(string)` -- remove leading/trailing whitespace
- [ ] `ltrim(string)` -- remove leading whitespace
- [ ] `rtrim(string)` -- remove trailing whitespace
- [ ] `btrim(string)` -- remove leading and trailing whitespace
- [ ] `replace(string, search, replacement)` -- replace occurrences
- [ ] `substring(string, start [, length])` -- substring
- [ ] `left(string, length)` -- leftmost characters
- [ ] `right(string, length)` -- rightmost characters
- [ ] `split(string, delimiter)` -- split into list
- [ ] `reverse(string)` -- reverse string
- [ ] `size(string)` -- string length
- [ ] `normalize(string)` -- normalize Unicode string

### 7.10 Temporal Functions
- [ ] `date()` -- current date
- [ ] `date({year, month, day})` -- construct date
- [ ] `date(string)` -- parse date from string
- [ ] `date.truncate(unit, temporal)` -- truncate to date
- [ ] `date.transaction()` / `date.statement()` / `date.realtime()` -- clock-based
- [ ] `localtime()` -- current local time
- [ ] `localtime({hour, minute, ...})` -- construct local time
- [ ] `localtime.truncate(unit, temporal)` -- truncate
- [ ] `time()` -- current zoned time
- [ ] `time({hour, minute, ..., timezone})` -- construct zoned time
- [ ] `time.truncate(unit, temporal)` -- truncate
- [ ] `localdatetime()` -- current local datetime
- [ ] `localdatetime({year, month, ...})` -- construct
- [ ] `localdatetime.truncate(unit, temporal)` -- truncate
- [ ] `datetime()` -- current zoned datetime
- [ ] `datetime({year, month, ..., timezone})` -- construct
- [ ] `datetime.fromEpoch(seconds, nanoseconds)` -- from epoch
- [ ] `datetime.fromEpochMillis(milliseconds)` -- from epoch millis
- [ ] `datetime.truncate(unit, temporal)` -- truncate
- [ ] `duration({years, months, weeks, days, hours, minutes, seconds})` -- construct duration
- [ ] `duration(string)` -- parse ISO 8601 duration
- [ ] `duration.between(temporal1, temporal2)` -- duration between instants
- [ ] `duration.inMonths(temporal1, temporal2)` -- duration in months
- [ ] `duration.inDays(temporal1, temporal2)` -- duration in days
- [ ] `duration.inSeconds(temporal1, temporal2)` -- duration in seconds
- [ ] `format(temporal, formatString)` -- format temporal as string (Cypher 25)

### 7.11 Spatial Functions
- [ ] `point({x, y})` -- 2D Cartesian point
- [ ] `point({x, y, z})` -- 3D Cartesian point
- [ ] `point({latitude, longitude})` -- 2D geographic point (WGS-84)
- [ ] `point({latitude, longitude, height})` -- 3D geographic point
- [ ] `point.distance(point1, point2)` -- distance between points
- [ ] `point.withinBBox(point, lowerLeft, upperRight)` -- bounding box check

### 7.12 Path Functions
- [ ] `length(path)` -- number of relationships in path
- [ ] `nodes(path)` -- list of nodes in path
- [ ] `relationships(path)` -- list of relationships in path

### 7.13 LOAD CSV Functions
- [ ] `file()` -- current file path being loaded
- [ ] `linenumber()` -- current line number

### 7.14 User-Defined Functions
- [ ] User-defined scalar functions
- [ ] User-defined aggregation functions

### 7.15 Vector Functions (Neo4j 2025+)
- [ ] `vector()` -- construct vector value
- [ ] `vector.similarity.cosine()` -- cosine similarity
- [ ] `vector.similarity.euclidean()` -- euclidean similarity
- [ ] `vector_dimension_count()` -- vector dimensions
- [ ] `vector_distance()` -- distance between vectors
- [ ] `vector_norm()` -- vector norm

### 7.16 Database/Graph Functions (Neo4j-specific)
- [ ] `db.nameFromElementId()` -- database name from element ID
- [ ] `graph.names()` -- list of graph names
- [ ] `graph.propertiesByName()` -- graph properties
- [ ] `graph.byName()` -- resolve graph by name
- [ ] `graph.byElementId()` -- resolve graph by element ID

## 8. Data Types

### 8.1 Property Types (storable as properties)
- [ ] `BOOLEAN` (synonym: `BOOL`)
- [ ] `INTEGER` (synonyms: `INT`, `SIGNED INTEGER`)
- [ ] `FLOAT` (synonym: `FLOAT64`)
- [ ] `STRING` (synonym: `VARCHAR`)
- [ ] `DATE`
- [ ] `LOCAL TIME`
- [ ] `ZONED TIME`
- [ ] `LOCAL DATETIME`
- [ ] `ZONED DATETIME`
- [ ] `DURATION`
- [ ] `POINT`
- [ ] `LIST<type>` (homogeneous lists of property types)
- [ ] `VECTOR`

### 8.2 Structural Types (not storable as properties)
- [ ] `NODE`
- [ ] `RELATIONSHIP`
- [ ] `PATH`

### 8.3 Constructed Types
- [ ] `LIST<ANY>` (heterogeneous lists)
- [ ] `MAP`

### 8.4 Special Types
- [ ] `NULL`
- [ ] `ANY`
- [ ] `NOTHING`
- [ ] `PROPERTY VALUE`
- [ ] Closed dynamic unions: `TYPE1 | TYPE2`

## 9. Schema -- Indexes

### 9.1 Index Types
- [ ] `CREATE INDEX` -- range index (default)
- [ ] `CREATE RANGE INDEX` -- explicit range index
- [ ] `CREATE TEXT INDEX` -- text index (for CONTAINS, ENDS WITH)
- [ ] `CREATE POINT INDEX` -- spatial point index
- [ ] `CREATE LOOKUP INDEX` -- token lookup index (labels/types)
- [ ] `CREATE FULLTEXT INDEX` -- full-text search index
- [ ] `CREATE VECTOR INDEX` -- vector similarity index

### 9.2 Index Management
- [ ] `CREATE INDEX ... IF NOT EXISTS`
- [ ] `CREATE INDEX ... FOR (n:Label) ON (n.property)`
- [ ] `CREATE INDEX ... FOR ()-[r:TYPE]-() ON (r.property)`
- [ ] `DROP INDEX name`
- [ ] `DROP INDEX name IF EXISTS`
- [ ] `SHOW INDEXES`

## 10. Schema -- Constraints

### 10.1 Constraint Types
- [ ] Node property uniqueness: `REQUIRE n.prop IS UNIQUE`
- [ ] Node property existence: `REQUIRE n.prop IS NOT NULL`
- [ ] Node property type: `REQUIRE n.prop IS :: TYPE`
- [ ] Node key: `REQUIRE n.prop IS NODE KEY` (unique + exists)
- [ ] Relationship property uniqueness: `REQUIRE r.prop IS UNIQUE`
- [ ] Relationship property existence: `REQUIRE r.prop IS NOT NULL`
- [ ] Relationship property type: `REQUIRE r.prop IS :: TYPE`
- [ ] Relationship key: `REQUIRE r.prop IS RELATIONSHIP KEY`
- [ ] Composite constraints (multi-property versions of above)

### 10.2 Constraint Management
- [ ] `CREATE CONSTRAINT name FOR (n:Label) REQUIRE ...`
- [ ] `CREATE CONSTRAINT name FOR ()-[r:TYPE]-() REQUIRE ...`
- [ ] `CREATE CONSTRAINT ... IF NOT EXISTS`
- [ ] `DROP CONSTRAINT name`
- [ ] `DROP CONSTRAINT name IF EXISTS`
- [ ] `SHOW CONSTRAINTS`

## 11. Procedures

- [ ] `CALL db.procedure() YIELD column`
- [ ] `CALL db.procedure() YIELD column AS alias`
- [ ] `CALL db.procedure() YIELD column WHERE predicate`
- [ ] Void procedures (no YIELD)
- [ ] In-query procedure calls with CALL { }
- [ ] `SHOW PROCEDURES`
- [ ] `SHOW FUNCTIONS`

## 12. Administrative Commands (Neo4j-specific)

### 12.1 Database Management
- [ ] `CREATE DATABASE`
- [ ] `DROP DATABASE`
- [ ] `START DATABASE`
- [ ] `STOP DATABASE`
- [ ] `SHOW DATABASES`
- [ ] `ALTER DATABASE`

### 12.2 User/Role Management
- [ ] `CREATE USER`
- [ ] `ALTER USER`
- [ ] `DROP USER`
- [ ] `SHOW USERS`
- [ ] `CREATE ROLE`
- [ ] `DROP ROLE`
- [ ] `GRANT`
- [ ] `DENY`
- [ ] `REVOKE`

### 12.3 Transaction Management
- [ ] `SHOW TRANSACTIONS`
- [ ] `TERMINATE TRANSACTIONS`

### 12.4 Settings
- [ ] `SHOW SETTINGS`

### 12.5 Graph Type Management (Cypher 25)
- [ ] `SHOW CURRENT GRAPH TYPE`
- [ ] `ALTER CURRENT GRAPH TYPE`

## 13. Query Modifiers and Special Syntax

- [ ] Query-level `CYPHER 25` / `CYPHER 5` version selector
- [ ] `EXPLAIN` -- show query plan without executing
- [ ] `PROFILE` -- execute and show actual plan with row counts
- [ ] Comments: `//` (line) and `/* */` (block)
- [ ] Backtick-quoted identifiers: `` `my variable` ``
- [ ] Escaped Unicode in strings: `\uXXXX`
- [ ] `AS` aliasing in RETURN, WITH, YIELD
- [ ] `*` wildcard in RETURN/WITH

---

## Sources

- [openCypher Specification](https://opencypher.org/)
- [openCypher GitHub - Standardisation Scope](https://github.com/opencypher/openCypher/blob/master/docs/standardisation-scope.adoc)
- [Cypher Query Language Reference, Version 9 (PDF)](https://s3.amazonaws.com/artifacts.opencypher.org/openCypher9.pdf)
- [Neo4j Cypher Manual - Functions](https://neo4j.com/docs/cypher-manual/current/functions/)
- [Neo4j Cypher Manual - Clauses](https://neo4j.com/docs/cypher-manual/current/clauses/)
- [Neo4j Cypher Manual - Expressions](https://neo4j.com/docs/cypher-manual/current/expressions/)
- [Neo4j Cypher Manual - Values and Types](https://neo4j.com/docs/cypher-manual/current/values-and-types/)
- [Neo4j Cypher Manual - Constraints](https://neo4j.com/docs/cypher-manual/current/schema/constraints/create-constraints/)
- [Neo4j Cypher Manual - Indexes](https://neo4j.com/docs/cypher-manual/current/indexes/search-performance-indexes/managing-indexes/)
- [Neo4j Cypher Cheat Sheet](https://neo4j.com/docs/cypher-cheat-sheet/25/all/)
