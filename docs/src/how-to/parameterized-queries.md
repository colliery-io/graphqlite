# Parameterized Queries

Parameterized queries prevent SQL injection and properly handle special characters. This guide shows how to use them.

## Basic Usage

Use `$parameter` syntax in Cypher and pass a JSON object:

```python
import json

# Named parameters
params = json.dumps({"name": "Alice", "age": 30})
results = g.query(
    "MATCH (n:Person {name: $name}) WHERE n.age > $age RETURN n",
    params
)
```

## With the Graph API

The `Graph.query()` method accepts parameters as the second argument:

```python
from graphqlite import Graph

g = Graph(":memory:")

# Create with parameters
g.query(
    "CREATE (n:Person {name: $name, age: $age})",
    '{"name": "Bob", "age": 25}'
)

# Query with parameters
results = g.query(
    "MATCH (n:Person) WHERE n.age >= $min_age RETURN n.name",
    '{"min_age": 21}'
)
```

## With Raw SQL

When using the SQLite interface directly:

```sql
SELECT cypher(
    'MATCH (n:Person {name: $name}) RETURN n',
    '{"name": "Alice"}'
);
```

## Parameter Types

Parameters support all JSON types:

```python
params = json.dumps({
    "string_val": "hello",
    "int_val": 42,
    "float_val": 3.14,
    "bool_val": True,
    "null_val": None,
    "array_val": [1, 2, 3]
})
```

## Use Cases

### User Input

Always use parameters for user-provided values:

```python
def search_by_name(user_input: str):
    # Safe - user input is parameterized
    return g.query(
        "MATCH (n:Person {name: $name}) RETURN n",
        json.dumps({"name": user_input})
    )
```

### Batch Operations

```python
people = [
    {"name": "Alice", "age": 30},
    {"name": "Bob", "age": 25},
    {"name": "Carol", "age": 35},
]

for person in people:
    g.query(
        "CREATE (n:Person {name: $name, age: $age})",
        json.dumps(person)
    )
```

### Complex Values

Parameters handle special characters automatically:

```python
# This works correctly even with quotes and newlines
text = "He said \"hello\"\nand then left."
g.query(
    "CREATE (n:Note {content: $text})",
    json.dumps({"text": text})
)
```

## Benefits

1. **Security**: Prevents Cypher injection attacks
2. **Correctness**: Properly handles quotes, newlines, and special characters
3. **Performance**: Query plans can be cached (future optimization)
4. **Clarity**: Separates query logic from data

## Common Patterns

### Optional Parameters

```python
def search(name: str = None, min_age: int = None):
    conditions = []
    params = {}

    if name:
        conditions.append("n.name = $name")
        params["name"] = name
    if min_age:
        conditions.append("n.age >= $min_age")
        params["min_age"] = min_age

    where = f"WHERE {' AND '.join(conditions)}" if conditions else ""

    return g.query(
        f"MATCH (n:Person) {where} RETURN n",
        json.dumps(params) if params else None
    )
```

### Lists in Parameters

```python
names = ["Alice", "Bob", "Carol"]
results = g.query(
    "MATCH (n:Person) WHERE n.name IN $names RETURN n",
    json.dumps({"names": names})
)
```
