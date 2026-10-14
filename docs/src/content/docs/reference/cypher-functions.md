---
title: Cypher Functions
---

Every function available in GraphQLite Cypher queries, organized by category.

---

## String Functions

| Signature | Returns | Description |
|-----------|---------|-------------|
| `toUpper(s)` | String | Convert to uppercase |
| `toLower(s)` | String | Convert to lowercase |
| `trim(s)` | String | Remove leading and trailing whitespace |
| `ltrim(s)` | String | Remove leading whitespace |
| `rtrim(s)` | String | Remove trailing whitespace |
| `btrim(s)` | String | Remove leading and trailing whitespace (alias of `trim`) |
| `substring(s, start)` | String | Substring from `start` (0-based) to end |
| `substring(s, start, len)` | String | Substring of length `len` from `start` |
| `replace(s, search, replacement)` | String | Replace all occurrences of `search` with `replacement` |
| `reverse(s)` | String | Reverse characters |
| `left(s, n)` | String | First `n` characters |
| `right(s, n)` | String | Last `n` characters |
| `split(s, delimiter)` | List\<String\> | Split string into list of substrings |
| `toString(val)` | String | Convert any value to its string representation |
| `size(s)` | Integer | Number of characters in string |
| `isEmpty(s)` | Boolean | `true` if string has length zero or is `null` |
| `char_length(s)` | Integer | Number of characters (alias of `size`) |
| `character_length(s)` | Integer | Number of characters (alias of `size`) |

**Examples**

```cypher
RETURN toUpper('hello')           -- 'HELLO'
RETURN substring('abcdef', 2, 3)  -- 'cde'
RETURN split('a,b,c', ',')        -- ['a', 'b', 'c']
RETURN left('abcdef', 3)          -- 'abc'
```

---

## Math Functions

| Signature | Returns | Description |
|-----------|---------|-------------|
| `abs(n)` | Number | Absolute value |
| `ceil(n)` | Integer | Ceiling (smallest integer >= n) |
| `floor(n)` | Integer | Floor (largest integer &lt;= n) |
| `round(n)` | Integer | Round to nearest integer |
| `round(n, precision)` | Float | Round to `precision` decimal places |
| `sqrt(n)` | Float | Square root |
| `sign(n)` | Integer | -1, 0, or 1 |
| `log(n)` | Float | Natural logarithm |
| `log10(n)` | Float | Base-10 logarithm |
| `exp(n)` | Float | e raised to the power n |
| `e()` | Float | Euler's number (2.718…) |
| `pi()` | Float | Pi (3.141…) |
| `rand()` | Float | Random float in [0, 1) |
| `toInteger(val)` | Integer | Convert to integer; `null` on failure |
| `toFloat(val)` | Float | Convert to float; `null` on failure |

**Examples**

```cypher
RETURN abs(-5)          -- 5
RETURN round(3.567, 2)  -- 3.57
RETURN sqrt(16)         -- 4.0
RETURN rand()           -- e.g. 0.7341...
```

---

## Trigonometric Functions

| Signature | Returns | Description |
|-----------|---------|-------------|
| `sin(n)` | Float | Sine (radians) |
| `cos(n)` | Float | Cosine (radians) |
| `tan(n)` | Float | Tangent (radians) |
| `asin(n)` | Float | Arcsine; result in radians |
| `acos(n)` | Float | Arccosine; result in radians |
| `atan(n)` | Float | Arctangent; result in radians |
| `atan2(y, x)` | Float | Two-argument arctangent |
| `degrees(n)` | Float | Radians to degrees |
| `radians(n)` | Float | Degrees to radians |
| `cot(n)` | Float | Cotangent |
| `haversin(n)` | Float | Half the versine of n |
| `sinh(n)` | Float | Hyperbolic sine |
| `cosh(n)` | Float | Hyperbolic cosine |
| `tanh(n)` | Float | Hyperbolic tangent |
| `coth(n)` | Float | Hyperbolic cotangent |
| `isNaN(n)` | Boolean | `true` if n is NaN |

**Examples**

```cypher
RETURN degrees(pi())   -- 180.0
RETURN atan2(1.0, 1.0) -- 0.7853...
```

---

## List Functions

| Signature | Returns | Description |
|-----------|---------|-------------|
| `size(list)` | Integer | Number of elements |
| `head(list)` | Any | First element; `null` if empty |
| `tail(list)` | List | All elements except the first; empty list if input has 0 or 1 elements |
| `last(list)` | Any | Last element; `null` if empty |
| `range(start, end)` | List\<Integer\> | Inclusive integer range with step 1 |
| `range(start, end, step)` | List\<Integer\> | Inclusive integer range with given step |
| `collect(expr)` | List | Aggregate: collect non-null values into a list |
| `keys(node_or_map)` | List\<String\> | Property key names of a node, relationship, or map |
| `reduce(acc = init, x IN list \| expr)` | Any | Fold list into single value |
| `[expr FOR x IN list]` | List | List comprehension without filter |
| `[expr FOR x IN list WHERE cond]` | List | List comprehension with filter |

**Examples**

```cypher
RETURN range(1, 5)                          -- [1, 2, 3, 4, 5]
RETURN range(0, 10, 2)                      -- [0, 2, 4, 6, 8, 10]
RETURN head([1, 2, 3])                      -- 1
RETURN tail([1, 2, 3])                      -- [2, 3]
RETURN reduce(total = 0, x IN [1,2,3] | total + x) -- 6
RETURN [x * 2 FOR x IN [1,2,3] WHERE x > 1]        -- [4, 6]
```

---

## Aggregation Functions

Aggregation functions collapse multiple rows into one. They are valid in `RETURN` and `WITH`.

| Signature | Returns | Description |
|-----------|---------|-------------|
| `count(expr)` | Integer | Count of non-null values |
| `count(*)` | Integer | Count of rows |
| `sum(expr)` | Number | Sum of numeric values |
| `avg(expr)` | Float | Arithmetic mean of numeric values |
| `min(expr)` | Any | Minimum value |
| `max(expr)` | Any | Maximum value |
| `collect(expr)` | List | List of non-null values |
| `stdev(expr)` | Float | Sample standard deviation |
| `stdevp(expr)` | Float | Population standard deviation |

**Examples**

```cypher
MATCH (n:Person) RETURN count(n), avg(n.age), collect(n.name)
MATCH (n:Person) RETURN count(*) AS total
```

---

## Entity Functions

| Signature | Returns | Description |
|-----------|---------|-------------|
| `id(entity)` | Integer | Internal numeric ID of a node or relationship |
| `elementId(entity)` | String | String form of internal ID |
| `labels(node)` | List\<String\> | All labels of a node |
| `type(rel)` | String | Relationship type name |
| `properties(entity)` | Map | All properties as a map |
| `startNode(rel)` | Node | Source node of a relationship |
| `endNode(rel)` | Node | Target node of a relationship |
| `nodes(path)` | List\<Node\> | Ordered list of nodes in a path |
| `relationships(path)` | List\<Relationship\> | Ordered list of relationships in a path |
| `length(path)` | Integer | Number of relationships in a path |

**Examples**

```cypher
MATCH (n:Person) RETURN id(n), labels(n)
MATCH ()-[r]->() RETURN type(r)
MATCH p = (a)-[*]->(b) RETURN length(p), nodes(p)
```

---

## Type Conversion Functions

| Signature | Returns | Description |
|-----------|---------|-------------|
| `toString(val)` | String | Convert to string; error on unconvertible types |
| `toInteger(val)` | Integer | Convert to integer; error on unconvertible types |
| `toFloat(val)` | Float | Convert to float; error on unconvertible types |
| `toBoolean(val)` | Boolean | Convert to boolean; error on unconvertible types |
| `toStringOrNull(val)` | String \| null | Convert to string; `null` on failure |
| `toIntegerOrNull(val)` | Integer \| null | Convert to integer; `null` on failure |
| `toFloatOrNull(val)` | Float \| null | Convert to float; `null` on failure |
| `toBooleanOrNull(val)` | Boolean \| null | Convert to boolean; `null` on failure |
| `valueType(val)` | String | Returns a string naming the Cypher type: `"INTEGER"`, `"FLOAT"`, `"STRING"`, `"BOOLEAN"`, `"NULL"`, `"LIST"`, `"MAP"`, `"NODE"`, `"RELATIONSHIP"`, `"PATH"` |

**Examples**

```cypher
RETURN toInteger('42')         -- 42
RETURN toFloatOrNull('abc')    -- null
RETURN valueType(3.14)         -- 'FLOAT'
RETURN toBoolean('true')       -- true
```

---

## Temporal Functions

| Signature | Returns | Description |
|-----------|---------|-------------|
| `date({year, month, day})` | Date | Construct a date |
| `time({hour, minute, second})` | Time | Construct a time |
| `datetime({year, month, day, hour, minute, second})` | DateTime | Construct a datetime |
| `localdatetime({year, month, day, hour, minute, second})` | LocalDateTime | Construct a local datetime (no timezone) |
| `duration({days, hours, minutes, seconds})` | Duration | Construct a duration; all fields optional |
| `datetime.fromepoch(seconds)` | DateTime | DateTime from Unix epoch seconds |
| `datetime.fromepochmillis(ms)` | DateTime | DateTime from Unix epoch milliseconds |
| `duration.inDays(d1, d2)` | Duration | Duration between two dates in days |
| `duration.inSeconds(d1, d2)` | Duration | Duration between two datetimes in seconds |
| `date.truncate(unit, date)` | Date | Truncate date to `unit`: `'year'`, `'month'`, `'week'`, `'day'` |

**Examples**

```cypher
RETURN date({year: 2024, month: 3, day: 15})
RETURN datetime.fromepoch(1700000000)
RETURN duration({days: 7, hours: 12})
RETURN date.truncate('month', date({year: 2024, month: 3, day: 15}))
```

---

## Spatial Functions

| Signature | Returns | Description |
|-----------|---------|-------------|
| `point({x, y})` | Point | 2D Cartesian point |
| `point({x, y, z})` | Point | 3D Cartesian point |
| `point({latitude, longitude})` | Point | 2D geographic point (WGS-84) |
| `point({latitude, longitude, height})` | Point | 3D geographic point (WGS-84) |
| `distance(p1, p2)` | Float | Distance between two points (meters for geographic, units for Cartesian) |
| `point.withinBBox(point, lowerLeft, upperRight)` | Boolean | `true` if point is inside bounding box |

**Examples**

```cypher
RETURN point({x: 1.0, y: 2.0})
RETURN distance(point({latitude: 48.8, longitude: 2.3}), point({latitude: 51.5, longitude: -0.1}))
RETURN point.withinBBox(
  point({x: 5, y: 5}),
  point({x: 0, y: 0}),
  point({x: 10, y: 10})
)  -- true
```

---

## Predicate Functions

| Signature | Returns | Description |
|-----------|---------|-------------|
| `exists(expr)` | Boolean | `true` if the property or pattern exists and is not `null` |
| `exists{pattern}` | Boolean | `true` if the pattern matches at least one result (full pattern syntax) |
| `coalesce(v1, v2, ...)` | Any | First non-null argument; `null` if all arguments are `null` |
| `nullIf(v1, v2)` | Any \| null | Returns `null` if `v1 = v2`; otherwise returns `v1` |

**Examples**

```cypher
MATCH (n:Person) WHERE exists(n.email) RETURN n.name
MATCH (n:Person) WHERE exists{(n)-[:KNOWS]->(:Person)} RETURN n.name
RETURN coalesce(null, null, 'default')   -- 'default'
RETURN nullIf(5, 5)                      -- null
RETURN nullIf(5, 6)                      -- 5
```

---

## CASE Expressions

**Simple form**

```cypher
CASE expr
  WHEN value1 THEN result1
  WHEN value2 THEN result2
  ELSE default
END
```

**Generic form**

```cypher
CASE
  WHEN condition1 THEN result1
  WHEN condition2 THEN result2
  ELSE default
END
```

`ELSE` is optional; omitting it returns `null` for unmatched rows.

**Examples**

```cypher
MATCH (n:Person)
RETURN CASE n.role
  WHEN 'admin' THEN 'Administrator'
  WHEN 'mod'   THEN 'Moderator'
  ELSE 'User'
END AS roleLabel
```

```cypher
MATCH (n:Person)
RETURN CASE
  WHEN n.age < 18 THEN 'minor'
  WHEN n.age < 65 THEN 'adult'
  ELSE 'senior'
END AS ageGroup
```

---

## Graph Algorithm Functions

Called inside Cypher queries using `CALL` syntax or inline. See [Graph Algorithms](../algorithms/) for full parameter and return type documentation.

| Signature | Description |
|-----------|-------------|
| `pageRank([damping, iterations])` | PageRank centrality |
| `labelPropagation([iterations])` | Label propagation community detection |
| `louvain([resolution])` | Louvain modularity community detection |
| `dijkstra(source, target[, weight_property])` | Weighted shortest path |
| `yen(source, target, k[, max_hops][, weight_property])` | k shortest loopless paths, optionally hop-capped (alias `kShortestPaths`) |
| `astar(source, target[, lat_prop, lon_prop])` | A* heuristic shortest path |
| `degreeCentrality()` | In/out/total degree per node |
| `betweennessCentrality()` | Betweenness centrality per node |
| `closenessCentrality()` | Closeness centrality per node |
| `eigenvectorCentrality([iterations])` | Eigenvector centrality per node |
| `weaklyConnectedComponents()` | WCC component assignment |
| `stronglyConnectedComponents()` | SCC component assignment |
| `bfs(start[, max_depth])` | Breadth-first traversal |
| `dfs(start[, max_depth])` | Depth-first traversal |
| `nodeSimilarity([node1, node2, threshold, top_k])` | Jaccard similarity |
| `knn(node, k)` | k-nearest neighbors by similarity |
| `triangleCount()` | Triangle count and clustering coefficient per node |
| `apsp()` | All-pairs shortest paths |
| `shortestPath(pattern)` | Shortest path as a path value |
