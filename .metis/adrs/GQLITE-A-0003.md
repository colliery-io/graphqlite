---
id: 001-gpu-compute-framework-selection
level: adr
title: "GPU Compute Framework Selection"
number: 1
short_code: "GQLITE-A-0003"
created_at: 2026-01-08T14:35:13.747066+00:00
updated_at: 2026-01-10T01:09:58.031744+00:00
decision_date: 
decision_maker: 
parent: 
archived: false

tags:
  - "#adr"
  - "#phase/decided"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# ADR-3: GPU Compute Framework Selection

## Status

**DECIDED: GPU acceleration not pursued. CPU caching provides sufficient optimization (2x speedup).**

## Context

GraphQLite investigated GPU acceleration for graph algorithms to address performance bottlenecks at scale (PageRank: 148ms at 100K nodes → 37.81s at 1M nodes). The investigation explored:

1. wgpu/WGSL (cross-platform WebGPU)
2. Native Metal on macOS with SIMD group functions
3. CUDA on Linux (theoretical, not implemented)

The existing codebase is C with Rust bindings. The CSR graph format is already GPU-transfer-friendly.

## Decision

**Do not pursue GPU acceleration. Instead, implement CPU-side CSR graph caching which provides 2x speedup with minimal complexity.**

Key findings:
1. **Metal GPU provides no benefit (~1.0x)** on Apple Silicon due to unified memory architecture
2. **wgpu/WGSL lacks subgroup operations** needed for efficient parallel reductions
3. **SQLite I/O dominates execution time (~95%)** - caching the graph structure is the real win
4. **CPU caching provides 2x speedup** - sufficient for most use cases with far less complexity

**Implemented solution:**
- Single library with optional graph caching via `gql_load_graph()` / `gql_unload_graph()`
- Per-connection cache scope with automatic cleanup
- All 17 graph algorithms refactored to use cached graph when available

### Revision History

- **2026-01-09 (rev 2)**: Architectural redesign. Benchmarking revealed SQLite I/O dominates execution time (~95%), making automatic GPU dispatch ineffective. Changed to explicit two-library model with user-controlled graph caching. Per-connection cache scope with explicit lifecycle management.
- **2026-01-09 (rev 1)**: Revised from wgpu/WGSL to platform-native backends. wgpu's WGSL compiler (naga) lacks subgroup operation support, preventing implementation of vector kernels needed for GPU speedup. Platform-native APIs provide full SIMD/warp primitive access.

### Library Architecture

| Library | Contents | Dependencies | Use Case |
|---------|----------|--------------|----------|
| **`graphqlite.dylib`** | Base Cypher, CPU algorithms | None (pure C) | Default, size-constrained, simple deployments |
| **`graphqlite_gpu.dylib`** | Base + GPU algorithms + graph caching | Metal (macOS) or CUDA (Linux) | Repeated graph analytics, large graphs |

### Graph Caching Model

**Scope:** Per-connection (automatic cleanup on connection close)

**Lifecycle:**
```sql
.load graphqlite_gpu.dylib

-- Explicit load: builds CSR from SQLite, caches in connection memory
SELECT cypher('CALL loadGraph()');

-- Algorithms use cached graph (fast, no I/O)
SELECT cypher('RETURN pageRank(0.85, 20)');
SELECT cypher('RETURN betweenness()');

-- Explicit unload (or automatic on connection close)
SELECT cypher('CALL unloadGraph()');

-- Introspection
SELECT cypher('CALL graphStatus()');  -- Returns: cached, node_count, edge_count, memory_bytes
```

**Error Handling:**
- Cypher: Algorithm called without `loadGraph()` → explicit error with recovery hint
- Bindings (Python/etc): May auto-recover by calling `loadGraph()` implicitly

**Mutation Handling:**
- Graph mutations (`CREATE`, `DELETE`, `SET`) while cache exists: user responsibility
- Recommended pattern: use multigraph to separate read-heavy (cached) from write-heavy workloads
- Future: optional lock to prevent mutations while cache active

### Platform-Specific Backends

| Platform | Backend | Shader Language | SIMD Support |
|----------|---------|-----------------|--------------|
| macOS | Metal (metal-rs) | MSL | `simd_sum`, `simd_shuffle`, `simd_broadcast` |
| Linux | CUDA | CUDA C++ | `__shfl_down_sync`, `__reduce_add_sync`, warp primitives |

Future: Vulkan backend for AMD/Intel GPUs on Linux (when needed).

### Build Invocation

```makefile
# Base library (CPU-only, no caching)
make extension
# Output: build/graphqlite.dylib

# GPU library (GPU + graph caching)
make extension-gpu GPU=1
# Output: build/graphqlite_gpu.dylib
# macOS: builds Metal backend
# Linux: builds CUDA backend (requires CUDA toolkit)
```

```bash
# Python
pip install graphqlite           # Ships both libraries
# User chooses which to load at runtime
```

### GPU-Enabled Build Details

**macOS (Metal):**
- Uses metal-rs crate for Metal API access
- MSL shaders with SIMD group functions for vector kernels
- Compiled as static library, linked into C extension

**Linux (CUDA):**
- Uses cuda-sys/rustacuda for CUDA API access
- CUDA kernels with warp-level primitives
- Requires CUDA toolkit installed
- Compiled as static library, linked into C extension

FFI boundary (same across platforms):
```rust
#[no_mangle]
pub extern "C" fn gpu_pagerank(
    node_count: i32,
    row_ptr: *const i32,
    col_idx: *const i32,
    scores: *mut f32,
    damping: f32,
    max_iter: i32,
) -> i32
```

Cargo configuration:
```toml
[lib]
crate-type = ["staticlib"]

[features]
default = []
gpu = ["gpu-metal", "gpu-cuda"]
gpu-metal = ["metal"]        # macOS only
gpu-cuda = ["rustacuda"]     # Linux only

[profile.release]
lto = true
panic = "abort"  # No unwinding across FFI
```

## Alternatives Analysis

| Option | Pros | Cons | Risk Level | Implementation Cost |
|--------|------|------|------------|-------------------|
| **Metal + CUDA** | Full SIMD/warp access; best per-platform performance; proven primitives | Two shader codebases; platform-specific builds | Low | Medium |
| wgpu + WGSL | Single shader language; runtime backend selection | naga lacks subgroup support; scalar kernel only; no speedup achieved | High | Medium |
| Separate Metal + Vulkan + CUDA | Maximum coverage | Three codebases; high maintenance | Medium | High |
| OpenCL | Single API; mature | Declining ecosystem; inferior NVIDIA support; verbose | Medium | Medium |
| CUDA only | Best NVIDIA performance | No macOS support | High | Low |
| Vulkan only | Cross-platform | No macOS without MoltenVK; subgroup support varies | Medium | High |

### wgpu/WGSL Post-Mortem (2026-01-09)

Initial implementation used wgpu with WGSL shaders. Results:
- Scalar kernel (1 thread per node): **No speedup** (0.84x-0.99x vs CPU)
- Vector kernel attempt: **Blocked** - naga doesn't support `enable subgroups;` WGSL directive
- Without subgroup operations, cannot implement warp-level parallel reductions needed for GPU benefit

The CUDA PageRank reference (github.com/anshulk-cmu/CUDA_PageRank) achieves 28x speedup specifically via warp primitives (`__shfl_down_sync`). Platform-native backends provide these primitives today.

### Metal/macOS Experiments (2026-01-09)

After switching from wgpu to native Metal with MSL shaders supporting `simd_sum()` for parallel reductions, comprehensive benchmarking was performed comparing CPU cached vs GPU cached performance:

| Nodes | Edges | CPU Cached | GPU Cached | GPU vs CPU |
|-------|-------|------------|------------|------------|
| 1K | 10K | 0.5ms | 0.6ms | **0.9x** |
| 5K | 50K | 3.9ms | 3.3ms | **1.2x** |
| 10K | 100K | 6.7ms | 8.5ms | **0.8x** |
| 20K | 200K | 14.1ms | 14.0ms | **1.0x** |
| 50K | 500K | 35.1ms | 35.0ms | **1.0x** |
| 100K | 1M | 76.1ms | 76.5ms | **1.0x** |
| 200K | 2M | 163.3ms | 153.9ms | **1.1x** |

**Conclusion: Metal GPU acceleration provides ~1.0x speedup (no meaningful benefit) on Apple Silicon.**

**Root causes:**
1. **PageRank is memory-bound, not compute-bound** - Algorithm spends most time fetching neighbors from memory, not computing. GPU excels at compute parallelism, not memory access.
2. **Apple Silicon unified memory** - CPU and GPU share the same memory bandwidth (~200 GB/s). Unlike discrete NVIDIA GPUs with dedicated high-bandwidth VRAM (900+ GB/s), there's no memory advantage for GPU.
3. **Optimized CPU baseline** - Our CPU implementation uses push-based iteration, float32 precision, and early convergence detection. The CUDA benchmarks showing 28x speedup compared against naive CPU implementations.
4. **Overhead negates parallelism** - Buffer creation, command encoding, and synchronization add latency that offsets parallel execution gains.

**Recommendation:** Metal/macOS GPU acceleration for PageRank is **not justified**. The complexity and maintenance burden outweigh the negligible performance benefit.

**CUDA may still prove useful** on Linux with discrete NVIDIA GPUs due to:
- Dedicated high-bandwidth VRAM (separate from CPU memory)
- Mature warp-level primitives optimized over 15+ years
- Different memory architecture may favor GPU for memory-bound workloads

This remains to be validated with actual CUDA benchmarks.

## Rationale

### Why Two Libraries Instead of Automatic GPU Dispatch?

Initial implementation used automatic GPU dispatch based on cost thresholds. Benchmarking revealed:

| Nodes | Edges | CPU Time | GPU Time | Speedup |
|-------|-------|----------|----------|---------|
| 100K | 1.0M | 259ms | 262ms | 0.99x |
| 1.0M | 10.0M | 94.76s | 91.01s | 1.04x |

**Root cause**: `csr_graph_load()` dominates execution time (~95%). Each `pageRank()` call:
1. Queries SQLite for all nodes (full table scan)
2. Builds node ID → index hash table
3. Queries SQLite for all edges (full table scan) - **twice**
4. Builds CSR row_ptr and col_idx arrays

GPU compute itself is fast (~10ms for 200K nodes), but invisible under I/O overhead.

### Why Explicit Caching?

1. **Caching has trade-offs**: Memory usage, staleness risk, mutation complexity
2. **Users should know**: "My graph is cached and mutations won't be reflected"
3. **Fits advanced use case**: Graph analytics users understand this model
4. **Composable**: Works naturally with multigraph for read/write separation

### Why Per-Connection Scope?

1. **Simplest implementation**: No reference counting or global state
2. **Automatic cleanup**: Connection close = memory freed
3. **Matches SQLite model**: Connections are independent
4. **Safe default**: No cross-connection surprises

## Consequences

### Positive (CPU Caching Approach)
- **Simple implementation**: Pure C, no GPU dependencies or Rust required
- **Cross-platform**: Works identically on macOS, Linux, Windows
- **2x speedup**: Meaningful performance improvement with minimal complexity
- **Explicit user control**: Clear mental model - "load graph, run algorithms, unload"
- **Automatic cleanup**: Per-connection scope means no memory leaks
- **Single library**: No packaging complexity

### Negative
- **Not as fast as theoretical GPU**: 2x vs potential 10-30x with optimized CUDA
- **Memory usage**: Cached graph doubles memory footprint while loaded

### Neutral
- CUDA investigation remains open for future if 2x proves insufficient
- Memory usage is user-controlled (explicit load/unload)

## Lessons Learned

1. **Benchmark early**: wgpu/WGSL looked promising on paper but lacked critical features
2. **Unified memory changes the equation**: Apple Silicon's shared memory eliminates GPU memory bandwidth advantage
3. **Profile the whole pipeline**: GPU compute was fast; I/O was the bottleneck
4. **Simpler solutions win**: CPU caching required 1/10th the code and provides real benefit