---
id: map-projections
level: initiative
title: "Map Projections"
short_code: "GQLITE-I-0011"
created_at: 2025-12-20T02:01:03.842075+00:00
updated_at: 2025-12-22T03:31:16.746168+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: true

tags:
  - "#initiative"
  - "#phase/completed"


exit_criteria_met: false
estimated_complexity: M
strategy_id: NULL
initiative_id: map-projections
---

# Map Projections Initiative

## Context

Map projections are missing from the current implementation, preventing creation of structured map/object data from graph properties. Map projections like `{name: n.name, age: n.age}` and `n{.name, .age}` are essential for data formatting, API responses, and creating structured result sets from graph queries.

## Goals & Non-Goals

**Goals:**
- Implement map literal syntax: `{key1: value1, key2: value2}`
- Support map projection shorthand: `n{.property1, .property2}`
- Enable property selection with aliasing: `n{name: .full_name, .age}`
- Generate SQL JSON objects using SQLite json_object() function
- Support nested map structures and complex value expressions
- Handle mixed property types (strings, numbers, booleans, nulls)

**Non-Goals:**
- Map manipulation functions (merge, keys, values) - separate initiative
- Complex map operations and computations
- Non-standard map syntax extensions
- Performance optimization for large maps

## Detailed Design

Implement maps as JSON objects using SQLite's JSON support:

1. **Grammar Implementation** (cypher_gram.y):
   - Map literal: `'{' [key ':' value (',' key ':' value)*] '}'`
   - Map projection: `variable '{' [projection_item (',' projection_item)*] '}'`
   - Projection items: `.property`, `alias: .property`, `alias: expression`

2. **AST Structure**:
   ```c
   typedef struct {
       cypher_astnode_t **keys;           // Map keys (identifiers or expressions)
       cypher_astnode_t **values;         // Map values (expressions)
       size_t entry_count;               // Number of key-value pairs
       bool is_projection;               // true for n{...}, false for {...}
       cypher_astnode_t *base_variable;  // Base variable for projections
   } cypher_ast_map_t;
   ```

3. **SQL Generation Strategy**:
   - Use SQLite `json_object()` function for map creation
   - Map literal: `{name: 'John', age: 30}` → `json_object('name', 'John', 'age', 30)`
   - Property projection: `n{.name, .age}` → `json_object('name', n.name, 'age', n.age)`

4. **Map Projection Forms**:
   - Property selection: `n{.name, .age}` extracts specific properties
   - Property aliasing: `n{fullName: .name, years: .age}` with custom key names
   - Mixed projections: `n{.name, computed: n.age * 2}` combining properties and expressions

## Alternatives Considered

1. **String concatenation approach**: Build maps as concatenated strings - rejected as it's error-prone and not type-safe
2. **Separate map table storage**: Store maps in dedicated tables - rejected due to complexity and poor performance
3. **Custom map serialization format**: Invent proprietary format - rejected as it prevents JSON interoperability
4. **Application-level map construction**: Build maps in application code - rejected as it breaks query composition

## Implementation Plan

1. **Grammar and AST Development**: Add map literal and projection grammar rules
2. **Map Literal Implementation**: Implement SQL generation using json_object()
3. **Map Projection Implementation**: Implement property projection syntax n{.prop1, .prop2}
4. **Property Access Integration**: Enable property access on map values: map.key
5. **Testing and Integration**: Test complex nested maps and mixed projection types

## Testing Strategy

**Map Literal Testing:**
- Basic literals: `{name: 'John', age: 30, active: true}`
- Empty maps: `{}`
- Maps with null values: `{name: 'John', age: null}`

**Map Projection Testing:**
- Simple projection: `n{.name, .age}`
- Property aliasing: `n{fullName: .name, years: .age}`
- Mixed projections: `n{.name, computed: n.age * 2}`

**Property Access on Maps:**
- Direct access: `WITH {name: 'John'} AS map RETURN map.name`
- Chained access: `map.person.name` for nested maps
- Non-existent key access (should return null)

**Integration Testing:**
- Maps in RETURN clauses, WHERE clauses
- Maps with other advanced features (CASE, list comprehensions)