---
title: Building a GraphRAG System
slug: 0.9.4/tutorials/graphrag
---

This tutorial builds a complete Graph Retrieval-Augmented Generation (GraphRAG) system by combining GraphQLite with [sqlite-vec](https://github.com/asg017/sqlite-vec) for vector search and [sentence-transformers](https://www.sbert.net/) for text embeddings.

GraphRAG enriches standard vector retrieval with graph traversal — finding not just semantically similar passages, but also the entities they mention and the graph neighbourhood around those entities. The result is richer, more contextual input for language models.

> **Note**: This tutorial requires additional dependencies beyond GraphQLite itself. The complete working example with HotpotQA multi-hop reasoning is in `examples/llm-graphrag/`.

## What is GraphRAG?

Traditional RAG:

1. Embed the query
2. Find the most similar document chunks by vector distance
3. Send those chunks to the LLM

GraphRAG adds:
4\. Extract entities mentioned in the retrieved chunks
5\. Traverse the knowledge graph to find related entities
6\. Include the graph neighbourhood as additional context

This matters when answers require connecting information across multiple documents — "Who co-authored papers with the researcher who invented Word2Vec?" requires knowing both the entity graph (researchers, papers) and the text content of specific chunks.

```
User query: "What work influenced the Transformer architecture?"
                    │
                    ▼
         ┌──────────────────┐
         │  Vector Search   │  Find chunks similar to query
         └────────┬─────────┘
                  │  chunk_ids: ["doc1_chunk_3", "doc2_chunk_1"]
                  ▼
         ┌──────────────────┐
         │  Entity Lookup   │  MATCH (chunk)-[:MENTIONS]->(entity)
         └────────┬─────────┘
                  │  entities: ["Word2Vec", "RNN", "attention mechanism"]
                  ▼
         ┌──────────────────┐
         │  Graph Expand    │  MATCH (entity)-[*1..2]-(related)
         └────────┬─────────┘
                  │  related: ["LSTM", "Bahdanau", "Seq2Seq", ...]
                  ▼
             LLM context
```

## Prerequisites

```bash
pip install graphqlite sentence-transformers sqlite-vec spacy
python -m spacy download en_core_web_sm
```

Verify:

```python
import graphqlite, sqlite_vec, spacy
from sentence_transformers import SentenceTransformer
print("All dependencies available")
```

## Step 1: Document Ingestion Helpers

Define the data structures and chunking logic:

```python
from __future__ import annotations
from dataclasses import dataclass
from typing import List

@dataclass
class Chunk:
    chunk_id: str
    doc_id:   str
    text:     str

def chunk_text(text: str, doc_id: str, chunk_size: int = 200, overlap: int = 40) -> List[Chunk]:
    """Split text into overlapping word-based chunks."""
    words = text.split()
    chunks = []
    start = 0
    index = 0
    while start < len(words):
        end = min(start + chunk_size, len(words))
        chunk_words = words[start:end]
        chunks.append(Chunk(
            chunk_id=f"{doc_id}_chunk_{index}",
            doc_id=doc_id,
            text=" ".join(chunk_words),
        ))
        start += chunk_size - overlap
        index += 1
    return chunks
```

## Step 2: Entity Extraction

Use spaCy to extract named entities and create co-occurrence relationships:

```python
import spacy

nlp = spacy.load("en_core_web_sm")

def extract_entities(text: str) -> List[dict]:
    """Return named entities with their type."""
    doc = nlp(text)
    seen = set()
    entities = []
    for ent in doc.ents:
        key = (ent.text.strip(), ent.label_)
        if key not in seen:
            seen.add(key)
            entities.append({"text": ent.text.strip(), "label": ent.label_})
    return entities

def entity_node_id(name: str) -> str:
    """Normalise entity name to a stable node ID."""
    return "ent_" + name.lower().replace(" ", "_").replace(".", "").replace(",", "")
```

## Step 3: Build the Knowledge Graph

```python
import sqlite3
import sqlite_vec
from sentence_transformers import SentenceTransformer
from graphqlite import Graph

EMBEDDING_DIM = 384
model = SentenceTransformer("all-MiniLM-L6-v2")

def setup(db_path: str):
    """Initialise GraphQLite and the vector table."""
    g = Graph(db_path)
    conn = sqlite3.connect(db_path)
    sqlite_vec.load(conn)

    conn.execute(f"""
        CREATE VIRTUAL TABLE IF NOT EXISTS chunk_embeddings USING vec0(
            chunk_id  TEXT PRIMARY KEY,
            embedding FLOAT[{EMBEDDING_DIM}]
        )
    """)
    conn.commit()
    return g, conn


def ingest_document(g: Graph, conn: sqlite3.Connection, doc_id: str, text: str):
    """
    Chunk a document, extract entities, store graph nodes and edges,
    and compute embeddings.
    """
    chunks = chunk_text(text, doc_id=doc_id)

    # Embed all chunks in one batch
    texts = [c.text for c in chunks]
    embeddings = model.encode(texts, show_progress_bar=False)

    for chunk, embedding in zip(chunks, embeddings):
        # Store chunk as a graph node (truncated text for storage efficiency)
        g.upsert_node(
            chunk.chunk_id,
            {"doc_id": doc_id, "text": chunk.text[:1000]},
            label="Chunk"
        )

        # Store embedding
        conn.execute(
            "INSERT OR REPLACE INTO chunk_embeddings (chunk_id, embedding) VALUES (?, ?)",
            [chunk.chunk_id, embedding.tobytes()]
        )

        # Extract entities from this chunk
        entities = extract_entities(chunk.text)
        for ent in entities:
            ent_id = entity_node_id(ent["text"])
            g.upsert_node(ent_id, {"name": ent["text"], "type": ent["label"]}, label="Entity")
            g.upsert_edge(chunk.chunk_id, ent_id, {}, rel_type="MENTIONS")

        # Entity co-occurrence: connect entities that appear in the same chunk
        for i, ent_a in enumerate(entities):
            for ent_b in entities[i + 1:]:
                id_a = entity_node_id(ent_a["text"])
                id_b = entity_node_id(ent_b["text"])
                g.upsert_edge(id_a, id_b, {"source_chunk": chunk.chunk_id}, rel_type="CO_OCCURS")

    conn.commit()
    print(f"Ingested {doc_id}: {len(chunks)} chunks, graph now has {g.stats()['nodes']} nodes")
```

## Step 4: Vector Search

```python
def vector_search(conn: sqlite3.Connection, query: str, k: int = 5) -> List[str]:
    """Return the k most semantically similar chunk IDs."""
    query_embedding = model.encode([query])[0]
    rows = conn.execute(
        """
        SELECT chunk_id
        FROM chunk_embeddings
        WHERE embedding MATCH ?
          AND k = ?
        """,
        [query_embedding.tobytes(), k]
    ).fetchall()
    return [row[0] for row in rows]
```

## Step 5: GraphRAG Retrieval

```python
def graphrag_retrieve(
    g: Graph,
    conn: sqlite3.Connection,
    query: str,
    k_chunks: int = 5,
    expand_hops: int = 2,
) -> dict:
    """
    Hybrid retrieval:
    1. Vector search for semantically similar chunks
    2. Find entities mentioned in those chunks (graph lookup)
    3. Expand to related entities via graph traversal
    4. Return chunks + entity context
    """

    # 1. Vector search
    chunk_ids = vector_search(conn, query, k=k_chunks)
    if not chunk_ids:
        return {"chunks": [], "entities": [], "related_entities": [], "graph_paths": []}

    # 2. Entity lookup via graph — parameterized query
    entities: set[str] = set()
    for chunk_id in chunk_ids:
        rows = g.connection.cypher(
            "MATCH (c:Chunk {id: $chunk_id})-[:MENTIONS]->(e:Entity) RETURN e.name AS name",
            {"chunk_id": chunk_id}
        )
        for row in rows:
            entities.add(row["name"])

    # 3. Graph expansion — find related entities
    related_entities: set[str] = set()
    for entity_name in entities:
        ent_id = entity_node_id(entity_name)
        rows = g.connection.cypher(
            """
            MATCH (e:Entity {id: $ent_id})-[*1..$hops]-(related:Entity)
            WHERE related.id <> $ent_id
            RETURN DISTINCT related.name AS name
            """,
            {"ent_id": ent_id, "hops": expand_hops}
        )
        for row in rows:
            related_entities.add(row["name"])

    # 4. Retrieve chunk texts
    chunk_texts = []
    for chunk_id in chunk_ids:
        node = g.get_node(chunk_id)
        if node:
            chunk_texts.append({
                "chunk_id": chunk_id,
                "doc_id":   node["properties"].get("doc_id", ""),
                "text":     node["properties"].get("text", ""),
            })

    return {
        "chunks":           chunk_texts,
        "entities":         sorted(entities),
        "related_entities": sorted(related_entities - entities),
    }
```

## Step 6: Graph Algorithms for Retrieval Enhancement

Graph algorithms improve retrieval quality in several ways.

### PageRank for entity importance

Entities that are heavily co-mentioned across documents are likely central to the corpus topics. Use PageRank to weight entity importance during retrieval.

```python
def get_important_entities(g: Graph, top_k: int = 20) -> List[str]:
    """Return top-k entity node IDs by PageRank."""
    g.connection.cypher("RETURN gql_load_graph()")
    results = g.pagerank(damping=0.85, iterations=20)

    important = []
    for r in sorted(results, key=lambda x: x["score"], reverse=True):
        node = g.get_node(r["user_id"])
        if node and node["label"] == "Entity":
            important.append(r["user_id"])
            if len(important) >= top_k:
                break
    return important


def graphrag_retrieve_ranked(
    g: Graph,
    conn: sqlite3.Connection,
    query: str,
    k_chunks: int = 5,
) -> dict:
    """Retrieval that boosts chunks mentioning high-PageRank entities."""
    base = graphrag_retrieve(g, conn, query, k_chunks=k_chunks)
    important_entities = set(get_important_entities(g, top_k=20))

    # Score chunks by how many important entities they mention
    scored_chunks = []
    for chunk in base["chunks"]:
        rows = g.connection.cypher(
            "MATCH (c:Chunk {id: $chunk_id})-[:MENTIONS]->(e:Entity) RETURN e.id AS eid",
            {"chunk_id": chunk["chunk_id"]}
        )
        ent_ids = {r["eid"] for r in rows}
        boost = len(ent_ids & important_entities)
        scored_chunks.append({**chunk, "importance_boost": boost})

    scored_chunks.sort(key=lambda x: x["importance_boost"], reverse=True)
    base["chunks"] = scored_chunks
    return base
```

### Community detection for topic-aware retrieval

Detect research clusters to route queries to the right part of the graph:

```python
def get_entity_communities(g: Graph) -> dict[str, int]:
    """Return a mapping of entity node_id -> community ID."""
    g.connection.cypher("RETURN gql_load_graph()")
    results = g.community_detection(iterations=10)
    return {r["user_id"]: r["community"] for r in results}


def retrieve_by_community(g: Graph, entity_name: str) -> List[str]:
    """Find all entity names in the same community as a given entity."""
    communities = get_entity_communities(g)
    ent_id = entity_node_id(entity_name)
    if ent_id not in communities:
        return []
    target_community = communities[ent_id]
    return [
        uid for uid, cid in communities.items()
        if cid == target_community and uid != ent_id
    ]
```

## Step 7: Complete Pipeline

```python
# ---- Initialise ----
g, conn = setup("graphrag.db")

# ---- Ingest Documents ----
documents = [
    {
        "id": "vaswani2017",
        "text": (
            "Attention Is All You Need. Vaswani et al., Google Brain, 2017. "
            "We propose the Transformer, a model architecture eschewing recurrence "
            "and instead relying entirely on an attention mechanism to draw global "
            "dependencies between input and output. The Transformer allows for "
            "significantly more parallelization than recurrent architectures. "
            "Our model is trained on the WMT 2014 English-German and English-French "
            "translation tasks and achieves state-of-the-art results."
        )
    },
    {
        "id": "mikolov2013",
        "text": (
            "Distributed Representations of Words and Phrases and their Compositionality. "
            "Mikolov et al., Google, 2013. "
            "We present several extensions of the original Word2Vec model that improve "
            "quality of the vectors and training speed. We show that subsampling of "
            "frequent words during training results in significant speedup."
        )
    },
    {
        "id": "devlin2018",
        "text": (
            "BERT: Pre-training of Deep Bidirectional Transformers for Language Understanding. "
            "Devlin, Chang, Lee, Toutanova, Google AI Language, 2018. "
            "We introduce BERT, a new language representation model. BERT stands for "
            "Bidirectional Encoder Representations from Transformers. BERT is designed "
            "to pre-train deep bidirectional representations from unlabeled text by "
            "jointly conditioning on both left and right context in all layers."
        )
    },
]

for doc in documents:
    ingest_document(g, conn, doc["id"], doc["text"])

print(f"\nFinal graph: {g.stats()}")

# ---- Retrieve ----
query = "What mechanisms replaced recurrence in language models?"
context = graphrag_retrieve(g, conn, query, k_chunks=3, expand_hops=2)

print(f"\nQuery: {query}")
print(f"Retrieved {len(context['chunks'])} chunks")
print(f"Direct entities: {context['entities']}")
print(f"Related entities: {context['related_entities']}")

print("\nChunk texts:")
for chunk in context["chunks"]:
    print(f"  [{chunk['doc_id']}] {chunk['text'][:120]}...")

# ---- Build prompt ----
def build_prompt(query: str, context: dict) -> str:
    chunk_text = "\n\n".join(
        f"[{c['doc_id']}] {c['text']}" for c in context["chunks"]
    )
    entity_context = ", ".join(context["entities"])
    related_context = ", ".join(context["related_entities"])

    return f"""Answer the following question using only the provided context.

Question: {query}

Relevant text passages:
{chunk_text}

Key entities mentioned: {entity_context}
Related entities from knowledge graph: {related_context}

Answer:"""

prompt = build_prompt(query, context)
print("\n--- Prompt ---")
print(prompt[:600], "...")
```

## Step 8: Working with the Example Project

The `examples/llm-graphrag/` directory contains a complete production-grade implementation:

* Ingests the HotpotQA multi-hop reasoning dataset
* Uses Ollama for local LLM inference (no API keys required)
* Demonstrates multi-hop question answering where the answer requires connecting information across multiple documents

```bash
cd examples/llm-graphrag

# Install dependencies (uses uv)
uv sync

# Ingest the HotpotQA dataset
uv run python ingest.py

# Interactive query mode
uv run python rag.py
```

The ingest script builds a graph of ~10,000 Wikipedia article chunks with entity co-occurrence relationships. The rag script accepts questions and returns answers with citations.

For a deeper discussion of when to use GraphRAG vs plain RAG, graph schema design trade-offs, and embedding model selection, see the [Architecture explanation](../../explanation/architecture/).

## Next Steps

* [Graph Analytics](../graph-analytics/) — Deep dive into PageRank, community detection, and other algorithms used here
* [Graph Algorithms Reference](../../reference/algorithms/) — Full algorithm parameter documentation
* [Python API Reference](../../reference/python-api/) — `Graph`, `Connection`, and `GraphManager` API
* [Use with Other Extensions](../../how-to/other-extensions/) — Loading sqlite-vec alongside GraphQLite
