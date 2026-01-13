---
id: graph-neural-network-integration
level: initiative
title: "Graph Neural Network Integration for Knowledge Graphs"
short_code: "GQLITE-I-0029"
created_at: 2026-01-13T02:40:54.710885+00:00
updated_at: 2026-01-13T02:40:54.710885+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/discovery"


exit_criteria_met: false
estimated_complexity: XL
strategy_id: NULL
initiative_id: graph-neural-network-integration
---

# Graph Neural Network Integration for Knowledge Graphs Initiative

## Context

GraphQLite currently provides traditional graph algorithms (PageRank, community detection, shortest paths, etc.) that use fixed, hand-coded rules. Graph Neural Networks (GNNs) represent the next evolution—*learned* graph algorithms that discover patterns from data rather than requiring explicit programming.

Knowledge graphs are particularly well-suited for GNNs because they have:
- **Typed nodes and edges** - rich relational structure for R-GCN style models
- **Node properties** - natural feature vectors for learning
- **Incomplete information** - real knowledge bases have missing facts to infer

Previous work on GPU acceleration (GQLITE-I-0027, ADR GQLITE-A-0003) found that traditional graph algorithms are memory-bound and don't benefit from Apple Silicon's unified memory architecture. However, GNNs shift the bottleneck to dense matrix operations in neural network layers—exactly where GPU excels. This makes MPS (Metal Performance Shaders) viable for GNN workloads where it wasn't for PageRank.

## Goals & Non-Goals

**Goals:**
- Enable machine learning on graph structure within GraphQLite
- Support five core GNN use cases: link prediction, node classification, entity resolution, knowledge completion, and embedding generation
- Leverage MPS/GPU acceleration for the compute-bound neural network operations
- Maintain GraphQLite's embedded, zero-configuration philosophy
- Provide both training and inference capabilities

**Non-Goals:**
- Replacing dedicated ML frameworks (PyTorch, TensorFlow) for research workloads
- Supporting every GNN architecture variant
- Distributed training across multiple machines
- Real-time online learning (batch training is sufficient)

## Use Cases

### Use Case 1: Link Prediction (Recommendations)
- **Actor**: Application developer building a recommendation system
- **Scenario**: 
  1. Load knowledge graph with users, products, and PURCHASED edges
  2. Train link predictor on existing purchase relationships
  3. Query for predicted links between users and unpurchased products
  4. Return ranked product recommendations per user
- **Expected Outcome**: Model predicts missing PURCHASED edges with >0.8 AUC

### Use Case 2: Node Classification (Fraud Detection)
- **Actor**: Security engineer analyzing transaction graphs
- **Scenario**:
  1. Load transaction graph with accounts and TRANSFER edges
  2. Label subset of accounts as fraudulent/legitimate
  3. Train node classifier using semi-supervised learning
  4. Classify unlabeled accounts based on neighborhood patterns
- **Expected Outcome**: Identify suspicious accounts from structural patterns

### Use Case 3: Entity Resolution (Deduplication)
- **Actor**: Data engineer merging datasets
- **Scenario**:
  1. Load graph with potentially duplicate entities from multiple sources
  2. Train entity matcher on known duplicates
  3. Query for candidate duplicate pairs above threshold
  4. Return ranked pairs for human review or automatic merge
- **Expected Outcome**: Surface duplicate entities beyond string matching

### Use Case 4: Knowledge Completion (Inference)
- **Actor**: Knowledge engineer enriching a knowledge base
- **Scenario**:
  1. Load knowledge graph with entities and typed relationships
  2. Train knowledge completion model on existing facts
  3. Query for inferred facts with confidence scores
  4. Surface high-confidence inferences for human validation
- **Expected Outcome**: Infer missing facts like "X works at Y" from indirect evidence

### Use Case 5: Embedding Generation (Semantic Search)
- **Actor**: Developer adding semantic search to application
- **Scenario**:
  1. Load graph and train unsupervised embedding model
  2. Generate vector embeddings for all nodes
  3. Store embeddings as node properties
  4. Query using vector similarity in Cypher
- **Expected Outcome**: Similar entities cluster in vector space

## Architecture

### Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        User API                                  │
│  g.train_link_predictor() / g.predict_links() / g.embed_nodes() │
└─────────────────────────┬───────────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────────┐
│                    GNN Pipeline                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐           │
│  │   Feature    │  │     GNN      │  │   Task Head  │           │
│  │  Extractor   │──▶   Encoder    │──▶  (Decoder)   │           │
│  │              │  │              │  │              │           │
│  │ Properties → │  │ GraphSAGE/  │  │ DistMult/    │           │
│  │ Dense Matrix │  │ R-GCN/GAT   │  │ Classifier/  │           │
│  └──────────────┘  └──────────────┘  │ Comparator   │           │
│                                      └──────────────┘           │
└─────────────────────────┬───────────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────────┐
│                   Compute Backend                                │
│  ┌─────────────────┐        ┌─────────────────┐                 │
│  │   CPU (BLAS)    │   OR   │   MPS (Metal)   │                 │
│  │   Fallback      │        │   Accelerated   │                 │
│  └─────────────────┘        └─────────────────┘                 │
└─────────────────────────────────────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────────┐
│                   GraphQLite Core                                │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐           │
│  │  CSR Cache   │  │   Property   │  │    SQLite    │           │
│  │  (existing)  │  │    Store     │  │   Storage    │           │
│  └──────────────┘  └──────────────┘  └──────────────┘           │
└─────────────────────────────────────────────────────────────────┘
```

### Component Design

**Feature Extractor**
- Extracts numeric properties from nodes into dense feature matrix
- Handles missing values (imputation or masking)
- Normalizes features (z-score or min-max)
- Encodes categorical properties (one-hot or learned embeddings)

**GNN Encoder Layers**
- GraphSAGE: Scalable, samples neighborhoods, mean/max/LSTM aggregation
- R-GCN: Separate weights per edge type, essential for knowledge graphs
- GAT: Attention-weighted aggregation, learns edge importance

**Task Heads**
- Link prediction: DistMult, TransE, RotatE, ComplEx decoders
- Node classification: MLP + softmax/sigmoid
- Entity resolution: Siamese comparator with contrastive loss
- Embedding: Identity (just use encoder output)

**Compute Backend**
- MPS (Metal Performance Shaders) for macOS GPU acceleration
- CPU fallback using BLAS for portability
- Runtime device selection based on availability and graph size

## Detailed Design

### Core GNN Layers (Foundation)

| Algorithm | Purpose | Equation | Priority |
|-----------|---------|----------|----------|
| **GCN** | Baseline layer | H' = σ(D⁻¹AD · H · W) | P0 |
| **GraphSAGE** | Scalable, samples neighborhoods | h'ᵥ = σ(W · [hᵥ ‖ AGG(hᵤ : u ∈ N(v))]) | P0 |
| **R-GCN** | Edge-type aware (knowledge graphs) | h'ᵥ = σ(Σᵣ Σᵤ (1/cᵥ,ᵣ) Wᵣ hᵤ) | P0 |
| **GAT** | Attention-weighted aggregation | h'ᵥ = σ(Σᵤ αᵥᵤ W hᵤ) | P1 |

### Link Prediction Decoders

| Algorithm | Scoring Function | Best For | Priority |
|-----------|------------------|----------|----------|
| **DistMult** | score(h,r,t) = h · Rᵣ · t | Symmetric relations | P0 |
| **TransE** | score(h,r,t) = -‖h + rᵣ - t‖ | Hierarchical relations | P0 |
| **RotatE** | score(h,r,t) = -‖h ∘ rᵣ - t‖ (complex) | Composition patterns | P1 |
| **ComplEx** | Re(〈h, rᵣ, t̄〉) | Asymmetric relations | P1 |

### Training Components

| Component | Description | Priority |
|-----------|-------------|----------|
| **Negative Sampling** | Corrupt head/tail for contrastive learning | P0 |
| **Mini-batch Sampling** | Sample subgraphs for scalability | P0 |
| **Adam Optimizer** | Standard optimizer with weight decay | P0 |
| **Early Stopping** | Halt on validation metric plateau | P0 |
| **Contrastive Loss** | Margin-based or cross-entropy | P0 |
| **Triplet Loss** | For entity resolution | P1 |

### Embedding Generation

| Algorithm | Method | Priority |
|-----------|--------|----------|
| **Node2Vec** | Biased random walks → skip-gram | P1 |
| **DGI** | Maximize mutual info (node ↔ graph) | P2 |
| **GraphSAGE-unsup** | Predict co-occurrence in walks | P1 |

### API Design

```python
# Training API
model = g.train_link_predictor(
    edge_types=["PURCHASED", "VIEWED"],
    encoder="rgcn",           # or "graphsage", "gat"
    decoder="distmult",       # or "transe", "rotate"
    layers=2,
    hidden_dim=128,
    epochs=100,
    device="mps"              # or "cpu"
)

# Inference API  
predictions = g.predict_links(
    model=model,
    source_type="User",
    target_type="Product",
    top_k=10
)

# Embedding API
g.embed_nodes(
    model=model,              # or method="node2vec"
    property_name="embedding"
)

# Classification API
model = g.train_node_classifier(
    label_property="is_fraud",
    encoder="graphsage",
    layers=3
)
predictions = g.classify_nodes(model=model)

# Entity Resolution API
model = g.train_entity_matcher(
    positive_pairs=[("n1", "n2"), ...],
    encoder="graphsage"
)
duplicates = g.find_duplicates(model=model, threshold=0.9)
```

### Storage Format

**Model Weights**: Stored as BLOB in SQLite table
```sql
CREATE TABLE gnn_models (
    name TEXT PRIMARY KEY,
    encoder_type TEXT,
    decoder_type TEXT,
    config JSON,
    weights BLOB,
    created_at TIMESTAMP
);
```

**Embeddings**: Stored as node properties (BLOB or JSON array)
```sql
-- Vector similarity function
SELECT * FROM nodes 
WHERE vector_cosine(properties->>'embedding', ?) > 0.8
```

## Testing Strategy

### Unit Testing
- **GNN Layers**: Verify forward pass produces correct tensor shapes
- **Decoders**: Test scoring functions against known values
- **Feature Extraction**: Test property-to-matrix conversion
- **Coverage Target**: >90% for core GNN operations

### Integration Testing
- **End-to-end pipelines**: Train → save → load → predict cycle
- **SQLite integration**: Model storage and retrieval
- **Python bindings**: All APIs accessible from Python

### Benchmarks
- **Standard datasets**: Cora, CiteSeer, FB15k-237, WN18RR
- **Metrics**: AUC, MRR, Hits@K for link prediction; F1 for classification
- **Performance**: Training throughput (nodes/sec), inference latency

## Alternatives Considered

### Alternative 1: External Training with Import
**Approach**: Export graphs to PyTorch Geometric, train externally, import embeddings.
**Pros**: Mature ecosystem, no ML code to maintain.
**Cons**: Breaks embedded philosophy, requires Python ML stack, data transfer overhead.
**Decision**: Rejected for core functionality, but support export format for advanced users.

### Alternative 2: ONNX Runtime Only
**Approach**: Train externally, deploy via ONNX Runtime for inference only.
**Pros**: Cross-platform, optimized inference.
**Cons**: No in-database training, limited to pre-trained models.
**Decision**: Include as inference backend option, but not primary approach.

### Alternative 3: MLX Framework
**Approach**: Use Apple's MLX for both training and inference.
**Pros**: Apple-native, good MPS integration, NumPy-like API.
**Cons**: macOS only, younger ecosystem, Python-first.
**Decision**: Consider for future if MPS integration proves difficult in Rust.

### Alternative 4: Pure CPU Implementation
**Approach**: Skip GPU entirely, use optimized BLAS.
**Pros**: Maximum portability, simpler build.
**Cons**: Slower training on large graphs.
**Decision**: Include as fallback, but prioritize MPS for macOS.

## Implementation Plan

### Phase 1: Foundation
- Tensor abstraction layer (CPU + MPS backends)
- Feature extraction from node properties
- GCN and GraphSAGE layer implementations
- Basic training loop with Adam optimizer

### Phase 2: Knowledge Graph Support
- R-GCN layer for typed edges
- DistMult and TransE decoders
- Link prediction training pipeline
- Negative sampling strategies

### Phase 3: Full Use Case Coverage
- Node classification head and training
- Entity resolution with Siamese architecture
- Embedding generation (Node2Vec, unsupervised GraphSAGE)
- Model serialization to SQLite

### Phase 4: Production Readiness
- GAT layer implementation
- RotatE and ComplEx decoders
- Vector similarity functions in Cypher
- Performance optimization and benchmarking
- Documentation and examples

### Dependencies
- Builds on CSR caching from GQLITE-I-0028
- May inform future GPU work (GQLITE-I-0027)
- Related to ADR GQLITE-A-0003 findings on GPU viability