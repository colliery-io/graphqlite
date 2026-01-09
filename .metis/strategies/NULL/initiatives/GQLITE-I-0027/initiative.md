---
id: gpu-accelerated-graph-algorithms
level: initiative
title: "GPU-Accelerated Graph Algorithms"
short_code: "GQLITE-I-0027"
created_at: 2026-01-08T14:35:13.680145+00:00
updated_at: 2026-01-08T14:35:13.680145+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/discovery"


exit_criteria_met: false
estimated_complexity: XL
strategy_id: NULL
initiative_id: gpu-accelerated-graph-algorithms
---

# GPU-Accelerated Graph Algorithms Initiative

## Context

GraphQLite's graph algorithms show significant performance degradation at scale. Benchmarks demonstrate:

| Algorithm | 100K nodes | 1M nodes |
|-----------|------------|----------|
| PageRank | 148ms | 37.81s |
| Label Propagation | ~400ms | 40+ s |

These iterative, parallelizable algorithms are ideal candidates for GPU acceleration. The existing CSR (Compressed Sparse Row) graph format is already GPU-friendly, and algorithms are modular C files with clean separation between graph loading and computation.

## Goals & Non-Goals

**Goals:**
- Accelerate computationally intensive graph algorithms via GPU compute
- Maintain single binary distribution with runtime backend selection
- Provide configurable thresholds based on algorithmic complexity (O-notation)
- Support Metal (macOS), Vulkan (Linux/Windows), and DX12 (Windows) via wgpu
- Preserve CPU fallback for all algorithms
- Enable new algorithm possibilities (TSP, node2vec, GNN primitives)

**Non-Goals:**
- CUDA support (may be added later if needed)
- GPU acceleration for parser/transformer stages
- Rewriting existing CPU implementations (they remain as fallback)
- Mobile platform support (iOS/Android)
- Forcing GPU dependency on edge/embedded users (GPU is opt-in)

## Architecture

### Build Profiles

| Profile | Build Command | Binary Size | Rust Required | Use Case |
|---------|--------------|-------------|---------------|----------|
| **CPU-only** (default) | `make extension` | ~200KB | No | Edge, embedded, size-constrained |
| **GPU-enabled** (opt-in) | `make extension GPU=1` | ~3-5MB | Yes | Desktop, server, large graphs |

### Overview (GPU-enabled build)

Single binary with runtime GPU backend selection:

```
graphqlite.{dylib|so|dll}
├── SQLite extension entry (C)
├── Parser/Transformer (C)
├── Executor (C)
│   └── Algorithm dispatch
│       ├── cost < threshold? → CPU path (C)
│       └── cost ≥ threshold? → GPU path (Rust/wgpu)
│           └── wgpu selects: Metal | Vulkan | DX12
└── CPU algorithm implementations (C)
```

### Component Structure

```
├── src/gpu/
│   ├── Cargo.toml
│   ├── src/
│   │   ├── lib.rs           # FFI exports (C-callable functions)
│   │   ├── backend.rs       # wgpu device management
│   │   ├── config.rs        # Threshold configuration
│   │   ├── pagerank.rs      # Algorithm orchestration
│   │   └── shaders/
│   │       ├── pagerank.wgsl
│   │       ├── label_prop.wgsl
│   │       └── components.wgsl
└── src/backend/executor/
    └── graph_algo_*.c       # CPU implementations (unchanged)
```

### FFI Boundary

Rust exposes C-callable functions:

```rust
#[no_mangle]
pub extern "C" fn gpu_pagerank(
    node_count: i32,
    row_ptr: *const i32,
    col_idx: *const i32,
    scores: *mut f32,
    damping: f32,
    max_iter: i32,
) -> i32 { ... }
```

C code calls these when GPU path is selected:

```c
extern int gpu_pagerank(int, const int*, const int*, float*, float, int);
```

Memory ownership: C owns all buffers, Rust borrows. No cross-boundary allocations.

## Detailed Design

### Configuration API

SQL-based configuration matching library ergonomics:

```sql
-- Global GPU toggle
SELECT graphqlite_config('gpu_enabled', true);

-- Algorithm-specific thresholds (based on O-notation cost)
SELECT graphqlite_config('gpu_threshold.pagerank', 1000000);
SELECT graphqlite_config('gpu_threshold.betweenness', 50000000);

-- Query current settings
SELECT graphqlite_config('gpu_threshold.pagerank');

-- Introspection
SELECT graphqlite_complexity('pagerank');
-- Returns: "O(k(V + E))"

SELECT graphqlite_gpu_decision('pagerank', 100000, 500000, 20);
-- Returns: { "cost": 12000000, "threshold": 1000000, "use_gpu": true }
```

### Algorithm Complexity Reference

| Algorithm | Complexity | Cost Formula |
|-----------|------------|--------------|
| PageRank | O(k(V + E)) | iterations * (V + E) |
| Label Propagation | O(k(V + E)) | iterations * (V + E) |
| Connected Components | O(V + E) | V + E |
| Betweenness | O(V × E) | V * E |
| Closeness | O(V × (V + E)) | V * (V + E) |
| APSP | O(V² log V + VE) | V*V*log(V) + V*E |

### Build Integration

```makefile
extension:
    cd src/gpu && cargo build --release
    $(CC) ... -L src/gpu/target/release -lgraphqlite_gpu ...
```

Rust component built as static library, linked into final extension.

### Validation Strategy

- **Floating-point algorithms**: Tolerance-based (e.g., PageRank within 1e-6)
- **Integer algorithms**: Exact match (connected components, etc.)
- **Behavioral equivalence**: Same rankings/assignments even if scores vary slightly

## Testing Strategy

### Unit Testing
- WGSL shader correctness via wgpu test harness
- Rust algorithm orchestration tests
- C/Rust FFI boundary tests

### Integration Testing
- GPU vs CPU result comparison on reference graphs
- Threshold decision logic verification
- Configuration API tests

### Performance Testing
- Benchmark suite comparing CPU vs GPU across graph sizes
- Profile to determine optimal default thresholds per algorithm

## Alternatives Considered

### Separate Metal/Vulkan Implementations
Write native MSL and GLSL shaders separately. Rejected: doubles shader maintenance burden, wgpu provides unified abstraction.

### Multiple Binary Distribution
Ship separate CPU-only and GPU builds. Rejected: wgpu's runtime backend selection makes single binary feasible and simplifies distribution.

### OpenCL
Cross-platform but declining ecosystem. NVIDIA support inferior to native. Rejected in favor of wgpu's modern approach.

### CUDA First
Best NVIDIA performance but locks out macOS (Metal) and non-NVIDIA Linux. Rejected: Metal + Vulkan coverage is broader. CUDA can be added later if needed.

## Implementation Plan

### Phase 1: Build Integration
- Integrate Rust/wgpu into Makefile build
- Establish FFI boundary with stub functions
- Verify single binary loads correctly across platforms

### Phase 2: Configuration API
- Implement `graphqlite_config()` SQL function
- Add complexity calculation utilities
- Implement threshold decision logic

### Phase 3: First Algorithm (PageRank)
- Port PageRank to WGSL compute shader
- Implement Rust orchestration layer
- Validate against CPU implementation
- Benchmark and tune default threshold

### Phase 4: Validation Framework
- Build comparison test harness
- Establish tolerance criteria
- CI integration (compile-only for GPU, full test on release)

### Phase 5: Additional Algorithms
- Label Propagation
- Connected Components
- Betweenness Centrality

### Phase 6: Bindings Updates
- Rust bindings: `gpu` feature flag
- Python bindings: optional GPU dependencies

### Future Phases
- New GPU-enabled algorithms: TSP, node2vec, GNN primitives
- CUDA backend (if demand warrants)
- Runtime detection for automatic GPU library loading