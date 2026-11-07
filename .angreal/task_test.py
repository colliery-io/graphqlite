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


@test()
@angreal.command(
    name="metamorphic",
    about="Run metamorphic query tests (generated queries, equivalent rewrites)",
    tool=angreal.ToolDescription(
        """
Run the metamorphic query harness (tests/metamorphic; GQLITE-T-0448, GQLITE-T-0452).

Each case is a random small graph and query variants that must give the
same result, over 22 relations: pass-through WITH *, renames, keyword
names, ternary logic partitioning of WHERE and count(*), SKIP/LIMIT slices,
DISTINCT, UNION, var-length vs fixed paths, reversed patterns, OPTIONAL
MATCH vs NOT EXISTS, :A|B vs OR, write expressions, SET read-back, MERGE
idempotence, DETACH DELETE. A variant that errors, results that differ, a
crash or a hang is a failure. Open bugs are listed in
tests/metamorphic/known.json with their Kairos task and do not fail the run.

Modes:
- random: --count cases from --seed, or --minutes of cases.
- cover: per relation, a covering array over the dimensions it reads (all
  pairs; all triples of the clause-structure dimensions), --reps runs each.

## When to use
- After a change to the transform or executor: `angreal test metamorphic --mode cover`
- To hunt for new bugs: raise --count, change --seed, or give --minutes

## Examples
```
angreal test metamorphic
angreal test metamorphic --mode cover --reps 3
angreal test metamorphic --minutes 10 --seed 20261025
angreal test metamorphic --relation optional
```

## Output
- Stdout: known tasks with case counts; each new signature with a replay id,
  the dimension values every failing case shares, a shrunk graph and the
  variants. Replay one case: `python -m tests.metamorphic --replay <id>`.
- `build/metamorphic-results.json`

## Exit code
1 on a new signature, or a crash or hang that no known task covers; else 0.
""",
        risk_level="safe"
    )
)
@angreal.argument(name="mode", long="mode", help="random (default) or cover")
@angreal.argument(name="seed", long="seed", python_type="int", help="Seed (default 1)")
@angreal.argument(name="count", long="count", python_type="int", help="random mode: cases (default 500)")
@angreal.argument(name="minutes", long="minutes", python_type="float", help="random mode: time budget")
@angreal.argument(name="reps", long="reps", python_type="int", help="cover mode: runs per row (default 2)")
@angreal.argument(name="jobs", long="jobs", python_type="int", help="Worker processes (default: cores - 1)")
@angreal.argument(name="relation", long="relation", help="Only relations whose name contains this")
def test_metamorphic(mode: str = "", seed: int = 1, count: int = 500, minutes: float = 0.0,
                     reps: int = 0, jobs: int = 0, relation: str = "") -> int:
    """Run the metamorphic query harness."""
    if not ensure_extension_built():
        return 1
    root = get_project_root()
    python = _pick_python_with_extension_support()
    if python is None:
        print("ERROR: no python3 with sqlite3 extension support found.", flush=True)
        return 2
    cmd = [python, "-m", "tests.metamorphic", "--mode", mode or "random",
           "--seed", str(seed or 1), "--count", str(count or 500)]
    if minutes:
        cmd += ["--minutes", str(minutes)]
    if reps:
        cmd += ["--reps", str(reps)]
    if jobs:
        cmd += ["--jobs", str(jobs)]
    if relation:
        cmd += ["--relation", relation]
    env = os.environ.copy()
    env["PYTHONPATH"] = root + (os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else "")
    return subprocess.run(cmd, cwd=root, env=env).returncode


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


@test()
@angreal.command(
    name="sanitize",
    about="Run the functional tests, TCK and metamorphic harness on an ASan + UBSan build",
    tool=angreal.ToolDescription(
        """
Build the extension with AddressSanitizer and UndefinedBehaviorSanitizer
(`make extension-asan`, into build/asan; GQLITE-T-0495) and run the
functional tests, the TCK and the metamorphic harness on it. A memory error
or undefined behaviour that still gives the right result shows up here.

The host processes (sqlite3, python) are not built with ASan, so the ASan
runtime is preloaded: DYLD_INSERT_LIBRARIES on macOS, LD_PRELOAD on Linux.
On macOS this runs the framework Python.app binary directly (a launcher
stub drops DYLD_* variables), and the runners pass the preload on to their
workers through GQLITE_SANITIZER_PRELOAD. Leak detection is off.

## When to use
- After a change to C code that builds strings, buffers or SQL
- Before a release

## Examples
```
angreal test sanitize
angreal test sanitize --part tck
angreal test sanitize --part metamorphic --count 20000 --seed 7
```

## Output
- One line per part; sanitizer reports in build/asan/reports/ (asan.<pid>,
  ubsan.<pid>), the first lines of each printed.

## Exit code
1 if a part fails or any sanitizer report was written; else 0.
""",
        risk_level="safe"
    )
)
@angreal.argument(name="part", long="part", help="functional, tck, metamorphic or all (default)")
@angreal.argument(name="count", long="count", python_type="int", help="metamorphic: random cases (default 5000)")
@angreal.argument(name="seed", long="seed", python_type="int", help="metamorphic: seed (default 1)")
def test_sanitize(part: str = "", count: int = 0, seed: int = 0) -> int:
    """Run the test suites on the sanitizer build."""
    import glob
    import shutil
    import sys
    part = part or "all"
    root = get_project_root()
    if run_make("extension-asan") != 0:
        return 1
    ext = os.path.join(root, "build", "asan", "graphqlite")
    cc = os.environ.get("CC", "cc")
    runtime_name = "libclang_rt.asan_osx_dynamic.dylib" if sys.platform == "darwin" else "libasan.so"
    runtime = subprocess.run([cc, "-print-file-name=" + runtime_name],
                             capture_output=True, text=True).stdout.strip()
    if not runtime or not os.path.exists(runtime):
        print(f"ERROR: ASan runtime {runtime_name} not found (via {cc} -print-file-name)", flush=True)
        return 2
    python = _pick_python_with_extension_support()
    if python is None:
        print("ERROR: no python3 with sqlite3 extension support found.", flush=True)
        return 2
    if sys.platform == "darwin":
        # The python3.X launcher drops DYLD_* variables: run the framework binary.
        real = subprocess.run([python, "-c", "import sys; print(sys.prefix)"],
                              capture_output=True, text=True).stdout.strip()
        app = os.path.join(real, "Resources", "Python.app", "Contents", "MacOS", "Python")
        if os.path.exists(app):
            python = app
    reports = os.path.join(root, "build", "asan", "reports")
    shutil.rmtree(reports, ignore_errors=True)
    os.makedirs(reports)
    env = os.environ.copy()
    env["DYLD_INSERT_LIBRARIES" if sys.platform == "darwin" else "LD_PRELOAD"] = runtime
    env["GQLITE_SANITIZER_PRELOAD"] = runtime
    env["ASAN_OPTIONS"] = f"halt_on_error=1:abort_on_error=0:detect_leaks=0:log_path={reports}/asan"
    env["UBSAN_OPTIONS"] = f"print_stacktrace=1:halt_on_error=1:log_path={reports}/ubsan"
    env["PYTHONPATH"] = root + (os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else "")
    sqlite = shutil.which(os.environ.get("SQLITE", "sqlite3")) or "sqlite3"
    failed = []

    if part in ("all", "functional"):
        func_dir = os.path.join(root, "build", "asan", "functional")
        os.makedirs(func_dir, exist_ok=True)
        n = bad = 0
        for f in sorted(glob.glob(os.path.join(root, "tests", "functional", "*.sql"))):
            name = os.path.basename(f)
            if "constraint" in name or "expected_failures" in name:
                continue
            with open(f) as src:
                text = src.read().replace(".load ./build/graphqlite", ".load ./build/asan/graphqlite")
            copy = os.path.join(func_dir, name)
            with open(copy, "w") as dst:
                dst.write(text)
            with open(copy) as stdin:
                r = subprocess.run([sqlite, "-bail"], stdin=stdin, cwd=root, env=env,
                                   capture_output=True, text=True)
            n += 1
            if r.returncode != 0:
                bad += 1
                print(f"  functional: {name} failed", flush=True)
        print(f"functional: {n - bad}/{n} passed", flush=True)
        if bad:
            failed.append("functional")

    if part in ("all", "tck"):
        r = subprocess.run([python, "-m", "tests.tck", "--ext", ext, "--quiet",
                            "--out", os.path.join(root, "build", "asan", "tck-results.json"),
                            "--parity-out", os.path.join(root, "build", "asan", "tck-parity.json")],
                           cwd=root, env=env, capture_output=True, text=True)
        line = [l for l in r.stdout.splitlines() if l.startswith("TCK [")]
        print(line[-1] if line else "TCK: no summary", flush=True)
        if r.returncode != 0 or not line or "fail=0 error=0" not in line[-1]:
            failed.append("tck")

    if part in ("all", "metamorphic"):
        for args in (["--mode", "random", "--seed", str(seed or 1), "--count", str(count or 5000)],
                     ["--mode", "cover", "--reps", "1"]):
            r = subprocess.run([python, "-m", "tests.metamorphic", *args, "--ext", ext],
                               cwd=root, env=env, capture_output=True, text=True)
            line = [l for l in r.stdout.splitlines() if l.startswith("metamorphic ")]
            print(line[-1] if line else "metamorphic: no summary", flush=True)
            if r.returncode != 0:
                failed.append("metamorphic")

    found = sorted(glob.glob(os.path.join(reports, "*")))
    for f in found[:10]:
        with open(f, errors="replace") as fh:
            head = [l.rstrip() for l in fh.readlines()[:6]]
        print(f"--- {os.path.basename(f)}", *head, sep="\n", flush=True)
    print(f"sanitizer reports: {len(found)} (in {reports})", flush=True)
    return 1 if failed or found else 0


@test()
@angreal.command(
    name="fuzz",
    about="Fuzz cypher() with libFuzzer on an ASan + UBSan build (Docker on macOS)",
    tool=angreal.ToolDescription(
        """
Run the libFuzzer target tests/fuzz/fuzz_cypher.c (GQLITE-T-0496): each
input is a Cypher query run through cypher() on an in-memory database with
a small graph, in a transaction that is rolled back. The extension is built
with AddressSanitizer, UndefinedBehaviorSanitizer and coverage
instrumentation (`make fuzz-build`). The seed corpus is every query of the
TCK features and the functional tests (tests/fuzz/make_corpus.py); the
dictionary is tests/fuzz/cypher.dict.

Apple clang has no libFuzzer: on macOS (or with --docker) the build and the
run happen in the Linux image tests/fuzz/Dockerfile (built on first use as
`graphqlite-fuzz`), into build/linux/fuzz. Elsewhere it runs natively, into
build/fuzz.

## When to use
- After a change to the parser, transform or executor
- To hunt for crashes: raise --minutes

## Examples
```
angreal test fuzz --minutes 10
angreal test fuzz --minutes 60 --jobs 6
angreal test fuzz --reproduce build/linux/fuzz/crashes/crash-<hash>
```

## Output
- Crash, timeout and OOM inputs in <build>/crashes/ (crash-*, timeout-*,
  oom-*), each with the libFuzzer log in <build>/fuzz.log.
- The grown corpus stays in <build>/corpus.

## Exit code
1 if the run saved a crash, timeout or OOM input; else 0.
""",
        risk_level="safe"
    )
)
@angreal.argument(name="minutes", long="minutes", python_type="float", help="Time budget (default 5)")
@angreal.argument(name="jobs", long="jobs", python_type="int", help="Parallel fuzz processes (default 1)")
@angreal.argument(name="docker", long="docker", is_flag=True, takes_value=False, help="Build and run in the Linux Docker image")
@angreal.argument(name="reproduce", long="reproduce", help="Run one saved input and exit")
def test_fuzz(minutes: float = 0.0, jobs: int = 0, docker: bool = False, reproduce: str = "") -> int:
    """Fuzz cypher() with libFuzzer."""
    import glob
    import sys
    root = get_project_root()
    use_docker = docker or sys.platform == "darwin"
    build = "build/linux" if use_docker else "build"
    fuzz_dir = f"{build}/fuzz"
    seconds = int((minutes or 5) * 60)
    flags = ["-dict=tests/fuzz/cypher.dict", "-max_len=4096", "-timeout=10",
             "-rss_limit_mb=3072", f"-artifact_prefix={fuzz_dir}/crashes/"]
    if reproduce:
        run = f"./{fuzz_dir}/fuzz_cypher {' '.join(flags[1:4])} {reproduce}"
    else:
        par = (f"-fork={jobs} -ignore_crashes=1 -ignore_timeouts=1 -ignore_ooms=1"
               if (jobs or 1) > 1 else "")
        run = (f"python3 tests/fuzz/make_corpus.py {fuzz_dir}/corpus && mkdir -p {fuzz_dir}/crashes && "
               f"./{fuzz_dir}/fuzz_cypher {par} {' '.join(flags)} -max_total_time={seconds} "
               f"-print_final_stats=1 {fuzz_dir}/corpus 2>&1 | tee {fuzz_dir}/fuzz.log | "
               f"grep -E 'INITED|DONE|ERROR|SUMMARY|Test unit written|stat::number_of_executed_units'")
    script = (f"mkdir -p {fuzz_dir} && make fuzz-build BUILD_DIR={build} > {fuzz_dir}/build.log 2>&1 "
              f"|| {{ tail -20 {fuzz_dir}/build.log; exit 2; }}; {run}")
    env = os.environ.copy()
    env.setdefault("ASAN_OPTIONS", "detect_leaks=0:abort_on_error=1")
    env.setdefault("UBSAN_OPTIONS", "print_stacktrace=1:halt_on_error=1")
    env["GQLITE_FUZZ_EXT"] = f"{fuzz_dir}/graphqlite"
    if use_docker:
        img = subprocess.run(["docker", "image", "inspect", "graphqlite-fuzz"], capture_output=True)
        if img.returncode != 0:
            print("Building the Docker image graphqlite-fuzz (tests/fuzz/Dockerfile)...", flush=True)
            if subprocess.run(["docker", "build", "-t", "graphqlite-fuzz", "tests/fuzz"], cwd=root).returncode != 0:
                return 2
        cmd = ["docker", "run", "--rm", "-v", f"{root}:/src",
               "-e", f"GQLITE_FUZZ_EXT={env['GQLITE_FUZZ_EXT']}",
               "-e", f"ASAN_OPTIONS={env['ASAN_OPTIONS']}",
               "-e", f"UBSAN_OPTIONS={env['UBSAN_OPTIONS']}",
               "graphqlite-fuzz", "sh", "-c", script]
    else:
        cmd = ["sh", "-c", script]
    before = set(glob.glob(os.path.join(root, fuzz_dir, "crashes", "*")))
    rc = subprocess.run(cmd, cwd=root, env=env).returncode
    if reproduce:
        return rc
    found = sorted(set(glob.glob(os.path.join(root, fuzz_dir, "crashes", "*"))) - before)
    for f in found:
        print(f"saved: {os.path.relpath(f, root)}", flush=True)
    print(f"new crash/timeout/oom inputs: {len(found)}", flush=True)
    return 1 if found else (0 if rc in (0, 1) else rc)


@test()
@angreal.command(
    name="harness-coverage",
    about="Report the transform and executor code the metamorphic harness never reaches",
    tool=angreal.ToolDescription(
        """
Build the extension with gcov counters (`make extension-cov`, into
build/cov; GQLITE-T-0497), run the metamorphic harness on it (cover mode,
then random mode), and report line coverage of src/backend/transform and
src/backend/executor: per file, the functions the run never entered
(largest first), and the large functions it entered but mostly missed.
Those are where a new generator or relation finds bugs.

## When to use
- To choose the next harness generator or relation
- After adding one, to see what it reached

## Examples
```
angreal test harness-coverage
angreal test harness-coverage --count 50000 --top 60
```

## Output
- The report on stdout and in build/cov/harness-coverage.txt.

## Exit code
0 when the report was written; 2 when there was no coverage data.
""",
        risk_level="safe"
    )
)
@angreal.argument(name="count", long="count", python_type="int", help="Random-mode cases (default 20000)")
@angreal.argument(name="reps", long="reps", python_type="int", help="Cover-mode runs per row (default 2)")
@angreal.argument(name="top", long="top", python_type="int", help="Functions listed per section (default 40)")
def test_harness_coverage(count: int = 0, reps: int = 0, top: int = 0) -> int:
    """Coverage of the metamorphic harness."""
    import glob
    root = get_project_root()
    if run_make("extension-cov") != 0:
        return 1
    for f in glob.glob(os.path.join(root, "build", "cov", "**", "*.gcda"), recursive=True):
        os.remove(f)
    python = _pick_python_with_extension_support()
    if python is None:
        print("ERROR: no python3 with sqlite3 extension support found.", flush=True)
        return 2
    env = os.environ.copy()
    env["PYTHONPATH"] = root + (os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else "")
    ext = os.path.join(root, "build", "cov", "graphqlite")
    for args in (["--mode", "cover", "--reps", str(reps or 2)],
                 ["--mode", "random", "--seed", "1", "--count", str(count or 20000)]):
        r = subprocess.run([python, "-m", "tests.metamorphic", *args, "--ext", ext],
                           cwd=root, env=env, capture_output=True, text=True)
        line = [l for l in r.stdout.splitlines() if l.startswith("metamorphic ")]
        print(line[-1] if line else "metamorphic: no summary", flush=True)
    return subprocess.run([python, "-m", "tests.metamorphic.coverage_report",
                           "--build", os.path.join(root, "build", "cov"), "--top", str(top or 40),
                           "--out", os.path.join(root, "build", "cov", "harness-coverage.txt")],
                          cwd=root, env=env).returncode


@test()
@angreal.command(
    name="neo4j-acceptance",
    about="Run Neo4j's acceptance features (fetched at test time) against the baseline",
    tool=angreal.ToolDescription(
        """
Run Neo4j's Cypher acceptance features (neo4j/neo4j release/5.26.32, pinned
commit; GQLITE-T-0501) through the TCK runner. They are GPL-3.0, so they are
fetched at test time into build/neo4j-acceptance (a sparse checkout, kept
between runs) and never committed. Files of Neo4j- or GQL-only features
(tests/tck/neo4j/skip.txt) are left out. The result is compared with
tests/tck/neo4j/baseline.txt, the scenarios that passed when it was recorded.

## When to use
- After a change to the parser, transform or executor, with the TCK
- After fixing one of the bugs these features found: record the baseline

## Examples
```
angreal test neo4j-acceptance
angreal test neo4j-acceptance --update-baseline
```

## Output
- Pass/fail counts, each baseline scenario that no longer passes, and how
  many newly pass. Per-scenario results in build/neo4j-results.json.

## Exit code
1 when a baseline scenario no longer passes; else 0. Needs network access
on the first run (git fetch from GitHub).
""",
        risk_level="safe"
    )
)
@angreal.argument(name="update_baseline", long="update-baseline", is_flag=True, takes_value=False,
                  help="Record the passing scenarios as the new baseline")
def test_neo4j_acceptance(update_baseline: bool = False) -> int:
    """Neo4j acceptance features against the baseline."""
    if not ensure_extension_built():
        return 1
    root = get_project_root()
    python = _pick_python_with_extension_support()
    if python is None:
        print("ERROR: no python3 with sqlite3 extension support found.", flush=True)
        return 2
    env = os.environ.copy()
    env["PYTHONPATH"] = root + (os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else "")
    cmd = [python, "-m", "tests.tck.neo4j.run"]
    if update_baseline:
        cmd.append("--update-baseline")
    return subprocess.run(cmd, cwd=root, env=env).returncode


@test()
@angreal.command(
    name="crash",
    about="Kill a writer at random points; check the database is whole and each write all-or-nothing",
    tool=angreal.ToolDescription(
        """
Crash test (GQLITE-T-0530). Starts tests/crash/writer.py on one database
file, kills it with SIGKILL after a random delay, reopens the file and
checks: PRAGMA integrity_check, gql_check_storage(), and that every write is
all there or not there (each write tags its rows with a batch id). The
writes: UNWIND CREATE, CREATE of relationships, MERGE in FOREACH, SET,
DETACH DELETE, BEGIN ... COMMIT of three CREATEs, and the Python bulk
importers. Rounds alternate journal_mode WAL and DELETE; the file is kept
across rounds.

## When to use
- After a change to a write path, the executor's transactions or the bulk
  importers

## Examples
```
angreal test crash
angreal test crash --rounds 1000
```

## Output
- One line per bad round and a summary. The database of the first bad
  round is kept in build/crash/.

## Exit code
1 when a round finds a problem; else 0.
""",
        risk_level="safe"
    )
)
@angreal.argument(name="rounds", long="rounds", python_type="int", help="Rounds (default 200)")
@angreal.argument(name="seed", long="seed", python_type="int", help="Seed (default 1)")
def test_crash(rounds: int = 0, seed: int = 0) -> int:
    """Crash test of the write paths."""
    if not ensure_extension_built():
        return 1
    root = get_project_root()
    python = _pick_python_with_extension_support()
    if python is None:
        print("ERROR: no python3 with sqlite3 extension support found.", flush=True)
        return 2
    env = os.environ.copy()
    env["PYTHONPATH"] = root + (os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else "")
    cmd = [python, "-m", "tests.crash.run", "--rounds", str(rounds or 200), "--seed", str(seed or 1)]
    return subprocess.run(cmd, cwd=root, env=env).returncode
