---
title: Cypher Operators
slug: 0.9.4/reference/cypher-operators
---

## Precedence

Operators are listed from highest to lowest precedence. Operators at the same level associate left-to-right unless noted.

| Level | Operator(s) | Associativity |
|-------|-------------|---------------|
| 1 (highest) | `.` (property access), `[` (subscript/slice) | Left |
| 2 | Unary `-` (negation), `NOT` | Right |
| 3 | `*`, `/`, `%` | Left |
| 4 | `+`, `-` | Left |
| 5 | `=`, `<>`, `<`, `>`, `<=`, `>=` | Left |
| 6 | `IS NULL`, `IS NOT NULL` | — |
| 7 | `STARTS WITH`, `ENDS WITH`, `CONTAINS`, `=~`, `IN` | Left |
| 8 | `AND` | Left |
| 9 | `XOR` | Left |
| 10 (lowest) | `OR` | Left |

Use parentheses to override precedence.

***

## Arithmetic Operators

| Operator | Syntax | Description | Example | Result |
|----------|--------|-------------|---------|--------|
| Addition | `a + b` | Numeric addition; string concatenation | `2 + 3` | `5` |
| Subtraction | `a - b` | Numeric subtraction | `10 - 4` | `6` |
| Multiplication | `a * b` | Numeric multiplication | `3 * 4` | `12` |
| Division | `a / b` | Numeric division; integer inputs yield float | `7 / 2` | `3.5` |
| Modulo | `a % b` | Remainder | `10 % 3` | `1` |
| Unary negation | `-a` | Negate a number | `-n.age` | negated |

**Type behavior**

* `Integer + Integer` → Integer
* `Integer + Float` → Float
* `String + String` → String concatenation
* Any arithmetic with `null` → `null`

***

## Comparison Operators

| Operator | Syntax | Description |
|----------|--------|-------------|
| Equals | `a = b` | Value equality |
| Not equals | `a <> b` | Value inequality |
| Less than | `a < b` | |
| Greater than | `a > b` | |
| Less or equal | `a <= b` | |
| Greater or equal | `a >= b` | |

**Type behavior**

* Comparing `null` with any operator returns `null` (not `true` or `false`).
* Comparisons between incompatible types return `null`.
* Strings compare lexicographically.

**Examples**

```cypher
WHERE n.age >= 18
WHERE n.name <> 'Alice'
WHERE n.score = 100
```

***

## Boolean Operators

| Operator | Syntax | Description |
|----------|--------|-------------|
| AND | `a AND b` | `true` if both operands are `true` |
| OR | `a OR b` | `true` if at least one operand is `true` |
| NOT | `NOT a` | Logical negation |
| XOR | `a XOR b` | `true` if exactly one operand is `true` |

**Three-valued logic (null behavior)**

| a | b | a AND b | a OR b |
|---|---|---------|--------|
| true | null | null | true |
| false | null | false | null |
| null | null | null | null |

`NOT null` → `null`

**Examples**

```cypher
WHERE n.age > 18 AND n.active = true
WHERE n.role = 'admin' OR n.role = 'mod'
WHERE NOT n.deleted
WHERE (n.a = 1) XOR (n.b = 1)
```

***

## String Operators

| Operator | Syntax | Description | Example |
|----------|--------|-------------|---------|
| Starts with | `s STARTS WITH prefix` | Prefix match | `n.name STARTS WITH 'Al'` |
| Ends with | `s ENDS WITH suffix` | Suffix match | `n.email ENDS WITH '.com'` |
| Contains | `s CONTAINS sub` | Substring search | `n.bio CONTAINS 'engineer'` |
| Regex match | `s =~ pattern` | PCRE regex; full-string match | `n.name =~ 'Al.*'` |
| Concatenation | `s1 + s2` | Join two strings | `n.first + ' ' + n.last` |

All string operators are case-sensitive. `=~` uses PCRE syntax via the `regexp()` SQL function registered by the extension.

**Examples**

```cypher
WHERE n.name STARTS WITH 'A'
WHERE n.email ENDS WITH '.org'
WHERE n.bio CONTAINS 'graph'
WHERE n.code =~ '[A-Z]{3}[0-9]+'
```

***

## List Operators

| Operator | Syntax | Description | Example |
|----------|--------|-------------|---------|
| Membership | `x IN list` | `true` if `x` is an element of `list` | `n.role IN ['admin', 'mod']` |
| Concatenation | `list1 + list2` | Combine two lists | `[1,2] + [3,4]` → `[1,2,3,4]` |
| Index access | `list[index]` | Element at 0-based index; negative index counts from end | `list[0]`, `list[-1]` |
| Slice | `list[start..end]` | Sublist from `start` (inclusive) to `end` (exclusive) | `list[1..3]` |

**Examples**

```cypher
WHERE n.status IN ['active', 'pending']
RETURN [1, 2] + [3, 4]       -- [1, 2, 3, 4]
RETURN [10, 20, 30][1]       -- 20
RETURN [10, 20, 30, 40][1..3] -- [20, 30]
```

***

## Null Operators

| Operator | Syntax | Description |
|----------|--------|-------------|
| Is null | `expr IS NULL` | `true` if expression is `null` |
| Is not null | `expr IS NOT NULL` | `true` if expression is not `null` |

**Examples**

```cypher
WHERE n.email IS NOT NULL
WHERE n.deletedAt IS NULL
```

***

## Property Access Operators

| Operator | Syntax | Description |
|----------|--------|-------------|
| Dot notation | `entity.property` | Access a property by name |
| String subscript | `entity['property']` | Access a property by string key |
| Nested dot | `n.a.b` | Access nested JSON field (requires `a` to be a JSON-type property) |

String-key subscript (`n['key']`) is normalized at transform time to the same SQL as dot notation. Both forms are equivalent.

**Examples**

```cypher
RETURN n.name
RETURN n['name']
RETURN n.address.city     -- requires address stored as JSON
```
