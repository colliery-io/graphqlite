# GraphQLite: High-Performance Native Graph Database for SQLite

## Overview

GraphQLite is a native graph database extension for SQLite that implements high-performance graph operations through a typed Entity-Attribute-Value (EAV) storage architecture. It provides both interactive ACID-compliant operations and high-throughput bulk import capabilities optimized for different workloads.

## Core Architecture

### Typed EAV Storage Model
- **Separate tables per property type** (integers, text, reals, booleans)
- **Property key interning** for 40%+ space savings through string deduplication
- **Entity-agnostic design** supporting both nodes and edges with properties

### Dual-Mode Operation
- **Interactive Mode**: Full ACID compliance, real-time operations, WAL journaling
- **Bulk Import Mode**: Optimized for throughput (50,000+ nodes/sec), deferred constraints

### Memory-Efficient Design
- **LRU caching** for property keys and prepared statements
- **Configurable memory limits** with automatic batch flushing
- **Thread-safe operations** with fine-grained locking

## Core Components

### Database Lifecycle (`database.c`)
- SQLite database connection management
- Schema creation and validation
- Component initialization and cleanup
- Connection pooling support

### Property Key Interning (`property_keys.c`)
- Hash-based cache for property key lookup
- Automatic string deduplication
- LRU eviction with usage statistics
- Thread-safe key registration

### Prepared Statement Management (`statements.c`)
- Fixed statements for common operations
- Dynamic statement caching with TTL
- Performance statistics and optimization
- Automatic statement cleanup

### Transaction Management (`transactions.c`)
- ACID-compliant transaction support
- Nested transaction handling with savepoints
- Automatic transaction management in bulk mode
- Deadlock detection and recovery

### Node Operations (`nodes.c`)
- Node creation, deletion, and existence checking
- Label management (add, remove, query)
- Batch node creation for performance
- Node degree calculations

### Edge Operations (`edges.c`)
- Edge creation with referential integrity
- Bidirectional edge queries (incoming/outgoing)
- Neighbor traversal operations
- Edge type management and statistics

### Property Management (`properties.c`)
- Unified property API across all entity types
- Type inference and validation
- Batch property operations
- Property set management utilities

### Mode Management (`modes.c`)
- Safe mode transitions with state validation
- Interactive vs bulk import optimization
- Readonly and maintenance modes
- Configuration management per mode

## API Reference

### Basic Operations

```c
// Database lifecycle
graphqlite_db_t* graphqlite_open(const char *path);
int graphqlite_close(graphqlite_db_t *db);

// Node operations
int64_t graphqlite_create_node(graphqlite_db_t *db);
int graphqlite_delete_node(graphqlite_db_t *db, int64_t node_id);
bool graphqlite_node_exists(graphqlite_db_t *db, int64_t node_id);

// Edge operations
int64_t graphqlite_create_edge(graphqlite_db_t *db, int64_t source_id, 
                              int64_t target_id, const char *type);
int64_t* graphqlite_get_outgoing_edges(graphqlite_db_t *db, int64_t node_id, 
                                      const char *type, size_t *count);

// Property operations
int graphqlite_set_property(graphqlite_db_t *db, entity_type_t entity_type,
                           int64_t entity_id, const char *key, property_value_t *value);
int graphqlite_get_property(graphqlite_db_t *db, entity_type_t entity_type,
                           int64_t entity_id, const char *key, property_value_t *value);
```

### Batch Operations

```c
// Batch node creation
int graphqlite_create_nodes_batch(graphqlite_db_t *db, size_t count, int64_t *result_ids);

// Bulk import with labels and properties
int graphqlite_bulk_create_nodes(graphqlite_db_t *db, size_t count,
                                const char **label_arrays[], size_t label_counts[],
                                property_set_t *property_sets[], int64_t *result_ids);
```

### Mode Management

```c
// Switch operation modes
int graphqlite_switch_to_interactive_mode(graphqlite_db_t *db);
int graphqlite_switch_to_bulk_import_mode(graphqlite_db_t *db);
int graphqlite_switch_to_readonly_mode(graphqlite_db_t *db);
graphqlite_mode_t graphqlite_get_current_mode(graphqlite_db_t *db);
```

## Operation Modes

### Interactive Mode (Default)
- **ACID Compliance**: Full transaction support with WAL journaling
- **Real-time Operations**: Immediate consistency for concurrent access
- **Optimized for**: Small to medium graphs, transactional workloads
- **Performance**: ~1,000-5,000 operations/sec depending on complexity

### Bulk Import Mode
- **High Throughput**: Optimized for maximum insertion speed
- **Deferred Constraints**: Foreign key validation postponed until completion
- **Large Transactions**: Batches of 10,000+ operations per transaction
- **Optimized for**: Large graph imports, data migration
- **Performance**: 50,000+ nodes/sec, 25,000+ edges/sec

### Readonly Mode
- **Query Optimization**: Large cache sizes for read performance
- **No Write Operations**: Database protection against modifications
- **Optimized for**: Analytics, reporting, graph traversals

### Maintenance Mode
- **Data Integrity**: Full synchronous mode with integrity checking
- **Safe Operations**: Optimized for schema changes and repairs
- **Optimized for**: Database maintenance, integrity verification

## Performance Characteristics

### Memory Usage
- **Property Key Cache**: ~1MB for 100,000 unique property names
- **Statement Cache**: ~10MB for typical workloads
- **Bulk Import Buffer**: Configurable (default 500MB)

### Disk Storage
- **Space Efficiency**: 40% reduction through property key interning
- **Indexed Access**: B-tree indexes on all entity and property tables
- **Compression**: SQLite page-level compression available

### Throughput Benchmarks
- **Interactive Mode**: 5,000 nodes/sec, 2,500 edges/sec
- **Bulk Import Mode**: 50,000+ nodes/sec, 25,000+ edges/sec
- **Property Operations**: 10,000+ property sets/sec
- **Graph Traversal**: 100,000+ neighbor queries/sec

## Build and Installation

### Prerequisites
```bash
# SQLite 3.38+ with development headers
sudo apt-get install libsqlite3-dev

# Build tools
sudo apt-get install build-essential
```

### Compilation
```bash
make clean
make all

# Run tests
make test
```

### Integration
```c
#include "graphqlite.h"

// Link with: -lgraphqlite -lsqlite3 -lpthread
```

## Thread Safety

GraphQLite is designed for concurrent access:

- **Database Handle**: Thread-safe for read operations
- **Write Operations**: Serialized through SQLite's built-in locking
- **Mode Transitions**: Protected by mutex locks
- **Property Cache**: Thread-safe with fine-grained locking

## Error Handling

All functions return appropriate SQLite error codes:
- `SQLITE_OK` (0): Success
- `SQLITE_ERROR`: General error
- `SQLITE_BUSY`: Resource temporarily unavailable
- `SQLITE_NOMEM`: Out of memory

Use `sqlite3_errmsg()` on the underlying SQLite handle for detailed error messages.

## Future Roadmap

- **Query Language**: Native graph query language support
- **Distributed Storage**: Multi-node cluster support
- **Advanced Algorithms**: Built-in graph algorithms (shortest path, centrality)
- **Streaming**: Real-time graph stream processing
- **Bindings**: Python, Rust, and JavaScript language bindings