"""Development utility commands for GraphQLite.

Common development tasks: cleaning build artifacts, generating
coverage reports, and other maintenance operations.
"""

import os
import shutil
import glob
import subprocess
import angreal
from utils import get_project_root, run_make

dev = angreal.command_group(name="dev", about="Development utilities")


@dev()
@angreal.command(
    name="clean",
    about="Remove build artifacts",
    tool=angreal.ToolDescription(
        """
Remove all build artifacts and generated files.

## When to use
- Clean slate for fresh build
- Clearing stale artifacts after branch switch
- Fixing strange build issues

## What gets removed
- `build/` directory (all compiled objects, extensions, executables)
- `*.gcda`, `*.gcno`, `*.gcov` files (coverage data)

## Examples
```
angreal dev clean            # Actually remove files
angreal dev clean --dry-run  # Show what would be removed
```
""",
        risk_level="safe"
    )
)
@angreal.argument(
    name="dry_run",
    long="dry-run",
    short="n",
    is_flag=True,
    takes_value=False,
    help="Show what would be removed without removing"
)
@angreal.argument(
    name="verbose",
    long="verbose",
    short="v",
    is_flag=True,
    takes_value=False,
    help="Show verbose output"
)
def dev_clean(dry_run: bool = False, verbose: bool = False) -> int:
    """Remove build artifacts."""
    root = get_project_root()

    dirs_to_remove = ["build"]
    patterns_to_remove = ["*.gcda", "*.gcno", "*.gcov"]

    removed_count = 0

    for dir_name in dirs_to_remove:
        dir_path = os.path.join(root, dir_name)
        if os.path.exists(dir_path):
            if dry_run:
                print(f"Would remove: {dir_path}/")
            else:
                if verbose:
                    print(f"Removing: {dir_path}/")
                shutil.rmtree(dir_path)
            removed_count += 1

    for pattern in patterns_to_remove:
        files = glob.glob(os.path.join(root, "**", pattern), recursive=True)
        for file_path in files:
            if dry_run:
                print(f"Would remove: {file_path}")
            else:
                if verbose:
                    print(f"Removing: {file_path}")
                os.remove(file_path)
            removed_count += 1

    if dry_run:
        print(f"\nDry run complete. Would remove {removed_count} items.")
    else:
        print(f"Cleaned {removed_count} items.")

    return 0


@dev()
@angreal.command(
    name="coverage",
    about="Generate code coverage report",
    tool=angreal.ToolDescription(
        """
Run tests with coverage instrumentation and generate a coverage report.

## When to use
- Measuring test coverage
- Finding untested code paths
- Before releases to verify coverage targets

## What happens
1. Builds test runner with coverage flags
2. Runs all unit tests
3. Processes coverage data with gcov
4. Generates summary report

## Examples
```
angreal dev coverage
```

## Output
- Summary table showing coverage % per file
- Detailed `.gcov` files in `build/coverage/`
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
def dev_coverage(verbose: bool = False) -> int:
    """Generate coverage report."""
    print("Running tests with coverage instrumentation...")

    result = run_make("coverage", verbose=verbose)

    if result == 0:
        print("\nCoverage report generated in build/coverage/")

    return result


@dev()
@angreal.command(
    name="setup",
    about="Set up development environment",
    tool=angreal.ToolDescription(
        """
Verify and set up the development environment.

## What it checks
- Required tools: gcc, flex, bison, sqlite3
- Optional tools: CUnit (for tests), Python, Rust
- Build directory structure

## Examples
```
angreal dev setup
```
""",
        risk_level="safe"
    )
)
def dev_setup() -> int:
    """Set up development environment."""
    root = get_project_root()

    print("Checking development environment...\n")

    required = [
        ("gcc", "C compiler"),
        ("flex", "Lexer generator"),
        ("bison", "Parser generator"),
        ("sqlite3", "SQLite CLI"),
    ]

    optional = [
        ("cargo", "Rust toolchain (for Rust bindings)"),
        ("python3", "Python (for Python bindings)"),
        ("pytest", "pytest (for Python tests)"),
    ]

    all_ok = True

    print("Required tools:")
    for tool, desc in required:
        result = subprocess.run(["which", tool], capture_output=True)
        if result.returncode == 0:
            path = result.stdout.decode().strip()
            print(f"  ✓ {tool}: {path}")
        else:
            print(f"  ✗ {tool}: NOT FOUND - {desc}")
            all_ok = False

    print("\nOptional tools:")
    for tool, desc in optional:
        result = subprocess.run(["which", tool], capture_output=True)
        if result.returncode == 0:
            path = result.stdout.decode().strip()
            print(f"  ✓ {tool}: {path}")
        else:
            print(f"  - {tool}: not found - {desc}")

    print("\nCUnit library:")
    cunit_paths = [
        "/opt/local/include/CUnit/CUnit.h",
        "/usr/local/include/CUnit/CUnit.h",
        "/opt/homebrew/include/CUnit/CUnit.h",
    ]
    cunit_found = False
    for path in cunit_paths:
        if os.path.exists(path):
            print(f"  ✓ CUnit: {os.path.dirname(os.path.dirname(path))}")
            cunit_found = True
            break
    if not cunit_found:
        print("  ✗ CUnit: NOT FOUND (needed for unit tests)")
        print("    Install with: brew install cunit (Homebrew) or port install cunit (MacPorts)")

    print("\nBuild directories:")
    result = run_make("dirs")
    if result == 0:
        print("  ✓ Build directories created")
    else:
        print("  ✗ Failed to create build directories")
        all_ok = False

    print()
    if all_ok:
        print("Environment ready for development!")
        return 0
    else:
        print("Some required tools are missing. Please install them.")
        return 1
