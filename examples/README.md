
# GraphQLite Examples

Tutorial examples for learning GraphQLite, organized by language.

## SQL Examples

Core tutorials using SQLite's CLI:

```bash
# Build the extension first
make extension

# Run any example
sqlite3 < examples/sql/01_getting_started.sql
```

| File | Description |
|------|-------------|
| `01_getting_started.sql` | Basic Cypher queries, creating nodes and relationships |
| `02_building_graphs.sql` | Using raw SQL INSERT for bulk graph loading |
| `03_querying_patterns.sql` | MATCH patterns, traversals, variable-length paths |
| `04_aggregations.sql` | count, min, max, collect, grouping |
| `05_graph_algorithms.sql` | PageRank and community detection |
| `06_sql_integration.sql` | Using algorithm results in SQL queries with json_each() |

## Python Examples

Using the GraphQLite Python bindings:

```bash
pip install graphqlite
python examples/python/knowledge_graph.py
```

| File | Description |
|------|-------------|
| `knowledge_graph.py` | Build and analyze a research publication network |

## Rust Examples

Using the GraphQLite Rust crate:

```bash
cd graphqlite-rs
cargo run --example knowledge_graph
```

| File | Description |
|------|-------------|
| `knowledge_graph.rs` | Build and analyze a research publication network |

## Knowledge Graph Tutorial

The Python and Rust examples demonstrate a complete knowledge graph workflow:

- **Dataset**: Research publications with Researchers, Articles, and Topics
- **Relationships**: `PUBLISHED` (researcher → article), `IN_TOPIC` (article → topic)
- **Queries covered**:
  - Counting and aggregating nodes
  - Finding co-author relationships
  - Analyzing collaboration networks
  - Multi-hop path queries
  - Cross-topic analysis
