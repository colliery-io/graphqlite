# Building a GraphRAG System

This tutorial shows how to build a Graph Retrieval-Augmented Generation (GraphRAG) system using GraphQLite.

## What is GraphRAG?

GraphRAG combines:
1. **Document chunking** - Split documents into processable pieces
2. **Entity extraction** - Identify entities and relationships
3. **Graph storage** - Store entities as nodes, relationships as edges
4. **Vector search** - Find relevant chunks by semantic similarity
5. **Graph traversal** - Expand context using graph structure

## Prerequisites

```bash
pip install graphqlite sentence-transformers sqlite-vec spacy
python -m spacy download en_core_web_sm
```

## Architecture

```
Query: "Who are the tech leaders?"
         │
         ▼
┌─────────────────────┐
│  1. Vector Search   │  Find chunks similar to query
└─────────┬───────────┘
         │
         ▼
┌─────────────────────┐
│  2. Graph Lookup    │  MATCH (chunk)-[:MENTIONS]->(entity)
└─────────┬───────────┘
         │
         ▼
┌─────────────────────┐
│  3. Graph Traversal │  MATCH (entity)-[*1..2]-(related)
└─────────┬───────────┘
         │
         ▼
    Context for LLM
```

## Step 1: Document Chunking

```python
from dataclasses import dataclass
from typing import List

@dataclass
class Chunk:
    chunk_id: str
    doc_id: str
    text: str
    start_char: int
    end_char: int

def chunk_text(text: str, chunk_size: int = 512, overlap: int = 50, doc_id: str = "doc") -> List[Chunk]:
    """Split text into overlapping chunks."""
    words = text.split()
    chunks = []
    start = 0
    chunk_index = 0

    while start < len(words):
        end = min(start + chunk_size, len(words))
        chunk_words = words[start:end]
        chunk_text = " ".join(chunk_words)

        # Calculate character positions
        start_char = len(" ".join(words[:start])) + (1 if start > 0 else 0)
        end_char = start_char + len(chunk_text)

        chunks.append(Chunk(
            chunk_id=f"{doc_id}_chunk_{chunk_index}",
            doc_id=doc_id,
            text=chunk_text,
            start_char=start_char,
            end_char=end_char,
        ))

        start += chunk_size - overlap
        chunk_index += 1

    return chunks
```

## Step 2: Entity Extraction

```python
import spacy

nlp = spacy.load("en_core_web_sm")

def extract_entities(text: str) -> List[dict]:
    """Extract named entities from text."""
    doc = nlp(text)
    entities = []

    for ent in doc.ents:
        entities.append({
            "text": ent.text,
            "label": ent.label_,
            "start": ent.start_char,
            "end": ent.end_char,
        })

    return entities

def extract_relationships(entities: List[dict]) -> List[tuple]:
    """Create co-occurrence relationships between entities."""
    relationships = []

    for i, e1 in enumerate(entities):
        for e2 in entities[i+1:]:
            relationships.append((
                e1["text"],
                e2["text"],
                "CO_OCCURS",
            ))

    return relationships
```

## Step 3: Build the Knowledge Graph

```python
from graphqlite import Graph

g = Graph("knowledge.db")

def ingest_document(doc_id: str, text: str):
    """Process a document and add to knowledge graph."""

    # Chunk the document
    chunks = chunk_text(text, doc_id=doc_id)

    for chunk in chunks:
        # Store chunk as node
        g.upsert_node(
            chunk.chunk_id,
            {"text": chunk.text[:500], "doc_id": doc_id},  # Truncate for storage
            label="Chunk"
        )

        # Extract and store entities
        entities = extract_entities(chunk.text)

        for entity in entities:
            entity_id = entity["text"].lower().replace(" ", "_")

            # Create entity node
            g.upsert_node(
                entity_id,
                {"name": entity["text"], "type": entity["label"]},
                label="Entity"
            )

            # Link chunk to entity
            g.upsert_edge(
                chunk.chunk_id,
                entity_id,
                {},
                rel_type="MENTIONS"
            )

        # Create entity co-occurrence edges
        relationships = extract_relationships(entities)
        for source, target, rel_type in relationships:
            source_id = source.lower().replace(" ", "_")
            target_id = target.lower().replace(" ", "_")
            g.upsert_edge(source_id, target_id, {}, rel_type=rel_type)
```

## Step 4: Add Vector Search

```python
import sqlite3
import sqlite_vec
from sentence_transformers import SentenceTransformer

# Initialize embedding model
model = SentenceTransformer("all-MiniLM-L6-v2")

def setup_vector_search(conn: sqlite3.Connection):
    """Set up vector search table."""
    sqlite_vec.load(conn)
    conn.execute("""
        CREATE VIRTUAL TABLE IF NOT EXISTS chunk_embeddings USING vec0(
            chunk_id TEXT PRIMARY KEY,
            embedding FLOAT[384]
        )
    """)

def embed_chunks(conn: sqlite3.Connection, chunks: List[Chunk]):
    """Embed chunks and store vectors."""
    texts = [c.text for c in chunks]
    embeddings = model.encode(texts)

    for chunk, embedding in zip(chunks, embeddings):
        conn.execute(
            "INSERT OR REPLACE INTO chunk_embeddings (chunk_id, embedding) VALUES (?, ?)",
            [chunk.chunk_id, embedding.tobytes()]
        )
    conn.commit()

def vector_search(conn: sqlite3.Connection, query: str, k: int = 5) -> List[str]:
    """Find chunks similar to query."""
    query_embedding = model.encode([query])[0]

    results = conn.execute("""
        SELECT chunk_id
        FROM chunk_embeddings
        WHERE embedding MATCH ?
        LIMIT ?
    """, [query_embedding.tobytes(), k]).fetchall()

    return [r[0] for r in results]
```

## Step 5: GraphRAG Retrieval

```python
def graphrag_retrieve(query: str, k_chunks: int = 5, expand_hops: int = 1) -> dict:
    """
    Retrieve context using GraphRAG:
    1. Vector search for relevant chunks
    2. Find entities mentioned in those chunks
    3. Expand to related entities via graph
    """

    # Get underlying connection for vector search
    conn = g.connection.sqlite_connection

    # Step 1: Vector search
    chunk_ids = vector_search(conn, query, k=k_chunks)

    # Step 2: Get entities from chunks
    entities = set()
    for chunk_id in chunk_ids:
        results = g.query(f"""
            MATCH (c:Chunk {{id: '{chunk_id}'}})-[:MENTIONS]->(e:Entity)
            RETURN e.name
        """)
        for r in results:
            entities.add(r["e.name"])

    # Step 3: Expand via graph
    related_entities = set()
    for entity in entities:
        entity_id = entity.lower().replace(" ", "_")
        results = g.query(f"""
            MATCH (e:Entity {{name: '{entity}'}})-[*1..{expand_hops}]-(related:Entity)
            RETURN DISTINCT related.name
        """)
        for r in results:
            related_entities.add(r["related.name"])

    # Get chunk texts
    chunk_texts = []
    for chunk_id in chunk_ids:
        node = g.get_node(chunk_id)
        if node:
            chunk_texts.append(node["properties"].get("text", ""))

    return {
        "chunks": chunk_texts,
        "entities": list(entities),
        "related_entities": list(related_entities - entities),
    }
```

## Step 6: Complete Pipeline

```python
# Initialize
g = Graph("graphrag.db")
conn = sqlite3.connect("graphrag.db")
setup_vector_search(conn)

# Ingest documents
documents = [
    {"id": "doc1", "text": "Apple Inc. was founded by Steve Jobs..."},
    {"id": "doc2", "text": "Microsoft, led by Satya Nadella..."},
]

for doc in documents:
    ingest_document(doc["id"], doc["text"])
    chunks = chunk_text(doc["text"], doc_id=doc["id"])
    embed_chunks(conn, chunks)

# Query
context = graphrag_retrieve("Who are the tech industry leaders?")
print("Relevant chunks:", len(context["chunks"]))
print("Entities:", context["entities"])
print("Related:", context["related_entities"])

# Use context with an LLM
# response = llm.generate(query, context=context)
```

## Graph Algorithms for GraphRAG

Use graph algorithms to enhance retrieval:

```python
# Find important entities
important = g.pagerank()
top_entities = sorted(important, key=lambda x: x["score"], reverse=True)[:10]

# Find entity communities
communities = g.community_detection()

# Find central entities (good for summarization)
central = g.query("RETURN betweennessCentrality()")
```

## Example Project

See `examples/llm-graphrag/` for a complete GraphRAG implementation using the HotpotQA multi-hop reasoning dataset:
- Graph-based knowledge storage with Cypher queries
- sqlite-vec for vector similarity search
- Ollama integration for local LLM inference
- Community detection for topic-based retrieval

```bash
cd examples/llm-graphrag
uv sync
uv run python ingest.py      # Ingest HotpotQA dataset
uv run python rag.py          # Interactive query mode
```

## Next Steps

- [Graph Algorithms](../reference/algorithms.md) - All available algorithms
- [Python API](../reference/python-api.md) - Complete API reference
