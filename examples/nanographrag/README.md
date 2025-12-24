# NanoGraphRAG Example

A minimal knowledge graph backend using GraphQLite, demonstrating:

- **Graph Storage**: Node/edge CRUD with Cypher queries
- **Graph Algorithms**: Community detection (label propagation), PageRank
- **Semantic Search**: Local embeddings via sentence-transformers + sqlite-vec

## Quick Start

```bash
# From the graphqlite root directory
cd examples/nanographrag

# Build the extension first (from repo root)
make extension RELEASE=1

# Install dependencies and run
uv sync
uv run python demo.py
```

The extension is built against vendored SQLite 3.47 headers, ensuring compatibility
with most Python installations.

## What It Does

1. **Builds a Knowledge Graph** - Creates entities (people, orgs, projects, technologies) and relationships
2. **Queries with Cypher** - Demonstrates pattern matching queries
3. **Community Detection** - Finds clusters of related entities
4. **PageRank** - Identifies the most important/connected entities
5. **Semantic Search** - Embeds entities locally and finds similar ones by meaning

## Files

- `graphqlite_graph.py` - Minimal graph storage class (~200 lines)
- `demo.py` - Full demo with sample data

## Using in Your Project

```python
from graphqlite_graph import GraphQLiteGraph

# Create a persistent graph
graph = GraphQLiteGraph(db_path="my_knowledge.db")

# Add entities
graph.upsert_node("alice", {"name": "Alice", "role": "Engineer"}, label="Person")
graph.upsert_node("project_x", {"name": "Project X"}, label="Project")

# Add relationships
graph.upsert_edge("alice", "project_x", {"since": "2024"}, rel_type="WORKS_ON")

# Query
results = graph.query(
    "MATCH (p:Person)-[:WORKS_ON]->(proj:Project) "
    "RETURN p.name, proj.name"
)

# Graph algorithms
communities = graph.community_detection()
pagerank = graph.pagerank()

graph.close()
```

## Embeddings

The demo uses `all-MiniLM-L6-v2` (~80MB, runs on CPU) for local semantic search.
Embeddings are stored in SQLite via sqlite-vec for efficient similarity queries.

## Adapting for GraphRAG

This storage backend can serve as a drop-in replacement for NetworkX or Neo4j in GraphRAG pipelines like [nano-graphrag](https://github.com/gusye1234/nano-graphrag). Key advantages:

- **Single file database** - No server to manage
- **SQL + Cypher** - Use SQL for metadata, Cypher for graph patterns
- **Embedded** - Runs in-process, no network overhead
