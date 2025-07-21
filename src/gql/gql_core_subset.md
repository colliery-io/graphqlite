# GraphQLite Core GQL Subset

## Overview

This document defines the core subset of ISO/IEC 39075 GQL that GraphQLite will support in the initial implementation. We focus on the most essential graph operations while maintaining compatibility with the standard.

## Supported Query Types

### 1. MATCH Queries
```gql
-- Basic node matching
MATCH (n)
MATCH (n:Person)
MATCH (n:Person {name: "Alice"})

-- Basic edge matching  
MATCH (a)-[r]->(b)
MATCH (a)-[r:KNOWS]->(b)
MATCH (a:Person)-[r:KNOWS]->(b:Person)

-- Combined patterns
MATCH (a:Person {name: "Alice"})-[r:KNOWS]->(b:Person)
```

### 2. WHERE Clauses
```gql
-- Property conditions
WHERE n.age > 25
WHERE n.name = "Alice"
WHERE n.active = true

-- Logical operators
WHERE n.age > 25 AND n.name STARTS WITH "A"
WHERE n.age < 30 OR n.department = "Engineering"
WHERE NOT n.retired

-- Property existence
WHERE n.email IS NOT NULL
```

### 3. RETURN Clauses
```gql
-- Return nodes/edges
RETURN n
RETURN r
RETURN a, b

-- Return properties
RETURN n.name, n.age
RETURN a.name AS person_name

-- Simple aggregation
RETURN COUNT(n)
RETURN COUNT(DISTINCT n.department)
```

### 4. CREATE Operations
```gql
-- Create nodes
CREATE (n:Person {name: "Bob", age: 30})

-- Create edges
CREATE (a)-[r:KNOWS {since: 2020}]->(b)

-- Create patterns
CREATE (a:Person {name: "Alice"})-[r:KNOWS]->(b:Person {name: "Bob"})
```

### 5. SET Operations
```gql
-- Set properties
SET n.age = 31
SET n.updated = "2024-01-15"

-- Set multiple properties
SET n.age = 31, n.status = "active"
```

### 6. DELETE Operations
```gql
-- Delete nodes (cascades to edges)
DELETE n

-- Delete edges
DELETE r
```

## Core Data Types

### Primitive Types
- **Integer**: `42`, `-17`
- **String**: `"hello"`, `'world'`  
- **Boolean**: `true`, `false`
- **Null**: `null`

### Comparison Operators
- `=`, `<>`, `!=`
- `<`, `<=`, `>`, `>=`
- `IS NULL`, `IS NOT NULL`

### String Operators
- `STARTS WITH`
- `ENDS WITH`  
- `CONTAINS`

### Logical Operators
- `AND`, `OR`, `NOT`

## Grammar Subset (EBNF)

```ebnf
query = match_query | create_query | set_query | delete_query ;

match_query = "MATCH" pattern_list [where_clause] "RETURN" return_list ;
create_query = "CREATE" pattern_list ;
set_query = "MATCH" pattern_list [where_clause] "SET" assignment_list ;
delete_query = "MATCH" pattern_list [where_clause] "DELETE" identifier_list ;

pattern_list = pattern ("," pattern)* ;
pattern = node_pattern [edge_pattern node_pattern]* ;

node_pattern = "(" [identifier] [label_spec] [property_map] ")" ;
edge_pattern = "-" "[" [identifier] [type_spec] [property_map] "]" "->" ;

label_spec = ":" identifier ;
type_spec = ":" identifier ;
property_map = "{" property_list "}" ;
property_list = property ("," property)* ;
property = identifier ":" literal ;

where_clause = "WHERE" expression ;
expression = or_expression ;
or_expression = and_expression ("OR" and_expression)* ;
and_expression = not_expression ("AND" not_expression)* ;
not_expression = ["NOT"] comparison_expression ;
comparison_expression = primary_expression [comparison_op primary_expression] ;

primary_expression = property_access | literal | "(" expression ")" ;
property_access = identifier "." identifier ;

return_list = return_item ("," return_item)* ;
return_item = expression ["AS" identifier] | aggregate_function ;
aggregate_function = "COUNT" "(" ["DISTINCT"] expression ")" ;

assignment_list = assignment ("," assignment)* ;
assignment = property_access "=" literal ;

identifier_list = identifier ("," identifier)* ;

literal = string_literal | integer_literal | boolean_literal | null_literal ;
comparison_op = "=" | "<>" | "!=" | "<" | "<=" | ">" | ">=" | "IS NULL" | "IS NOT NULL" | "STARTS WITH" | "ENDS WITH" | "CONTAINS" ;
```

## Example Queries

### Find all people
```gql
MATCH (n:Person) RETURN n.name, n.age
```

### Find friends of Alice
```gql
MATCH (a:Person {name: "Alice"})-[r:KNOWS]->(b:Person) 
RETURN b.name
```

### Find people over 25 in Engineering
```gql
MATCH (n:Person) 
WHERE n.age > 25 AND n.department = "Engineering"
RETURN n.name, n.age
```

### Create a person and relationship
```gql
CREATE (alice:Person {name: "Alice", age: 30})-[r:KNOWS]->(bob:Person {name: "Bob", age: 25})
```

### Update person's age
```gql
MATCH (n:Person {name: "Alice"}) 
SET n.age = 31
```

### Delete inactive users
```gql
MATCH (n:Person) 
WHERE n.active = false 
DELETE n
```

## Implementation Priority

1. **Phase 1**: Basic MATCH with simple patterns and RETURN
2. **Phase 2**: WHERE clauses with basic comparisons
3. **Phase 3**: CREATE operations
4. **Phase 4**: SET and DELETE operations
5. **Phase 5**: Aggregation functions and advanced features

This subset provides essential graph operations while keeping the parser manageable and maintainable.