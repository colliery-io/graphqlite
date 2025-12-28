# Cypher Support

GraphQLite implements a substantial subset of the Cypher query language.

## Overview

Cypher is a declarative graph query language originally developed by Neo4j. GraphQLite supports the core features needed for most graph operations.

## Quick Reference

| Feature | Support |
|---------|---------|
| Node patterns | ✅ Full |
| Relationship patterns | ✅ Full |
| Variable-length paths | ✅ Full |
| Parameterized queries | ✅ Full |
| MATCH/OPTIONAL MATCH | ✅ Full |
| CREATE/MERGE | ✅ Full |
| SET/REMOVE/DELETE | ✅ Full |
| WITH/UNWIND/FOREACH | ✅ Full |
| RETURN with modifiers | ✅ Full |
| Aggregation functions | ✅ Full |
| List comprehensions | ⚠️ Partial |
| CALL procedures | ❌ Not supported |

## Pattern Syntax

### Nodes

```cypher
(n)                           -- Any node
(n:Person)                    -- Node with label
(n:Person {name: 'Alice'})    -- Node with properties
(:Person)                     -- Anonymous node with label
```

### Relationships

```cypher
-[r]->                        -- Outgoing relationship
<-[r]-                        -- Incoming relationship
-[r]-                         -- Either direction
-[:KNOWS]->                   -- Relationship with type
-[r:KNOWS {since: 2020}]->    -- With properties
```

### Variable-Length Paths

```cypher
-[*]->                        -- Any length
-[*2]->                       -- Exactly 2 hops
-[*1..3]->                    -- 1 to 3 hops
-[:KNOWS*1..5]->              -- Typed, 1 to 5 hops
```

## Clauses

See [Clauses Reference](./cypher-clauses.md) for detailed documentation.

## Functions

See [Functions Reference](./cypher-functions.md) for the complete function list.

## Operators

See [Operators Reference](./cypher-operators.md) for comparison and logical operators.

## Implementation Notes

GraphQLite implements standard Cypher with some differences from full implementations:

1. **No CALL procedures** - Use built-in graph algorithm functions instead
2. **No list comprehensions** - Use `UNWIND` for list operations
3. **No CREATE CONSTRAINT** - Use SQLite's constraint mechanisms
4. **No EXPLAIN/PROFILE** - Query analysis not yet implemented
5. **Graph algorithms are functions** - `RETURN pageRank()` syntax
