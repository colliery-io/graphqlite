---
id: implement-cpu-only-and-gpu-enabled
level: task
title: "Implement CPU-only and GPU-enabled build profiles"
short_code: "GQLITE-T-0090"
created_at: 2026-01-08T14:40:36.764584+00:00
updated_at: 2026-01-08T14:40:36.764584+00:00
parent: GQLITE-I-0027
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
strategy_id: NULL
initiative_id: GQLITE-I-0027
---

# Implement CPU-only and GPU-enabled build profiles

## Parent Initiative

[[GQLITE-I-0027]] - GPU-Accelerated Graph Algorithms

## Objective

Ensure the build system supports two distinct profiles: a slim CPU-only build (default) and an opt-in GPU-enabled build. This preserves backwards compatibility and keeps binary size small for edge/embedded deployments while allowing desktop/server users to opt into GPU acceleration.

## Acceptance Criteria

- [ ] `make extension` produces CPU-only binary (~200KB, no Rust dependency)
- [ ] `make extension GPU=1` produces GPU-enabled binary (~3-5MB, includes wgpu)
- [ ] CPU-only build works without Rust toolchain installed
- [ ] GPU-enabled build gracefully handles missing GPU at runtime (falls back to CPU)
- [ ] Rust bindings support `--features gpu` flag
- [ ] Python package supports `pip install graphqlite[gpu]` optional extra
- [ ] CI builds both profiles on all platforms (macOS, Linux, Windows)
- [ ] Binary size documented in release notes

## Implementation Notes

### Technical Approach

**Makefile changes:**
```makefile
# Detect GPU build flag
ifdef GPU
    RUST_GPU_LIB = src/gpu/target/release/libgraphqlite_gpu.a
    CFLAGS += -DGPU_ENABLED
    LDFLAGS += -L src/gpu/target/release -lgraphqlite_gpu
endif

extension: $(RUST_GPU_LIB)
    $(CC) $(CFLAGS) ... $(LDFLAGS)

# Only build Rust lib if GPU=1
ifdef GPU
$(RUST_GPU_LIB):
    cd src/gpu && cargo build --release
endif
```

**C preprocessor guards:**
```c
// graph_algo_pagerank.c
#ifdef GPU_ENABLED
extern int gpu_pagerank(...);
#endif

int execute_pagerank(...) {
    #ifdef GPU_ENABLED
    if (should_use_gpu(cost, threshold)) {
        return gpu_pagerank(...);
    }
    #endif
    return cpu_pagerank(...);
}
```

**Rust Cargo.toml:**
```toml
[features]
default = []
gpu = ["wgpu", "naga"]

[dependencies]
wgpu = { version = "...", optional = true }
```

**Python setup.py / pyproject.toml:**
```toml
[project.optional-dependencies]
gpu = ["graphqlite-gpu"]  # or bundled differently
```

### Dependencies

- Phase 1 of initiative (build integration) must establish basic Rust linking first
- Requires deciding on Python GPU packaging strategy (separate wheel vs extras)

### Risk Considerations

- **Conditional compilation complexity**: `#ifdef` guards can become messy. Keep them at dispatch boundaries only.
- **CI matrix expansion**: Now testing 2 profiles × 3 platforms = 6 configurations minimum
- **Python packaging**: `[gpu]` extra may need platform-specific wheels or runtime library bundling

## Status Updates

*To be added during implementation*