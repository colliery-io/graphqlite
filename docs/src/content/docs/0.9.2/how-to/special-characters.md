---
title: Handling Special Characters
slug: 0.9.2/how-to/special-characters
---

Certain characters in property values or identifiers require special treatment in Cypher queries. This guide covers the three main categories — property values, relationship types, and property names — and the right approach for each.

## Property Values

### The Problem

When property values are interpolated directly into a Cypher string, control characters and punctuation can break parsing or produce silently wrong results:

```python
# This will cause a syntax error or corrupt the query
g.query("CREATE (n:Note {text: 'It's a lovely day'})")  # SyntaxError: unmatched quote

# This may parse but produce no results
g.query("CREATE (n:Note {text: 'Line1\nLine2'})")      # newline inside string literal
```

Characters that need special handling:

| Character | Risk |
|-----------|------|
| `'` single quote | Terminates the string literal early |
| `\` backslash | Starts an escape sequence |
| `\n` newline | Splits the literal across lines; breaks parsing |
| `\r` carriage return | Same as newline |
| `\t` tab | Less common; can cause issues in some parsers |
| `"` double quote | Less common in Cypher but still problematic |

### Solution 1: Parameterized Queries (Recommended)

Parameters bypass the Cypher string parser entirely. The value is passed as JSON outside the query text, so no escaping is required:

```python
from graphqlite import connect

conn = connect(":memory:")

# Single quotes, newlines, backslashes — all handled automatically
conn.cypher(
    "CREATE (n:Note {title: $title, content: $content})",
    {
        "title": "Alice's Report",
        "content": "Line 1\nLine 2\nBackslash: \\ done.",
    }
)

# Retrieve and verify
results = conn.cypher(
    "MATCH (n:Note {title: $title}) RETURN n.content",
    {"title": "Alice's Report"}
)
print(results[0]["n.content"])
# Line 1
# Line 2
# Backslash: \ done.
```

See [Parameterized Queries](../parameterized-queries/) for the full guide.

### Solution 2: The Graph API

The high-level `Graph` API handles escaping internally for `upsert_node` and `upsert_edge`. Pass raw Python strings; no escaping is needed:

```python
from graphqlite import Graph

g = Graph(":memory:")

g.upsert_node("note1", {
    "title": "Alice's Report",
    "content": "Line 1\nLine 2\nBackslash: \\",
    "author": 'Bob said "hello"',
}, label="Note")

node = g.get_node("note1")
print(node["properties"]["content"])  # Line 1\nLine 2\nBackslash: \
```

### Solution 3: escape_string() for Manual Queries

If you must build a Cypher string by hand, use the `escape_string()` utility from the graphqlite package:

```python
from graphqlite import Graph, escape_string

g = Graph(":memory:")

raw_text = "Alice's note:\nLine 1\nLine 2"
safe_text = escape_string(raw_text)  # Escapes quotes, newlines, backslashes

g.query(f"CREATE (n:Note {{content: '{safe_text}'}})")
```

`escape_string()` applies these transformations in order:

1. `\` → `\\` (backslashes first, so they are not double-escaped)
2. `'` → `\'` (single quotes)
3. `\n` → ` ` (newlines replaced with a space)
4. `\r` → ` ` (carriage returns replaced with a space)
5. `\t` → ` ` (tabs replaced with a space)

If preserving newlines in stored values is important, use parameterized queries instead — `escape_string()` converts them to spaces.

## Relationship Types

Relationship type names must be valid identifiers. The `sanitize_rel_type()` utility converts arbitrary strings into safe type names:

```python
from graphqlite import sanitize_rel_type

print(sanitize_rel_type("has-friend"))   # HAS_FRIEND
print(sanitize_rel_type("works with"))  # WORKS_WITH
print(sanitize_rel_type("type/1"))      # TYPE_1
```

The function:

- Converts to uppercase
- Replaces hyphens, spaces, and slashes with underscores
- Strips other non-alphanumeric characters

Use it whenever relationship types come from user input or external data:

```python
from graphqlite import Graph, sanitize_rel_type

g = Graph(":memory:")

user_provided_type = "works-with"
safe_type = sanitize_rel_type(user_provided_type)  # WORKS_WITH

g.upsert_edge("alice", "bob", {"project": "Apollo"}, rel_type=safe_type)
```

## Property Names and Identifiers

### Backtick Quoting

Property names that conflict with Cypher keywords or contain special characters can be quoted with backticks:

```python
# "type", "end", "order" are reserved Cypher keywords
g.connection.cypher("MATCH (n) WHERE n.`type` = 'A' RETURN n.`order`")

# Property names with spaces or hyphens
g.connection.cypher("CREATE (n:Item {`item-code`: 'XYZ-001', `display name`: 'Widget'})")
```

### Using CYPHER_RESERVED

The `CYPHER_RESERVED` set contains all reserved Cypher keywords. Check before using a string as a label or property name:

```python
from graphqlite import CYPHER_RESERVED

def safe_label(name: str) -> str:
    if name.upper() in CYPHER_RESERVED:
        return f"`{name}`"
    return name

label = safe_label("order")  # "`order`"
label = safe_label("Person") # "Person"

g.connection.cypher(f"CREATE (n:{label} {{id: 'o1'}})")
```

## Common Pitfalls

### Symptom: MATCH returns nothing after CREATE

**Cause:** Newlines or carriage returns in property values broke the inline Cypher string during creation. The node was stored but its properties are corrupt or missing.

**Fix:** Use parameterized queries for any value that may contain whitespace control characters.

```python
# Broken
name_with_newline = "Alice\nMitchell"
g.query(f"CREATE (n:Person {{name: '{name_with_newline}'}})")

# Fixed
g.connection.cypher("CREATE (n:Person {name: $name})", {"name": name_with_newline})
```

### Symptom: SyntaxError on CREATE

**Cause:** Unescaped single quotes in the value.

```python
# Broken
g.query("CREATE (n:Quote {text: 'It's a test'})")   # SyntaxError

# Fixed: use parameters
g.connection.cypher("CREATE (n:Quote {text: $text})", {"text": "It's a test"})

# Or: escape manually (less preferred)
g.query("CREATE (n:Quote {text: 'It\\'s a test'})")
```

### Symptom: Relationship type with hyphens not found

**Cause:** Hyphens are not valid in unquoted Cypher identifiers. `CREATE (a)-[:has-friend]->(b)` is parsed as `has` minus `friend`.

**Fix:** Use underscores or sanitize the type name:

```python
from graphqlite import sanitize_rel_type

# Broken
g.query("CREATE (a:Person {name: 'A'})-[:has-friend]->(b:Person {name: 'B'})")

# Fixed: use sanitized type
rel_type = sanitize_rel_type("has-friend")  # HAS_FRIEND
g.upsert_edge("alice", "bob", {}, rel_type=rel_type)
```

### Symptom: Property access on reserved keyword property name

**Cause:** `n.type` is parsed as `n` followed by the keyword `type`, not as property access.

**Fix:** Quote with backticks.

```python
# Broken
results = g.query("MATCH (n) WHERE n.type = 'Product' RETURN n")

# Fixed
results = g.query("MATCH (n) WHERE n.`type` = 'Product' RETURN n")
```

## Best Practices

1. **Always use parameterized queries for user-supplied data.** This is the only safe approach for arbitrary values.
2. **Use the Graph API (`upsert_node`, `upsert_edge`) for CRUD operations.** It handles escaping automatically.
3. **Call `sanitize_rel_type()` for dynamic relationship types.** Any type name derived from external input needs sanitization.
4. **Backtick-quote property names that are reserved words.** Check against `CYPHER_RESERVED` when property names come from a schema or API response.
5. **Validate and strip control characters at ingestion time** if your data comes from sources that may embed nulls or other non-printable characters.
