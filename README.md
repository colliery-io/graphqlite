# GraphQLite: OpenCypher SQLite Extension

## Overview

GraphQLite is a SQLite extension that adds OpenCypher graph query support to SQLite. It implements a typed Entity-Attribute-Value (EAV) storage model with native support for CREATE and MATCH operations, enabling graph database functionality within SQLite.

## Features

- **OpenCypher Query Language**: Parse and execute OpenCypher CREATE and MATCH statements
- **Multiple Property Support**: Create nodes with multiple properties in a single statement
- **Typed Property Storage**: Separate tables for different property types (text, integer, real, boolean)
- **Property Key Interning**: Efficient storage through property key deduplication
- **ACID Compliance**: Full SQLite transaction support
- **Comprehensive Testing**: Both unit tests and SQL integration tests

## Architecture

### Schema Design
GraphQLite uses a typed EAV model with the following tables:

- **nodes**: Core node storage
- **node_labels**: Node label associations
- **property_keys**: Property key interning for efficiency
- **node_props_text/int/real/bool**: Typed property storage tables

### Parser Implementation
- **BISON/FLEX Grammar**: Complete OpenCypher parser for CREATE and MATCH statements
- **AST Generation**: Abstract syntax tree for query representation
- **Property Lists**: Support for multiple properties in single statements

### Query Execution
- **CREATE Statements**: Node creation with labels and multiple properties
- **MATCH Statements**: Node retrieval with label and property filtering
- **Type Safety**: Automatic property type inference and storage

## Usage Examples

### Creating Nodes

```sql
-- Load the extension
.load ./build/graphqlite.so

-- Create a simple node
SELECT cypher('CREATE (n:Person)');

-- Create node with single property
SELECT cypher('CREATE (n:Person {name: "Alice"})');

-- Create node with multiple properties
SELECT cypher('CREATE (n:Product {name: "Widget", price: 100, rating: 4.5, inStock: true})');
```

### Querying Nodes

```sql
-- Find all Person nodes
SELECT cypher('MATCH (n:Person) RETURN n');

-- Find nodes with specific property
SELECT cypher('MATCH (n:Person {name: "Alice"}) RETURN n');

-- Find products with specific price
SELECT cypher('MATCH (n:Product {price: 100}) RETURN n');
```

### Supported OpenCypher Syntax

- **CREATE statements**: `CREATE (variable:Label {prop1: value1, prop2: value2, ...})`
- **MATCH statements**: `MATCH (variable:Label {property: value}) RETURN variable`
- **Property types**: String literals, integers, floats, booleans
- **Multiple properties**: Comma-separated property lists in braces

## Database Schema

When the extension is loaded, it automatically creates the following schema:

```sql
-- Core node storage
CREATE TABLE nodes (id INTEGER PRIMARY KEY AUTOINCREMENT);

-- Node labels (many-to-many)
CREATE TABLE node_labels (
    node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
    label TEXT NOT NULL,
    PRIMARY KEY (node_id, label)
);

-- Property key interning
CREATE TABLE property_keys (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    key TEXT UNIQUE NOT NULL
);

-- Typed property tables
CREATE TABLE node_props_text (
    node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
    key_id INTEGER NOT NULL REFERENCES property_keys(id),
    value TEXT NOT NULL,
    PRIMARY KEY (node_id, key_id)
);

CREATE TABLE node_props_int (
    node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
    key_id INTEGER NOT NULL REFERENCES property_keys(id),
    value INTEGER NOT NULL,
    PRIMARY KEY (node_id, key_id)
);

-- Similar tables for node_props_real and node_props_bool
```

## Build and Installation

### Prerequisites
```bash
# macOS with Homebrew
brew install sqlite

# Ubuntu/Debian
sudo apt-get install libsqlite3-dev build-essential

# Build tools for parser generation
# bison and flex (usually included with build-essential)
```

### Compilation
```bash
# Clean build
make clean

# Build extension and tests
make all

# Run all tests (unit tests + SQL integration tests)
make test

# Run only unit tests
make test-unit

# Run only SQL tests
make test-sql
```

### Usage
```bash
# Use the extension in SQLite
sqlite3 mydatabase.db
.load ./build/graphqlite.so

# Test basic functionality
SELECT graphqlite_test();
SELECT cypher('CREATE (n:Person {name: "Alice"})');
```

## Testing

The project includes comprehensive testing:

- **Unit Tests**: 35 tests covering parser and database functionality
- **SQL Integration Tests**: End-to-end tests using actual SQL files
- **Error Handling**: Parse error detection and validation
- **Memory Management**: Leak detection and cleanup verification

## Current Limitations

- **Edge Support**: Currently focused on node operations (edges planned)
- **Query Complexity**: Single property filters in MATCH (multiple filter support planned)
- **Advanced Features**: No graph algorithms or complex traversals yet

## Future Roadmap

- **Edge Operations**: CREATE and MATCH for relationships
- **Complex Queries**: Multi-property filters, WHERE clauses
- **Graph Algorithms**: Shortest path, centrality calculations  
- **Performance Optimization**: Bulk operations and caching improvements