---
title: Parameterized Queries
---

Parameterized queries pass values separately from the Cypher query text. They are the recommended approach for any query that incorporates user input, external data, or strings that may contain special characters.

## Why Use Parameters

- **Security.** Parameters prevent Cypher injection — the same class of attack as SQL injection. A value like `' OR 1=1 --` in a parameter is treated as a literal string, not query syntax.
- **Correctness.** Values with single quotes, backslashes, newlines, or Unicode characters work without manual escaping.
- **Clarity.** Query logic and data stay separate, making queries easier to read and reuse.

## Named Parameters with $

GraphQLite uses `$name` syntax for named parameters. Parameter names map to keys in the dictionary (Python) or JSON object (SQL) you pass alongside the query:

```cypher
MATCH (n:Person {name: $name}) WHERE n.age > $min_age RETURN n
```

Parameters can appear anywhere a literal value is valid: in property predicates, `WHERE` clauses, `SET` assignments, and `CREATE` property maps.

## Python: Connection.cypher()

Pass a dictionary as the second argument to `Connection.cypher()`:

```python
from graphqlite import connect

conn = connect(":memory:")

# CREATE with parameters
conn.cypher(
    "CREATE (n:Person {name: $name, age: $age, city: $city})",
    {"name": "Alice", "age": 30, "city": "London"}
)

# MATCH with parameters
results = conn.cypher(
    "MATCH (n:Person) WHERE n.age >= $min_age AND n.city = $city RETURN n.name, n.age",
    {"min_age": 25, "city": "London"}
)
for row in results:
    print(f"{row['n.name']}, age {row['n.age']}")

# SET with parameters
conn.cypher(
    "MATCH (n:Person {name: $name}) SET n.status = $status",
    {"name": "Alice", "status": "active"}
)
```

## Python: Graph.query() with Parameters

`Graph.query()` accepts an optional `params` argument:

```python
from graphqlite import Graph

g = Graph(":memory:")
g.upsert_node("alice", {"name": "Alice", "age": 30}, "Person")
g.upsert_node("bob",   {"name": "Bob",   "age": 25}, "Person")

results = g.query(
    "MATCH (n:Person) WHERE n.age >= $min_age RETURN n.name ORDER BY n.name",
    params={"min_age": 26}
)
for row in results:
    print(row["n.name"])  # Alice
```

## SQL Interface

Pass the parameters as a JSON string in the second argument to `cypher()`:

```sql
-- Single parameter
SELECT cypher(
    'MATCH (n:Person {name: $name}) RETURN n.age',
    '{"name": "Alice"}'
);

-- Multiple parameters
SELECT cypher(
    'MATCH (n:Person) WHERE n.age >= $min AND n.age <= $max RETURN n.name',
    '{"min": 25, "max": 35}'
);

-- CREATE with parameters
SELECT cypher(
    'CREATE (n:Event {title: $title, year: $year})',
    '{"title": "Graph Summit", "year": 2025}'
);
```

In Python with a raw `sqlite3` connection:

```python
import sqlite3, json, graphqlite

conn = sqlite3.connect(":memory:")
graphqlite.load(conn)

params = json.dumps({"name": "Alice", "age": 30})
conn.execute("SELECT cypher('CREATE (n:Person {name: $name, age: $age})', ?)", [params])
conn.commit()

params = json.dumps({"min_age": 25})
rows = conn.execute(
    "SELECT cypher('MATCH (n:Person) WHERE n.age >= $min_age RETURN n.name', ?)",
    [params]
).fetchall()
```

## Rust

In Rust, embed parameter values directly into the query string using `format!`. Full parameterized binding is planned for a future release.

```rust
use graphqlite::Connection;

fn main() -> graphqlite::Result<()> {
    let conn = Connection::open_in_memory()?;

    // Safe integer embedding
    let min_age: i32 = 25;
    let results = conn.cypher(&format!(
        "MATCH (n:Person) WHERE n.age >= {} RETURN n.name AS name",
        min_age
    ))?;

    for row in &results {
        println!("{}", row.get::<String>("name")?);
    }

    // For strings, pass via JSON through the SQL cypher() function
    let name = "Alice";
    let params = serde_json::json!({"name": name, "age": 30});
    conn.execute_sql(
        "SELECT cypher('CREATE (n:Person {name: $name, age: $age})', ?)",
        &[&params.to_string()],
    )?;

    Ok(())
}
```

## Supported Parameter Types

Parameters map to JSON types, which GraphQLite converts to Cypher-compatible values:

| JSON Type | Cypher Type | Python Example | Rust Type |
|-----------|-------------|----------------|-----------|
| String | String | `"hello"` | `String`, `&str` |
| Integer | Integer | `42` | `i32`, `i64` |
| Float | Float | `3.14` | `f64` |
| Boolean | Boolean | `True` / `False` | `bool` |
| Null | Null | `None` | `Option<T>` |
| Array | List | `[1, 2, 3]` | `Vec<T>` |
| Object | Map | `{"k": "v"}` | `serde_json::Value` |

```python
conn.cypher(
    "CREATE (n:Record {label: $label, count: $count, ratio: $ratio, active: $active, tags: $tags})",
    {
        "label": "alpha",
        "count": 100,
        "ratio": 0.75,
        "active": True,
        "tags": ["graph", "database", "cypher"],
    }
)
```

## Common Patterns

### User Input Safety

Always parameterize user-provided values:

```python
def find_person(user_input: str):
    return conn.cypher(
        "MATCH (n:Person {name: $name}) RETURN n",
        {"name": user_input}   # Safe regardless of what user_input contains
    )

# These all work correctly and safely:
find_person("Alice")
find_person("O'Brien")
find_person("Robert'); DROP TABLE nodes;--")
```

### Dynamic Filtering

Build the parameter dictionary dynamically; keep the query shape stable:

```python
def search_people(name=None, min_age=None, city=None):
    conditions = []
    params = {}

    if name is not None:
        conditions.append("n.name = $name")
        params["name"] = name
    if min_age is not None:
        conditions.append("n.age >= $min_age")
        params["min_age"] = min_age
    if city is not None:
        conditions.append("n.city = $city")
        params["city"] = city

    where = f"WHERE {' AND '.join(conditions)}" if conditions else ""
    query = f"MATCH (n:Person) {where} RETURN n.name, n.age, n.city ORDER BY n.name"

    return conn.cypher(query, params if params else None)
```

### IN Clause with Lists

Pass a list parameter and use `IN`:

```python
names = ["Alice", "Bob", "Carol"]
results = conn.cypher(
    "MATCH (n:Person) WHERE n.name IN $names RETURN n.name, n.age",
    {"names": names}
)
```

### Batch Inserts

Loop over a dataset and reuse the same parameterized query:

```python
people = [
    {"name": "Alice",  "age": 30, "city": "London"},
    {"name": "Bob",    "age": 25, "city": "Paris"},
    {"name": "Carol",  "age": 35, "city": "Berlin"},
    {"name": "Dave",   "age": 28, "city": "London"},
]

for person in people:
    conn.cypher(
        "CREATE (n:Person {name: $name, age: $age, city: $city})",
        person
    )
```

For very large datasets (thousands of nodes), the [Bulk Import API](../bulk-import/) is significantly faster.

### Values with Special Characters

Parameters handle all special characters automatically — no need for `escape_string()`:

```python
documents = [
    {"id": "d1", "text": "It's a lovely day.\nThe sun is shining."},
    {"id": "d2", "text": 'He said "hello" and left.'},
    {"id": "d3", "text": "Path: C:\\Users\\alice\\documents"},
]

for doc in documents:
    conn.cypher(
        "CREATE (n:Document {doc_id: $id, content: $text})",
        doc
    )
```

### Optional / Nullable Values

Pass `None` for parameters that may be absent:

```python
conn.cypher(
    "CREATE (n:Person {name: $name, nickname: $nickname})",
    {"name": "Alice", "nickname": None}  # nickname will be stored as null
)
```

## Parameters vs. String Interpolation

Avoid building queries by string formatting or concatenation:

```python
# Dangerous — susceptible to injection and escaping bugs
name = user_input
conn.cypher(f"MATCH (n {{name: '{name}'}}) RETURN n")

# Correct
conn.cypher("MATCH (n {name: $name}) RETURN n", {"name": name})
```

The only case where string formatting is appropriate is for **structural** parts of a query that cannot be parameterized, such as label names or property names. Even then, validate the value against an allowlist before interpolating it.
