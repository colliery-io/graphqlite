# Cypher Functions

## String Functions

| Function | Description | Example |
|----------|-------------|---------|
| `toLower(s)` | Convert to lowercase | `toLower('Hello')` → `'hello'` |
| `toUpper(s)` | Convert to uppercase | `toUpper('Hello')` → `'HELLO'` |
| `trim(s)` | Remove leading/trailing whitespace | `trim('  hi  ')` → `'hi'` |
| `ltrim(s)` | Remove leading whitespace | `ltrim('  hi')` → `'hi'` |
| `rtrim(s)` | Remove trailing whitespace | `rtrim('hi  ')` → `'hi'` |
| `replace(s, from, to)` | Replace occurrences | `replace('hello', 'l', 'x')` → `'hexxo'` |
| `substring(s, start, len)` | Extract substring | `substring('hello', 1, 3)` → `'ell'` |
| `left(s, n)` | First n characters | `left('hello', 2)` → `'he'` |
| `right(s, n)` | Last n characters | `right('hello', 2)` → `'lo'` |
| `split(s, delim)` | Split into list | `split('a,b,c', ',')` → `['a','b','c']` |
| `reverse(s)` | Reverse string | `reverse('hello')` → `'olleh'` |
| `length(s)` | String length | `length('hello')` → `5` |
| `size(s)` | String length (alias) | `size('hello')` → `5` |
| `toString(x)` | Convert to string | `toString(123)` → `'123'` |

## String Predicates

| Function | Description | Example |
|----------|-------------|---------|
| `startsWith(s, prefix)` | Check prefix | `startsWith('hello', 'he')` → `true` |
| `endsWith(s, suffix)` | Check suffix | `endsWith('hello', 'lo')` → `true` |
| `contains(s, sub)` | Check substring | `contains('hello', 'ell')` → `true` |

## Math Functions

| Function | Description | Example |
|----------|-------------|---------|
| `abs(n)` | Absolute value | `abs(-5)` → `5` |
| `ceil(n)` | Round up | `ceil(2.3)` → `3` |
| `floor(n)` | Round down | `floor(2.7)` → `2` |
| `round(n)` | Round to nearest | `round(2.5)` → `3` |
| `sign(n)` | Sign (-1, 0, 1) | `sign(-5)` → `-1` |
| `sqrt(n)` | Square root | `sqrt(16)` → `4` |
| `log(n)` | Natural logarithm | `log(e())` → `1` |
| `log10(n)` | Base-10 logarithm | `log10(100)` → `2` |
| `exp(n)` | e^n | `exp(1)` → `2.718...` |
| `rand()` | Random 0-1 | `rand()` → `0.42...` |
| `random()` | Random 0-1 (alias) | `random()` → `0.42...` |
| `pi()` | π constant | `pi()` → `3.14159...` |
| `e()` | e constant | `e()` → `2.71828...` |

## Trigonometric Functions

| Function | Description |
|----------|-------------|
| `sin(n)` | Sine |
| `cos(n)` | Cosine |
| `tan(n)` | Tangent |
| `asin(n)` | Arc sine |
| `acos(n)` | Arc cosine |
| `atan(n)` | Arc tangent |

## List Functions

| Function | Description | Example |
|----------|-------------|---------|
| `head(list)` | First element | `head([1,2,3])` → `1` |
| `tail(list)` | All but first | `tail([1,2,3])` → `[2,3]` |
| `last(list)` | Last element | `last([1,2,3])` → `3` |
| `size(list)` | Length | `size([1,2,3])` → `3` |
| `range(start, end)` | Create range | `range(1, 5)` → `[1,2,3,4,5]` |
| `reverse(list)` | Reverse list | `reverse([1,2,3])` → `[3,2,1]` |
| `keys(map)` | Get map keys | `keys({a:1, b:2})` → `['a','b']` |

## Aggregate Functions

| Function | Description | Example |
|----------|-------------|---------|
| `count(x)` | Count items | `count(n)`, `count(*)` |
| `sum(x)` | Sum values | `sum(n.amount)` |
| `avg(x)` | Average | `avg(n.score)` |
| `min(x)` | Minimum | `min(n.age)` |
| `max(x)` | Maximum | `max(n.age)` |
| `collect(x)` | Collect into list | `collect(n.name)` |

## Entity Functions

| Function | Description | Example |
|----------|-------------|---------|
| `id(node)` | Get node/edge ID | `id(n)` |
| `labels(node)` | Get node labels | `labels(n)` → `['Person']` |
| `type(rel)` | Get relationship type | `type(r)` → `'KNOWS'` |
| `properties(x)` | Get all properties | `properties(n)` |
| `startNode(rel)` | Start node of relationship | `startNode(r)` |
| `endNode(rel)` | End node of relationship | `endNode(r)` |

## Path Functions

| Function | Description | Example |
|----------|-------------|---------|
| `nodes(path)` | Get all nodes in path | `nodes(p)` |
| `relationships(path)` | Get all relationships | `relationships(p)` |
| `rels(path)` | Get all relationships (alias) | `rels(p)` |
| `length(path)` | Path length (edges) | `length(p)` |

## Type Conversion

| Function | Description | Example |
|----------|-------------|---------|
| `toInteger(x)` | Convert to integer | `toInteger('42')` → `42` |
| `toFloat(x)` | Convert to float | `toFloat('3.14')` → `3.14` |
| `toBoolean(x)` | Convert to boolean | `toBoolean('true')` → `true` |
| `coalesce(x, y, ...)` | First non-null value | `coalesce(n.name, 'Unknown')` |

## Temporal Functions

| Function | Description | Example |
|----------|-------------|---------|
| `date()` | Current date | `date()` → `'2025-01-15'` |
| `datetime()` | Current datetime | `datetime()` |
| `time()` | Current time | `time()` |
| `timestamp()` | Unix timestamp (ms) | `timestamp()` |
| `localdatetime()` | Local datetime | `localdatetime()` |
| `randomUUID()` | Generate random UUID | `randomUUID()` → `'550e8400-e29b-...'` |

## Predicate Functions

| Function | Description | Example |
|----------|-------------|---------|
| `exists(pattern)` | Pattern exists | `EXISTS { (n)-[:KNOWS]->() }` |
| `exists(prop)` | Property exists | `exists(n.email)` |
| `all(x IN list WHERE pred)` | All match | `all(x IN [1,2,3] WHERE x > 0)` |
| `any(x IN list WHERE pred)` | Any match | `any(x IN [1,2,3] WHERE x > 2)` |
| `none(x IN list WHERE pred)` | None match | `none(x IN [1,2,3] WHERE x < 0)` |
| `single(x IN list WHERE pred)` | Exactly one | `single(x IN [1,2,3] WHERE x = 2)` |

## Reduce

| Function | Description | Example |
|----------|-------------|---------|
| `reduce(acc = init, x IN list \| expr)` | Fold/reduce | `reduce(s = 0, x IN [1,2,3] \| s + x)` → `6` |

## CASE Expressions

### Searched CASE

Evaluates conditions in order and returns the first matching result:

```cypher
RETURN CASE
    WHEN n.age < 18 THEN 'minor'
    WHEN n.age < 65 THEN 'adult'
    ELSE 'senior'
END AS category
```

### Simple CASE

Compares an expression against values:

```cypher
RETURN CASE n.status
    WHEN 'A' THEN 'Active'
    WHEN 'I' THEN 'Inactive'
    WHEN 'P' THEN 'Pending'
    ELSE 'Unknown'
END AS status_name
```

## Comprehensions

### List Comprehension

Create lists by transforming or filtering:

```cypher
// Transform each element
RETURN [x IN range(1, 5) | x * 2]
// → [2, 4, 6, 8, 10]

// Filter elements
RETURN [x IN range(1, 10) WHERE x % 2 = 0]
// → [2, 4, 6, 8, 10]

// Filter and transform
RETURN [x IN range(1, 10) WHERE x % 2 = 0 | x * x]
// → [4, 16, 36, 64, 100]
```

### Pattern Comprehension

Extract data from pattern matches within an expression:

```cypher
// Collect names of friends
MATCH (p:Person)
RETURN p.name, [(p)-[:KNOWS]->(friend) | friend.name] AS friends

// With filtering
RETURN [(p)-[:KNOWS]->(f:Person) WHERE f.age > 21 | f.name] AS adult_friends
```

### Map Projection

Create maps by selecting properties from nodes:

```cypher
// Select specific properties
MATCH (n:Person)
RETURN n {.name, .age}
// → {name: "Alice", age: 30}

// Include computed values
MATCH (n:Person)
RETURN n {.name, status: 'active', upperName: toUpper(n.name)}
```
