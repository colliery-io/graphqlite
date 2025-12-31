"""Testing commands for GraphQLite.

Provides a unified interface for running various test suites:
- Unit tests (CUnit)
- Rust binding tests
- Python binding tests
- Functional SQL tests
- Constraint tests (expected failures)
"""

import angreal
from utils import run_make, ensure_extension_built

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
    default_value="python3.11",
    help="Python interpreter to use (default: python3.11)"
)
def test_python(verbose: bool = False, python: str = "python3.11") -> int:
    """Run Python binding tests."""
    if not ensure_extension_built():
        return 1

    print(f"Running Python binding tests (using {python})...")
    return run_make("test-python", verbose=verbose, PYTHON=python)


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
