# Cypher Operators

## Comparison Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `=` | Equal | `n.age = 30` |
| `<>` | Not equal | `n.status <> 'deleted'` |
| `<` | Less than | `n.age < 18` |
| `>` | Greater than | `n.age > 65` |
| `<=` | Less than or equal | `n.score <= 100` |
| `>=` | Greater than or equal | `n.score >= 0` |

## Boolean Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `AND` | Logical and | `n.age > 18 AND n.active = true` |
| `OR` | Logical or | `n.role = 'admin' OR n.role = 'mod'` |
| `NOT` | Logical not | `NOT n.deleted` |
| `XOR` | Exclusive or | `a.flag XOR b.flag` |

## Null Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `IS NULL` | Check for null | `n.email IS NULL` |
| `IS NOT NULL` | Check for non-null | `n.email IS NOT NULL` |

## String Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `STARTS WITH` | Prefix match | `n.name STARTS WITH 'A'` |
| `ENDS WITH` | Suffix match | `n.email ENDS WITH '.com'` |
| `CONTAINS` | Substring match | `n.bio CONTAINS 'developer'` |
| `=~` | Regex match | `n.email =~ '.*@gmail\\.com'` |

## List Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `IN` | List membership | `n.status IN ['active', 'pending']` |
| `+` | List concatenation | `[1, 2] + [3, 4]` → `[1, 2, 3, 4]` |
| `[index]` | Index access | `list[0]` (first element) |

## Arithmetic Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `+` | Addition | `n.price + tax` |
| `-` | Subtraction | `n.total - discount` |
| `*` | Multiplication | `n.quantity * n.price` |
| `/` | Division | `n.total / n.count` |
| `%` | Modulo | `n.id % 10` |

## String Concatenation

| Operator | Description | Example |
|----------|-------------|---------|
| `+` | Concatenate strings | `n.first + ' ' + n.last` |

## Property Access

| Operator | Description | Example |
|----------|-------------|---------|
| `.` | Property access | `n.name` |

## Operator Precedence

From highest to lowest:

1. `.` `[]` - Property/index access
2. `*` `/` `%` - Multiplication, division, modulo
3. `+` `-` - Addition, subtraction
4. `=` `<>` `<` `>` `<=` `>=` - Comparison
5. `IS NULL` `IS NOT NULL`
6. `IN` `STARTS WITH` `ENDS WITH` `CONTAINS` `=~`
7. `NOT`
8. `AND`
9. `XOR`
10. `OR`

Use parentheses to override precedence:

```cypher
WHERE (n.age > 18 OR n.verified) AND n.active
```
