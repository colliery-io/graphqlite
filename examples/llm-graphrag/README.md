# LLM-powered GraphRAG with HotpotQA

A GraphRAG demo using the HotpotQA multi-hop reasoning dataset, demonstrating how graph structure improves context retrieval for complex questions.

Uses:
- **GraphQLite** - Graph storage with Cypher queries and graph algorithms
- **sqlite-vec** - Vector similarity search
- **Ollama** - Local LLM inference (qwen3:8b by default)
- **Sentence Transformers** - Embeddings (all-MiniLM-L6-v2)

## Why GraphRAG?

Traditional RAG uses only vector similarity to find relevant documents. But multi-hop questions require connecting information across documents.

**Example:** "Were Scott Derrickson and Ed Wood of the same nationality?"

- Vector search finds "Ed Wood" but may miss "Scott Derrickson"
- Graph traversal via COOCCURS edges discovers the connection
- Community detection finds topically related articles

## Data Flow

The demo shows a 6-step pipeline:

```
STEP 1: Vector Search
    Find top-k similar articles by embedding distance

STEP 2: Graph Traversal (COOCCURS edges)
    For each seed article, traverse to co-occurring articles via Cypher

STEP 3: Community Detection (Louvain)
    Find other articles in the same topic community

STEP 4: Combined Context
    Merge all retrieved articles (vector + graph + community)

STEP 5: LLM Prompt
    Build context + question for the LLM

STEP 6: LLM Answer
    Generate the final answer
```

## Prerequisites

1. Install and start [Ollama](https://ollama.ai):
   ```bash
   ollama serve
   ```

2. Pull a model:
   ```bash
   ollama pull qwen3:8b
   ```

## Quick Start

```bash
# Install dependencies
uv sync

# Ingest HotpotQA dataset (takes ~10 min for full dataset)
uv run python ingest.py

# Query with full data flow logging
uv run python rag.py "Were Scott Derrickson and Ed Wood of the same nationality?"

# Interactive mode
uv run python rag.py
```

## Ingestion Pipeline

The ingestion process transforms HotpotQA's JSON dataset into a queryable knowledge graph:

```
JSON Dataset → Parse Examples → Collect Articles → Generate Embeddings → Create Graph
```

**Stage 1: Parse & Deduplicate**
Extracts ~7,400 multi-hop questions with their context paragraphs. Deduplicates Wikipedia articles (many questions share articles), reducing to ~66K unique articles.

**Stage 2: Embedding Generation**
Uses Sentence Transformers (all-MiniLM-L6-v2) with batch processing for fast vectorization. Embeddings are normalized for cosine similarity.

**Stage 3: Graph Construction**
Creates the knowledge graph with Article and Question nodes, then establishes relationships:
- `HAS_CONTEXT`: Links questions to their supporting articles
- `COOCCURS`: Links articles that appear together in question contexts (enables multi-hop traversal)

The `COOCCURS` edges are key for GraphRAG - they capture the multi-hop reasoning paths required by HotpotQA questions.

## Commands

### ingest.py - Load HotpotQA

Downloads and ingests the HotpotQA dev distractor set:

```bash
uv run python ingest.py                # Full dataset (~7400 questions, 66K articles)
uv run python ingest.py --limit 500    # Smaller subset for testing
uv run python ingest.py --db custom.db # Custom database path
```

Creates:
- **Article nodes** with text + embeddings
- **Question nodes** with metadata
- **HAS_CONTEXT edges** linking questions to their context articles
- **COOCCURS edges** linking articles that appear together in question contexts

### rag.py - Ask Questions

```bash
uv run python rag.py "Your question here"
uv run python rag.py                      # Interactive mode
uv run python rag.py --no-llm "question"  # Show context only, skip LLM
uv run python rag.py --model llama3.2     # Use different Ollama model
```

### analyze.py - Graph Analytics

```bash
uv run python analyze.py   # Show community detection and graph stats
```

## Graph Schema

```
(:Article {id, title, text})
(:Question {id, text, answer, type, level})

(Question)-[:HAS_CONTEXT {is_supporting}]->(Article)
(Article)-[:COOCCURS {supporting_pair}]-(Article)
```

## Retrieval Methods

### 1. Vector Search (Step 1)
Finds articles with embeddings similar to the question using sqlite-vec.

### 2. Graph Traversal (Step 2)
For each seed article, traverses COOCCURS edges to find related articles:
```cypher
MATCH (a {id: 'article:Ed_Wood'})-[:COOCCURS]-(b:Article)
RETURN b.id, b.title, b.text LIMIT 5
```

### 3. Community Detection (Step 3)
Uses Louvain algorithm to find topic clusters, then retrieves other articles in the same community as seed articles.

## Files

- `ingest.py` - Dataset download and ingestion
- `rag.py` - GraphRAG query interface
- `analyze.py` - Graph analytics and community detection
- `hotpotqa.py` - HotpotQA dataset parser
- `ollama_client.py` - Ollama REST client
