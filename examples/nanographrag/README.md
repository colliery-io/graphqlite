# NanoGraphRAG Example

A minimal GraphRAG (Graph Retrieval-Augmented Generation) backend using GraphQLite, demonstrating:

- **Document Chunking**: Split documents into overlapping chunks for processing
- **Entity Extraction**: Extract entities and relationships using spaCy NER
- **Graph Storage**: Node/edge CRUD with Cypher queries
- **Graph Algorithms**: Community detection (label propagation), PageRank
- **Vector Search**: Local embeddings via sentence-transformers + sqlite-vec
- **Graph-Augmented Retrieval**: Query -> Chunks -> Entities -> Related context

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

1. **Chunks Documents** - Splits markdown files into 512-token chunks with 50-token overlap
2. **Extracts Entities** - Uses spaCy NER to find people, organizations, locations
3. **Builds Knowledge Graph** - Entities as nodes, co-occurrences as edges, chunks linked via MENTIONS
4. **Queries with Cypher** - Pattern matching across the graph
5. **Runs Graph Algorithms** - PageRank for importance, community detection for clustering
6. **GraphRAG Retrieval** - Vector search finds chunks, graph traversal finds related entities

## Files

- `chunking.py` - Text chunking utilities (token-based, overlapping)
- `entity_extractor.py` - spaCy-based entity and relationship extraction
- `graphqlite_graph.py` - Minimal graph storage class (~200 lines)
- `demo.py` - Full GraphRAG demo pipeline

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

## Chunking API

```python
from chunking import chunk_text, chunk_documents, Chunk

# Single document
chunks = chunk_text(
    text="Long document content...",
    chunk_size=512,    # Target tokens per chunk
    overlap=50,        # Token overlap between chunks
    doc_id="doc1",     # Document identifier
)

# Multiple documents
chunks = chunk_documents(
    documents=[
        {"id": "doc1", "text": "First document..."},
        {"id": "doc2", "text": "Second document..."},
    ],
    chunk_size=512,
    overlap=50,
)

# Each chunk has:
# - chunk_id: Unique identifier
# - doc_id: Source document
# - text: Chunk content
# - start_char, end_char: Character positions
# - chunk_index: Position in document
# - token_count: Approximate token count
```

## GraphRAG Retrieval Flow

The demo shows the complete GraphRAG pipeline:

```
Query: "Who are the tech industry leaders?"
         │
         ▼
┌─────────────────────┐
│  1. Vector Search   │  Find chunks similar to query
│     (sqlite-vec)    │
└─────────┬───────────┘
         │
         ▼
┌─────────────────────┐
│  2. Graph Lookup    │  MATCH (chunk)-[:MENTIONS]->(entity)
│     (Cypher)        │
└─────────┬───────────┘
         │
         ▼
┌─────────────────────┐
│  3. Graph Traversal │  MATCH (entity)-[r]-(related)
│     (1-hop expand)  │
└─────────┬───────────┘
         │
         ▼
    Context for LLM
```

## Adapting for GraphRAG

This storage backend can serve as a drop-in replacement for NetworkX or Neo4j in GraphRAG pipelines like [nano-graphrag](https://github.com/gusye1234/nano-graphrag). Key advantages:

- **Single file database** - No server to manage
- **SQL + Cypher** - Use SQL for metadata, Cypher for graph patterns
- **Embedded** - Runs in-process, no network overhead
- **Integrated vector search** - sqlite-vec in same database as graph
