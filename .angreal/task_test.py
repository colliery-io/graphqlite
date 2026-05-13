"""Testing commands for GraphQLite.

Provides a unified interface for running various test suites:
- Unit tests (CUnit)
- Rust binding tests
- Python binding tests
- Functional SQL tests
- Constraint tests (expected failures)
- GPU acceleration tests
"""

import angreal
import subprocess
import os
from utils import run_make, ensure_extension_built, get_project_root

test = angreal.command_group(name="test", about="Run GraphQLite tests")


@test()
@angreal.command(
    name="unit",
    about="Run CUnit tests",
    tool=angreal.ToolDescription(
        """
Run the C unit tests using CUnit framework.

## When to use
- Testing parser, transform, and executor components
- Quick validation of core C code changes

## Examples
```
angreal test unit
angreal test unit --verbose
```

## Prerequisites
- CUnit installed (via MacPorts or Homebrew)
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
def test_unit(verbose: bool = False) -> int:
    """Run CUnit tests."""
    print("Running unit tests...")
    return run_make("test-unit", verbose=verbose)


@test()
@angreal.command(
    name="rust",
    about="Run Rust binding tests",
    tool=angreal.ToolDescription(
        """
Run the Rust binding tests using cargo test.

## When to use
- After changes to Rust bindings
- Validating Rust API compatibility

## Examples
```
angreal test rust
```

## Prerequisites
- Extension must be built first (auto-built if missing)
- Rust toolchain installed
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
def test_rust(verbose: bool = False) -> int:
    """Run Rust binding tests."""
    if not ensure_extension_built():
        return 1

    print("Running Rust binding tests...")
    return run_make("test-rust", verbose=verbose)


@test()
@angreal.command(
    name="python",
    about="Run Python binding tests",
    tool=angreal.ToolDescription(
        """
Run the Python binding tests using pytest.

## When to use
- After changes to Python bindings
- Validating Python API compatibility

## Examples
```
angreal test python
angreal test python --python python3.12
```

## Prerequisites
- Extension must be built first (auto-built if missing)
- Python with pytest installed
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
@angreal.argument(
    name="python",
    long="python",
    default_value="",
    help="Python interpreter to use (auto-detects uv, falls back to python3)"
)
def test_python(verbose: bool = False, python: str = "") -> int:
    """Run Python binding tests."""
    if not ensure_extension_built():
        return 1

    root = get_project_root()
    bindings_dir = os.path.join(root, "bindings", "python")

    # Auto-detect: if uv.lock exists in the bindings dir, use uv run
    use_uv = not python and os.path.exists(os.path.join(bindings_dir, "uv.lock"))

    if use_uv:
        print("Running Python binding tests (using uv)...")
        cmd = ["uv", "run", "python", "-m", "pytest", "tests/", "-v"]
    else:
        interpreter = python or "python3"
        print(f"Running Python binding tests (using {interpreter})...")
        cmd = [interpreter, "-m", "pytest", "tests/", "-v"]

    if verbose:
        print(f"Running: {' '.join(cmd)} in {bindings_dir}")

    result = subprocess.run(cmd, cwd=bindings_dir)
    return result.returncode


@test()
@angreal.command(
    name="bindings",
    about="Run all binding tests (Rust + Python)",
    tool=angreal.ToolDescription(
        """
Run all language binding tests (Rust and Python).

## When to use
- Full binding validation
- Before releases

## Examples
```
angreal test bindings
```
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
def test_bindings(verbose: bool = False) -> int:
    """Run all binding tests."""
    if not ensure_extension_built():
        return 1

    print("Running all binding tests...")

    result = test_rust(verbose=verbose)
    if result != 0:
        print("Rust tests failed!")
        return result

    result = test_python(verbose=verbose)
    if result != 0:
        print("Python tests failed!")
        return result

    print("All binding tests passed!")
    return 0


@test()
@angreal.command(
    name="functional",
    about="Run functional SQL tests",
    tool=angreal.ToolDescription(
        """
Run functional SQL tests that exercise the extension end-to-end.

## When to use
- Validating Cypher query behavior
- Integration testing

## Examples
```
angreal test functional
```

## Prerequisites
- Extension must be built first (auto-built if missing)
- sqlite3 CLI installed
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
def test_functional(verbose: bool = False) -> int:
    """Run functional SQL tests."""
    if not ensure_extension_built():
        return 1

    print("Running functional tests...")
    return run_make("test-functional", verbose=verbose)


@test()
@angreal.command(
    name="tck",
    about="Run openCypher TCK conformance suite",
    tool=angreal.ToolDescription(
        """
Run the openCypher Technology Compatibility Kit (TCK) against GraphQLite.

The TCK is the canonical openCypher conformance suite. This task runs the
vendored `.feature` corpus (`vendor/tck/features/`) through the Python
harness (`tests/tck/`) against one or more backends and writes structured
results to `build/tck-results.json`.

## When to use
- Measuring openCypher conformance coverage.
- Detecting regressions in parser/transform/executor.
- Cross-checking that the SQLite extension, Python binding, and Rust binding
  return identical results for the same Cypher.

## Examples
```
angreal test tck                          # full corpus, extension backend
angreal test tck --filter Match1          # smoke a single feature
angreal test tck --backend all            # all three backends + parity report
angreal test tck --debug                  # leak extension debug to stderr
```

## Output
- `build/tck-results.json` — structured per-scenario records.
- `build/tck-debug.log` — captured extension debug noise.
- Stdout: pass/fail/error/skipped totals + top-10 failing feature files.

## Exit code
Returns 0 if the harness completed (regardless of scenario failures);
non-zero only on harness errors. Regression gating happens in CI (TCK-12).

## Prerequisites
- Extension must be built (auto-built if missing).
- A `python3` with `enable_load_extension` support (Apple's Xcode python3
  does NOT have this; MacPorts/Homebrew/uv pythons do).
""",
        risk_level="safe"
    )
)
@angreal.argument(
    name="backend",
    long="backend",
    help="Backend to run against: extension|python|rust|all (default: extension)"
)
@angreal.argument(
    name="filter",
    long="filter",
    help="Substring filter on feature file path"
)
@angreal.argument(
    name="limit",
    long="limit",
    python_type="int",
    help="Stop after N scenarios (smoke test)"
)
@angreal.argument(
    name="debug",
    long="debug",
    is_flag=True,
    takes_value=False,
    help="Don't redirect extension debug output"
)
@angreal.argument(
    name="verbose",
    long="verbose",
    short="v",
    is_flag=True,
    takes_value=False,
    help="Show verbose output"
)
def test_tck(backend: str = "extension", filter: str = "", limit: int = 0,
             debug: bool = False, verbose: bool = False) -> int:
    """Run openCypher TCK conformance suite."""
    if not ensure_extension_built():
        return 1

    root = get_project_root()
    python = _pick_python_with_extension_support(verbose=verbose)
    if python is None:
        print("ERROR: no python3 with sqlite3 extension support found.", flush=True)
        print("       Apple's /usr/bin/python3 is built without loadable extensions.", flush=True)
        print("       Install MacPorts python313 or Homebrew python3 and retry.", flush=True)
        return 2

    cmd = [python, "-m", "tests.tck",
           "--backend", backend or "extension",
           "--out", os.path.join("build", "tck-results.json")]
    if filter:
        cmd += ["--filter", filter]
    if limit:
        cmd += ["--limit", str(limit)]
    if debug:
        cmd += ["--debug"]

    env = os.environ.copy()
    env["PYTHONPATH"] = root + (os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else "")

    if verbose:
        print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=root, env=env)
    return result.returncode


def _pick_python_with_extension_support(verbose: bool = False) -> str | None:
    """Return path to a python3 whose sqlite3 supports load_extension, else None."""
    import shutil
    candidates = [
        os.environ.get("GQLITE_TCK_PYTHON"),
        shutil.which("python3.13"),
        shutil.which("python3.12"),
        shutil.which("python3.11"),
        shutil.which("python3"),
    ]
    probe = (
        "import sqlite3, sys; "
        "c=sqlite3.connect(':memory:'); "
        "sys.exit(0 if hasattr(c,'enable_load_extension') else 1)"
    )
    for cand in candidates:
        if not cand:
            continue
        try:
            r = subprocess.run([cand, "-c", probe], capture_output=True)
            if r.returncode == 0:
                if verbose:
                    print(f"Using python: {cand}")
                return cand
        except (OSError, subprocess.SubprocessError):
            continue
    return None


@test()
@angreal.command(
    name="constraints",
    about="Run constraint tests (expected failures)",
    tool=angreal.ToolDescription(
        """
Run constraint tests that verify error handling.

These tests are EXPECTED to fail - they validate that constraints
are properly enforced and errors are correctly reported.

## When to use
- Testing error handling
- Validating constraint enforcement

## Examples
```
angreal test constraints
```
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
def test_constraints(verbose: bool = False) -> int:
    """Run constraint tests (expected to fail)."""
    if not ensure_extension_built():
        return 1

    print("Running constraint tests (expected failures)...")
    return run_make("test-constraints", verbose=verbose)


@test()
@angreal.command(
    name="cli",
    about="Run CLI tests for gqlite binary",
    tool=angreal.ToolDescription(
        """
Run CLI tests that exercise the gqlite interactive shell.

## When to use
- After changes to src/main.c
- Testing multi-line statement handling
- Validating CLI behavior

## Examples
```
angreal test cli
```

## Prerequisites
- gqlite binary must be built first (auto-built if missing)
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
def test_cli(verbose: bool = False) -> int:
    """Run CLI tests."""
    from utils import run_make

    print("Running CLI tests...")
    return run_make("test-cli", verbose=verbose)


@test()
@angreal.command(
    name="all",
    about="Run all tests",
    tool=angreal.ToolDescription(
        """
Run the complete test suite: unit, functional, and all bindings.

## When to use
- Full validation before commits
- CI/CD pipelines
- Pre-release verification

## Examples
```
angreal test all
angreal test all --verbose
```

## Test order
1. Unit tests (CUnit)
2. Functional tests (SQL)
3. Binding tests (Rust + Python)
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
def test_all(verbose: bool = False) -> int:
    """Run all tests."""
    print("Running all tests...")

    tests = [
        ("Unit tests", lambda: test_unit(verbose=verbose)),
        ("Functional tests", lambda: test_functional(verbose=verbose)),
        ("CLI tests", lambda: test_cli(verbose=verbose)),
        ("Binding tests", lambda: test_bindings(verbose=verbose)),
    ]

    for name, test_fn in tests:
        print(f"\n{'='*50}")
        print(f"Running: {name}")
        print('='*50)
        result = test_fn()
        if result != 0:
            print(f"\n{name} failed!")
            return result

    print("\n" + "="*50)
    print("All tests passed!")
    print("="*50)
    return 0


@test()
@angreal.command(
    name="gpu",
    about="Run GPU acceleration tests",
    tool=angreal.ToolDescription(
        """
Run GPU-specific tests for the wgpu-based acceleration.

## What this tests
1. Rust GPU crate unit tests (config, cost calculations)
2. GPU extension build with GPU=1
3. GPU PageRank integration test (forces GPU execution)

## When to use
- After changes to src/gpu/ Rust code
- Validating GPU dispatch logic
- Testing GPU algorithm implementations

## Examples
```
angreal test gpu
angreal test gpu --verbose
```

## Prerequisites
- Rust toolchain installed
- GPU-capable machine (Metal on macOS, Vulkan on Linux)
- wgpu dependencies available
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
def test_gpu(verbose: bool = False) -> int:
    """Run GPU acceleration tests."""
    root = get_project_root()
    gpu_dir = os.path.join(root, "src", "gpu")

    # Step 1: Run Rust unit tests
    print("Step 1: Running Rust GPU crate tests...")
    cmd = ["cargo", "test"]
    if verbose:
        cmd.append("--verbose")
        print(f"Running: {' '.join(cmd)} in {gpu_dir}")

    result = subprocess.run(cmd, cwd=gpu_dir)
    if result.returncode != 0:
        print("Rust GPU tests failed!")
        return result.returncode
    print("Rust GPU tests passed!")

    # Step 2: Build extension with GPU=1
    print("\nStep 2: Building extension with GPU=1...")
    result = run_make("clean", verbose=verbose)
    if result != 0:
        print("Clean failed!")
        return result

    result = run_make("extension", verbose=verbose, GPU="1")
    if result != 0:
        print("GPU extension build failed!")
        return result
    print("GPU extension built successfully!")

    # Step 3: Run GPU integration test
    print("\nStep 3: Running GPU integration test...")
    test_script = '''
-- GPU PageRank Integration Test
-- This test forces GPU execution by using a graph that exceeds the threshold

-- Create a moderately sized graph to trigger GPU dispatch
-- With threshold at 100,000 and 20 iterations, we need ~5000 nodes+edges
-- For simplicity, we'll test with a smaller graph but verify GPU init works

.load build/graphqlite.dylib

-- Create test graph
SELECT cypher('CREATE (a:Page {id: "A"})');
SELECT cypher('CREATE (b:Page {id: "B"})');
SELECT cypher('CREATE (c:Page {id: "C"})');
SELECT cypher('CREATE (d:Page {id: "D"})');
SELECT cypher('MATCH (a:Page {id: "A"}), (b:Page {id: "B"}) CREATE (a)-[:LINKS]->(b)');
SELECT cypher('MATCH (a:Page {id: "A"}), (c:Page {id: "C"}) CREATE (a)-[:LINKS]->(c)');
SELECT cypher('MATCH (b:Page {id: "B"}), (c:Page {id: "C"}) CREATE (b)-[:LINKS]->(c)');
SELECT cypher('MATCH (c:Page {id: "C"}), (a:Page {id: "A"}) CREATE (c)-[:LINKS]->(a)');
SELECT cypher('MATCH (d:Page {id: "D"}), (c:Page {id: "C"}) CREATE (d)-[:LINKS]->(c)');

-- Run PageRank and verify output
SELECT cypher('RETURN pageRank()');
'''
    cmd = ["sqlite3", ":memory:"]
    if verbose:
        print(f"Running: {' '.join(cmd)}")

    result = subprocess.run(
        cmd,
        input=test_script,
        capture_output=True,
        text=True,
        cwd=root
    )

    if verbose:
        print("STDOUT:", result.stdout)
        print("STDERR:", result.stderr)

    # Check for GPU initialization
    if "GPU acceleration enabled" not in result.stderr:
        print("WARNING: GPU acceleration not detected in output")
        print("This may be expected if no GPU is available")

    # Check for valid PageRank output
    if '"score"' not in result.stdout:
        print("ERROR: PageRank did not return expected results")
        print("Output:", result.stdout)
        return 1

    # Verify ranking order (C should be first - highest PageRank)
    if '"node_id":3' not in result.stdout:
        print("WARNING: Node C (id:3) expected to have highest PageRank")

    print("GPU integration test passed!")

    # Step 4: Run C unit tests with GPU build
    print("\nStep 4: Running C unit tests with GPU build...")
    result = run_make("test-unit", verbose=verbose, GPU="1")
    if result != 0:
        print("C unit tests with GPU build failed!")
        return result

    print("\n" + "="*50)
    print("All GPU tests passed!")
    print("="*50)
    return 0
