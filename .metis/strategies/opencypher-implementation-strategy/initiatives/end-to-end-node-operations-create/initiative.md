---
id: end-to-end-node-operations-create
level: initiative
title: "End-to-End Node Operations (CREATE/MATCH)"
created_at: 2025-07-22T01:39:35.969613+00:00
updated_at: 2025-07-22T01:39:35.969613+00:00
parent: opencypher-implementation-strategy
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/discovery"


exit_criteria_met: false
estimated_complexity: M
---

# End-to-End Node Operations (CREATE/MATCH) Initiative

## Context

Build a minimal but complete end-to-end pipeline for basic node operations to validate the entire architecture. This initiative focuses on the simplest possible graph operations: creating nodes and matching them back.

**Core Goal**: Prove that we can parse, execute, and return results for basic OpenCypher queries without relationships or complex patterns.

**Target Queries**:
```cypher
CREATE (n:Person {name: "John"})
MATCH (n:Person) RETURN n
MATCH (n:Person {name: "John"}) RETURN n
```

This "wide before deep" approach validates all major components (parser, AST, executor, storage, results) with minimal complexity, establishing a foundation for more advanced features.

## Goals & Non-Goals

**Goals:**
- Parse CREATE statements for nodes with single label and single property
- Parse MATCH statements for nodes with label and property filtering
- Parse basic RETURN statements for node projection
- Store nodes in SQLite tables with proper schema
- Retrieve nodes based on label and property criteria
- Return query results in structured format
- Establish memory management patterns for AST and results
- Validate entire pipeline with CUnit integration tests
- Demonstrate functional SQLite extension interface

**Non-Goals:**
- Multiple properties per node (future iteration)
- Multiple labels per node (future iteration)
- Relationships/edges between nodes
- Complex WHERE clause expressions
- Advanced RETURN projections (aliases, aggregations)
- Query optimization or indexing
- Error recovery in parser
- Performance optimization
- Variable-length patterns or multi-hop traversals

## Detailed Design

## Architecture Components

### 1. Parser Layer (BISON + FLEX)
- **Minimal BISON Grammar**: Support only CREATE, MATCH, RETURN for nodes
- **FLEX Lexer**: Tokenize keywords (CREATE, MATCH, RETURN), identifiers, strings, symbols
- **AST Nodes**: Basic node types for statements, patterns, variables, literals

### 2. Database Schema
```sql
CREATE TABLE nodes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT NOT NULL,
    property_key TEXT,
    property_value TEXT,
    property_type INTEGER  -- 1=string, 2=integer, etc.
);
```
*Note: Simple single-property design for this iteration*

### 3. Execution Pipeline
- **AST Walker**: Interpret parsed statements
- **CREATE Handler**: Insert nodes into database
- **MATCH Handler**: Query nodes by label/property
- **RETURN Handler**: Format results for output

### 4. Memory Management
- **AST Cleanup**: Free parser-generated trees after execution
- **Result Management**: Clean result structures after consumption
- **Error Handling**: Proper cleanup on parse/execution failures

### 5. API Interface
```c
// SQLite extension entry point
int sqlite3_graphqlite_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi);

// Core query execution
int graphqlite_exec(sqlite3 *db, const char *cypher_query, graphqlite_result_t **result);
```

## Data Flow
1. **Parse**: BISON/FLEX converts query string → AST
2. **Execute**: AST walker performs database operations
3. **Store/Retrieve**: SQLite table operations for nodes
4. **Format**: Convert internal data → result structure
5. **Cleanup**: Free all allocated memory

## Alternatives Considered

**1. Multiple Properties per Node from Start**
- **Rejected**: Adds complexity to schema design and property handling
- **Rationale**: Single property validates the pattern, multiple can be added incrementally

**2. JSON Property Storage**
- **Considered**: Store properties as JSON blob in single column
- **Rejected**: Harder to query and filter, less SQL-friendly
- **Rationale**: Explicit columns make debugging and SQL queries easier

**3. Starting with Relationships**
- **Rejected**: Relationships require source/target node management complexity
- **Rationale**: Nodes are simpler foundation, relationships build on top

**4. Hand-written Parser Instead of BISON**
- **Considered**: Custom recursive descent parser
- **Rejected**: More maintenance burden, less standard
- **Rationale**: BISON provides proven parser generation with good error handling

**5. Complete OpenCypher Grammar from Start**
- **Rejected**: Too much complexity for initial validation
- **Rationale**: Minimal grammar proves architecture, incremental expansion is safer

## Implementation Plan

## Phase 1: Foundation (Week 1)
**Tasks:**
- Set up minimal BISON grammar file for CREATE/MATCH/RETURN
- Create FLEX lexer for basic tokens
- Define core AST node structures
- Implement AST memory management functions
- Update Makefile for BISON/FLEX compilation

**Exit Criteria:** Parser compiles and generates AST for target queries

## Phase 2: Database Integration (Week 1-2)
**Tasks:** 
- Create SQLite extension entry points
- Implement database schema creation
- Build node insertion functions
- Build node query functions
- Add basic error handling

**Exit Criteria:** Can store and retrieve nodes via direct C API calls

## Phase 3: Query Execution (Week 2)
**Tasks:**
- Implement AST walker/interpreter
- Connect parser output to database operations
- Build result formatting system
- Add memory cleanup for full pipeline
- Create main query execution function

**Exit Criteria:** End-to-end query execution working for target queries

## Phase 4: Testing & Validation (Week 2-3)
**Tasks:**
- Create CUnit test suite for each component
- Add integration tests for target queries
- Test memory management (no leaks)
- Validate SQLite extension loading
- Performance baseline measurements

**Exit Criteria:** All tests passing, ready for next iteration

## Testing Strategy

## Unit Testing (CUnit Framework)
**Parser Tests:**
- Valid query parsing produces correct AST
- Invalid queries produce appropriate errors
- Memory cleanup after parse failures

**Database Tests:**
- Node insertion with label and property
- Node retrieval by label only
- Node retrieval by label and property
- Schema creation and table validation

**Execution Tests:**
- CREATE statement execution
- MATCH statement execution  
- RETURN statement formatting
- End-to-end pipeline memory management

## Integration Testing
**Target Query Validation:**
```cypher
-- Test 1: Create and verify storage
CREATE (n:Person {name: "John"})

-- Test 2: Match by label
MATCH (n:Person) RETURN n

-- Test 3: Match by label and property
MATCH (n:Person {name: "John"}) RETURN n
```

**Success Criteria per Test:**
- Query parses without errors
- Database operations complete successfully
- Results contain expected data
- No memory leaks detected
- SQLite extension loads properly

## Performance Baseline
- Parse time: <1ms for target queries
- Execution time: <10ms for single node operations
- Memory usage: <1MB for test dataset
- Zero memory leaks under valgrind

## Validation Environment
- CUnit test runner integrated with make test
- Memory testing with appropriate tools
- SQLite extension loading verification
- Cross-platform testing (Linux/macOS)
