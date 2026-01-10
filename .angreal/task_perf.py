"""Performance testing commands for GraphQLite.

Run performance benchmarks at different scale levels to measure
query execution time, memory usage, and scalability.
"""

import angreal
import subprocess
import os
from utils import run_make, ensure_extension_built, get_project_root

perf = angreal.command_group(name="perf", about="Run performance benchmarks")


@perf()
@angreal.command(
    name="quick",
    about="Quick performance check (~30s)",
    tool=angreal.ToolDescription(
        """
Run a quick performance benchmark with 10K nodes.

## When to use
- Fast sanity check during development
- Smoke test for performance regressions
- Quick iteration on optimizations

## Examples
```
angreal perf quick
```

## Duration
Approximately 30 seconds.

## Scale
- 10,000 nodes
- Basic query patterns
""",
        risk_level="safe"
    )
)
@angreal.argument(
    name="verbose",
    long="verbose",
    short="v",
    is_flag=True,
    takes_value=False,
    help="Show verbose output"
)
def perf_quick(verbose: bool = False) -> int:
    """Run quick performance tests."""
    if not ensure_extension_built():
        return 1

    print("Running quick performance tests (~30s, 10K nodes)...")
    return run_make("performance-quick", verbose=verbose)


@perf()
@angreal.command(
    name="standard",
    about="Standard performance suite (~2-3 min)",
    tool=angreal.ToolDescription(
        """
Run the standard performance benchmark suite.

## When to use
- Regular performance validation
- Before merging significant changes
- Baseline performance measurement

## Examples
```
angreal perf standard
angreal perf standard --iterations 5
```

## Duration
Approximately 2-3 minutes.

## Scale
- 100,000 nodes
- Full query pattern coverage
""",
        risk_level="safe"
    )
)
@angreal.argument(
    name="iterations",
    long="iterations",
    short="n",
    python_type="int",
    help="Number of iterations per test"
)
@angreal.argument(
    name="verbose",
    long="verbose",
    short="v",
    is_flag=True,
    takes_value=False,
    help="Show verbose output"
)
def perf_standard(iterations: int = None, verbose: bool = False) -> int:
    """Run standard performance tests."""
    if not ensure_extension_built():
        return 1

    print("Running standard performance tests (~2-3 min)...")
    if iterations:
        return run_make("performance", verbose=verbose, ITERATIONS=str(iterations))
    return run_make("performance", verbose=verbose)


@perf()
@angreal.command(
    name="full",
    about="Full performance suite (~10 min)",
    tool=angreal.ToolDescription(
        """
Run the comprehensive performance benchmark suite.

## When to use
- Pre-release performance validation
- Detailed scalability analysis
- Comprehensive regression testing

## Examples
```
angreal perf full
```

## Duration
Approximately 10 minutes.

## Scale
- Up to 1,000,000 nodes
- All query patterns
- Memory profiling
- Scalability curves
""",
        risk_level="safe"
    )
)
@angreal.argument(
    name="iterations",
    long="iterations",
    short="n",
    python_type="int",
    help="Number of iterations per test"
)
@angreal.argument(
    name="verbose",
    long="verbose",
    short="v",
    is_flag=True,
    takes_value=False,
    help="Show verbose output"
)
def perf_full(iterations: int = None, verbose: bool = False) -> int:
    """Run full performance tests."""
    if not ensure_extension_built():
        return 1

    print("Running full performance suite (~10 min, up to 1M nodes)...")
    if iterations:
        return run_make("performance-full", verbose=verbose, ITERATIONS=str(iterations))
    return run_make("performance-full", verbose=verbose)


@perf()
@angreal.command(
    name="gpu",
    about="GPU vs CPU performance comparison",
    tool=angreal.ToolDescription(
        """
Compare GPU-accelerated vs CPU-only PageRank performance.

## What this tests
- Builds both CPU-only and GPU-enabled extensions
- Runs PageRank on increasingly large graphs
- Measures execution time for both paths
- Calculates speedup ratios

## When to use
- Evaluating GPU acceleration benefits
- Finding optimal graph sizes for GPU dispatch
- Validating GPU implementation performance

## Examples
```
angreal perf gpu              # Standard benchmark (50K-250K nodes)
angreal perf gpu --mode quick # Quick test (10K-50K nodes)
angreal perf gpu --mode full  # Full suite (up to 1M nodes)
```

## Prerequisites
- Rust toolchain installed
- GPU-capable machine (Metal on macOS, Vulkan on Linux)

## Duration
- quick: ~1 minute
- standard: ~3 minutes
- full: ~10 minutes
""",
        risk_level="safe"
    )
)
@angreal.argument(
    name="mode",
    long="mode",
    short="m",
    default_value="standard",
    help="Benchmark mode: quick, standard, or full"
)
@angreal.argument(
    name="iterations",
    long="iterations",
    short="n",
    python_type="int",
    default_value="3",
    help="Number of test iterations per measurement"
)
@angreal.argument(
    name="pagerank_iters",
    long="pagerank-iters",
    short="p",
    python_type="int",
    default_value="20",
    help="Number of PageRank iterations per test"
)
def perf_gpu(mode: str = "standard", iterations: int = 3, pagerank_iters: int = 20) -> int:
    """Run GPU vs CPU performance comparison."""
    root = get_project_root()
    script = os.path.join(root, "tests", "performance", "perf_gpu_comparison.sh")

    if not os.path.exists(script):
        print(f"Error: Benchmark script not found at {script}")
        return 1

    print(f"Running GPU vs CPU benchmark (mode={mode})...")
    print(f"PageRank iterations: {pagerank_iters}, Test iterations: {iterations}")
    print("")

    env = os.environ.copy()
    env["PERF_ITERATIONS"] = str(iterations)
    env["PAGERANK_ITERS"] = str(pagerank_iters)

    result = subprocess.run([script, mode], cwd=root, env=env)
    return result.returncode
