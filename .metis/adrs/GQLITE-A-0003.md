---
id: 001-gpu-compute-framework-selection
level: adr
title: "GPU Compute Framework Selection"
number: 1
short_code: "GQLITE-A-0003"
created_at: 2026-01-08T14:35:13.747066+00:00
updated_at: 2026-01-08T14:35:13.747066+00:00
decision_date: 
decision_maker: 
parent: 
archived: false

tags:
  - "#adr"
  - "#phase/draft"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# ADR-3: GPU Compute Framework Selection

## Context

GraphQLite needs GPU acceleration for graph algorithms to address performance bottlenecks at scale (PageRank: 148ms at 100K nodes → 37.81s at 1M nodes). This requires choosing a GPU compute framework that:

1. Supports multiple platforms (macOS, Linux, Windows)
2. Integrates with our C-based SQLite extension
3. Minimizes maintenance burden (ideally single shader codebase)
4. Allows single binary distribution

The existing codebase is C with Rust bindings. The CSR graph format is already GPU-transfer-friendly.

## Decision

**Use wgpu (Rust) with WGSL shaders for GPU acceleration, with GPU support as an opt-in build profile.**

### Build Profiles

| Profile | Description | Binary Size | Use Case |
|---------|-------------|-------------|----------|
| **CPU-only** (default) | No Rust/wgpu dependency | ~200KB | Edge, embedded, size-constrained |
| **GPU-enabled** (opt-in) | Includes wgpu, runtime backend selection | ~3-5MB | Desktop, server, large graphs |

### Build Invocation

```makefile
# CPU-only (default) - no Rust required
make extension

# GPU-enabled (opt-in) - requires Rust toolchain
make extension GPU=1
```

```bash
# Rust bindings
cargo build                      # CPU-only
cargo build --features gpu       # GPU-enabled

# Python
pip install graphqlite           # CPU-only
pip install graphqlite[gpu]      # GPU-enabled
```

### GPU-Enabled Build Details

When `GPU=1` or `--features gpu`:
- Rust GPU backend compiled as static library, linked into C extension
- Compute shaders written in WGSL (WebGPU Shading Language)
- wgpu automatically selects Metal (macOS), Vulkan (Linux/Windows), or DX12 (Windows) at runtime
- CPU fallback always available within GPU-enabled builds

FFI boundary:
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

[profile.release]
lto = true
panic = "abort"  # No unwinding across FFI
```

## Alternatives Analysis

| Option | Pros | Cons | Risk Level | Implementation Cost |
|--------|------|------|------------|-------------------|
| **wgpu + WGSL** | Single shader language; runtime backend selection; modern API; proven (powers major apps) | Adds Rust to build; ~3-5MB binary size increase | Low | Medium |
| Separate Metal + Vulkan | Platform-optimal code; no Rust dependency | Two shader codebases; two implementations; double maintenance | Medium | High |
| OpenCL | Single API; mature | Declining ecosystem; inferior NVIDIA support; verbose | Medium | Medium |
| CUDA only | Best NVIDIA performance | No macOS support; excludes AMD/Intel GPUs | High | Low |
| Vulkan only | Cross-platform | No macOS without MoltenVK overhead; complex API | Medium | High |

## Rationale

1. **Single shader codebase**: WGSL eliminates maintaining parallel MSL/GLSL implementations. Algorithm logic written once.

2. **Runtime backend selection**: wgpu handles Metal/Vulkan/DX12 selection automatically. Enables single binary distribution without conditional compilation or multiple artifacts.

3. **Proven in production**: wgpu powers Firefox's WebGPU, multiple game engines, and terminal emulators. Runtime detection is battle-tested.

4. **Rust already adjacent**: Project has Rust bindings. Team has Rust experience. Build integration is tractable.

5. **FFI is simple**: Our interface is small (handful of functions taking CSR arrays). Memory stays C-owned. Well-trodden path.

6. **Future-proof**: WebGPU is the emerging standard. WGSL shaders could potentially run in browser contexts.

## Consequences

### Positive
- Single shader codebase (WGSL) for all platforms
- Single binary distribution simplifies packaging and user experience
- Runtime backend selection "just works" on user machines
- Modern, actively maintained ecosystem
- Opens path to WebGPU/browser execution in future
- Clean FFI boundary with minimal surface area

### Negative
- Binary size increases ~15-25x for GPU-enabled builds (~3-5MB from wgpu + naga)
- Rust toolchain required for builds (not for CPU-only development)
- Mixed C/Rust debugging requires familiarity with both
- wgpu's naga shader compiler adds build-time dependency

### Neutral
- GPU builds require Rust; CPU-only development unchanged
- Developers working on GPU code need Rust knowledge
- CI needs Rust installation for GPU build jobs