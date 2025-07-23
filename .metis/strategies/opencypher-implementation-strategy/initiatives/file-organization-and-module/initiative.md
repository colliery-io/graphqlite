---
id: file-organization-and-module
level: initiative
title: "File Organization and Module Separation"
created_at: 2025-07-23T23:01:37.242743+00:00
updated_at: 2025-07-23T23:01:37.242743+00:00
parent: opencypher-implementation-strategy
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/discovery"


exit_criteria_met: false
estimated_complexity: M
---

# File Organization and Module Separation Initiative

## Context

The GraphQLite codebase suffers from a monolithic architecture with the main `graphqlite.c` file containing 3,004 lines of code spanning multiple distinct functional areas. This violates the single responsibility principle and creates significant maintenance challenges.

**Current Architecture Problems:**
- **Monolithic Structure**: Single file handles schema management, query execution, property handling, serialization, and extension registration
- **Mixed Concerns**: Database operations, business logic, and presentation layer are intermingled
- **Testing Challenges**: Difficult to isolate and unit test individual components
- **Development Bottlenecks**: Multiple developers cannot work effectively on different functional areas
- **Code Navigation**: Finding specific functionality requires searching through thousands of lines

**Clear Functional Boundaries Identified:** Despite the monolithic structure, analysis revealed distinct functional areas with natural boundaries that can be cleanly separated into focused modules.

This initiative will break down the monolithic file into cohesive, well-organized modules that reflect the actual functional architecture of the system.

## Goals & Non-Goals

**Goals:**
- Break down 3,004-line monolithic `graphqlite.c` into focused, cohesive modules
- Create clear separation of concerns with well-defined module boundaries
- Enable parallel development by multiple team members on different components
- Improve code discoverability and maintainability through logical organization
- Establish foundation for future modular development and testing
- Reduce average file size to under 500 lines per module

**Non-Goals:**
- Changing existing public API or breaking backward compatibility
- Performance optimization (focus is on organization)
- Adding new features or functionality
- Modifying generated grammar files (cypher.tab.c, lex.yy.c)
- Changing build system beyond adding new source files

## Detailed Design

## Proposed Modular Architecture

### Module 1: Schema Management (`schema.c`, `schema.h`)
**Responsibility**: Database schema creation and management
**Functions**: `create_schema()`, database initialization, index creation, schema validation
**Size Estimate**: ~200 lines
**Dependencies**: SQLite, db_utils (from consolidation initiative)

### Module 2: Property System (`property.c`, `property.h`)
**Responsibility**: EAV property management and operations
**Functions**: `get_or_create_property_key_id()`, `insert_node_property()`, `insert_edge_property()`, `extract_property_from_ast()`
**Size Estimate**: ~300 lines
**Dependencies**: Schema module, memory_utils, db_utils

### Module 3: Expression Evaluation (`expression.c`, `expression.h`)
**Responsibility**: WHERE clause and expression processing
**Functions**: `eval_result_t` operations, `evaluate_expression()`, `compare_eval_results()`, property lookups
**Size Estimate**: ~400 lines
**Dependencies**: Property system, AST module

### Module 4: Query Execution Engine (Split into sub-modules)

#### 4a: CREATE Operations (`query_create.c`)
**Functions**: `execute_create_node()`, `execute_create_relationship()`, `execute_create_path()`
**Size Estimate**: ~350 lines

#### 4b: MATCH Operations (`query_match.c`) 
**Functions**: `execute_match_node()`, `execute_match_relationship()`, `execute_variable_length_match()`
**Size Estimate**: ~400 lines

#### 4c: DELETE Operations (`query_delete.c`)
**Functions**: `execute_delete_node()`, `execute_delete_relationship()`
**Size Estimate**: ~200 lines

#### 4d: Query Common (`query.h`)
**Shared**: Common query execution utilities and types
**Size Estimate**: ~100 lines (header)

### Module 5: Serialization (`serialization.c`, `serialization.h`)
**Responsibility**: Entity serialization for OpenCypher JSON format
**Functions**: `serialize_node_entity()`, `serialize_relationship_entity()`, JSON formatting utilities
**Size Estimate**: ~250 lines (after consolidation)
**Dependencies**: Property system, serialization_utils

### Module 6: Extension Interface (`extension.c`, `extension.h`)
**Responsibility**: SQLite extension registration and main entry point
**Functions**: `graphqlite_cypher_func()`, `sqlite3_graphqlite_init()`, SQLite integration
**Size Estimate**: ~150 lines
**Dependencies**: All query modules, parser

## Module Dependency Graph
```
extension.c
├── query_create.c → property.c → schema.c
├── query_match.c → expression.c → property.c
├── query_delete.c → property.c
└── serialization.c → property.c

schema.c (base layer - no dependencies on other modules)
```

## Migration Strategy
- **Low Risk**: Clear functional boundaries with minimal circular dependencies
- **Incremental**: Extract modules one at a time starting with lowest dependencies
- **Validated**: Each extraction step verified with full test suite
- **API Stable**: Public interface in `graphqlite.h` unchanged throughout process

## Alternatives Considered

{Alternative approaches and why they were rejected}

## Implementation Plan

{Phases and timeline for execution}

## Testing Strategy

{How the initiative will be validated}