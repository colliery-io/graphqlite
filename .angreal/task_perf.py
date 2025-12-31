"""Performance testing commands for GraphQLite.

Run performance benchmarks at different scale levels to measure
query execution time, memory usage, and scalability.
"""

import angreal
from utils import run_make, ensure_extension_built

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
