---
title: Building from Source
slug: 0.9.3/how-to/building-from-source
---

This guide covers building the GraphQLite SQLite extension and CLI from source, running tests, checking code quality, and installing the result for use with the Rust crate.

## Prerequisites

| Tool | Minimum Version | macOS | Linux (Debian/Ubuntu) | Windows (MSYS2) |
|------|----------------|-------|----------------------|-----------------|
| GCC or Clang | 9 / 11 | Xcode CLI tools | `build-essential` | `mingw-w64-x86_64-gcc` |
| Bison | 3.0+ | `brew install bison` | `bison` | `bison` |
| Flex | 2.6+ | `brew install flex` | `flex` | `flex` |
| SQLite dev headers | 3.35+ | `brew install sqlite` | `libsqlite3-dev` | `mingw-w64-x86_64-sqlite3` |
| CUnit | 2.1+ | `brew install cunit` | `libcunit1-dev` | (optional) |
| make | 4.0+ | Xcode CLI tools | `make` | `make` |

### macOS

```bash
xcode-select --install
brew install bison flex sqlite cunit

# Homebrew Bison must precede the system Bison on PATH
export PATH="$(brew --prefix bison)/bin:$PATH"
```

Add the `PATH` export to your shell profile (`~/.zshrc` or `~/.bash_profile`) to make it persistent.

### Linux (Debian/Ubuntu)

```bash
sudo apt-get update
sudo apt-get install build-essential bison flex libsqlite3-dev libcunit1-dev
```

### Windows (MSYS2)

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-sqlite3 bison flex make
```

Run all commands from the MSYS2 MinGW 64-bit shell.

## Clone and Build

```bash
git clone https://github.com/your-org/graphqlite
cd graphqlite
make extension
```

This produces:

| Platform | Output file |
|----------|-------------|
| macOS | `build/graphqlite.dylib` |
| Linux | `build/graphqlite.so` |
| Windows | `build/graphqlite.dll` |

### Debug vs. Release

The default build includes debug symbols and C assertions. For a production build:

```bash
make extension RELEASE=1
```

`RELEASE=1` adds `-O2` optimization and strips assertions. Always use release builds for benchmarking.

## Build Targets

Run `make help` to see all available targets. The most commonly used ones are:

### Core Targets

| Target | Description |
|--------|-------------|
| `make extension` | Build the SQLite extension (`.dylib`/`.so`/`.dll`) |
| `make extension RELEASE=1` | Build optimized release extension |
| `make graphqlite` | Build the `gqlite` interactive CLI |
| `make graphqlite RELEASE=1` | Build optimized release CLI |
| `make all` | Build everything (extension + CLI) |

### Test Targets

| Target | Description |
|--------|-------------|
| `make test unit` | Run CUnit unit tests (770 tests) |
| `make test functional` | Run SQL-based functional tests |
| `make test python` | Run Python binding tests |
| `make test rust` | Build and run Rust binding tests |
| `make test-all` | Run all test suites |

### Quality Targets

| Target | Description |
|--------|-------------|
| `make lint` | Strict C11 compliance check |
| `make coverage` | Build with coverage instrumentation and run unit tests |
| `make performance` | Run performance benchmarks |
| `make performance-quick` | Quick performance smoke test |

### Install Targets

| Target | Description |
|--------|-------------|
| `make install-bundled` | Copy extension into the Rust crate's source tree |

## Running Tests

### Unit Tests

```bash
make test unit
```

Builds and runs the CUnit test suite. Output shows pass/fail counts per suite. All 770 tests must pass on a clean build.

### Functional Tests

```bash
make test functional
```

Runs SQL script tests against the built extension. Each test file is a `.sql` script in `tests/functional/` that exercises a specific feature:

```bash
# Run a single functional test manually
sqlite3 :memory: < tests/functional/01_create_match.sql
```

### Python Tests

```bash
make test python
```

Requires Python 3.8+ and `pip install graphqlite[dev]` (or install the dev dependencies from `requirements-dev.txt`).

### Rust Tests

```bash
make test rust
```

This target first calls `make install-bundled` to copy the freshly-built extension into the Rust crate, then runs `cargo test`.

### All Tests

```bash
make test-all
```

Runs unit, functional, CLI, and binding tests in sequence. All suites must pass before a release.

## Linting

```bash
make lint
```

The lint target compiles all C source files with strict C11 flags:

```
-std=c11 -Wall -Wextra -Wpedantic -Werror -Wshadow -Wformat=2
```

Zero warnings are permitted (`-Werror`). Fix all lint warnings before submitting changes. The lint target does not link the extension; it only checks the source.

## Coverage

```bash
make coverage
```

Builds the unit test runner with `--coverage` instrumentation (GCC `gcov`/`lcov`) and runs the tests. Coverage data appears in `build/coverage/`. Open `build/coverage/index.html` in a browser to browse line-by-line coverage.

Requirements: `lcov` must be installed (`brew install lcov` / `sudo apt-get install lcov`).

## Performance Benchmarks

```bash
# Full benchmark suite
make performance

# Quick smoke test (faster, fewer iterations)
make performance-quick

# Extended benchmark with larger graphs
make performance-full
```

Benchmark results are printed to stdout. Run with `RELEASE=1` for meaningful numbers:

```bash
make performance RELEASE=1
```

## Installing for Rust Bundled Builds

The Rust crate defaults to the `bundled-extension` feature, which embeds the extension binary at compile time. After building from source, copy the extension into the Rust crate:

```bash
make install-bundled
```

This copies `build/graphqlite.{dylib,so,dll}` into the correct location inside `bindings/rust/` so that `cargo build` picks it up. Run this after every source change when developing the Rust crate.

## Platform-Specific Notes

### macOS

* The system `bison` (in `/usr/bin`) is version 2.x, which is too old. Always install via Homebrew and prepend it to `PATH`.

* If you see `library not loaded: libsqlite3.dylib` when loading the built extension, set `DYLD_LIBRARY_PATH`:

  ```bash
  export DYLD_LIBRARY_PATH="$(brew --prefix sqlite)/lib:$DYLD_LIBRARY_PATH"
  ```

* On Apple Silicon (M1/M2), the Homebrew prefix is `/opt/homebrew` rather than `/usr/local`.

### Linux

* The `libcunit1-dev` package is only needed for unit tests. The extension itself (`make extension`) does not require CUnit.
* On systems without `lcov`, `make coverage` will fail at the report generation step. Install lcov or skip coverage.
* On Alpine Linux (musl libc), replace `apt-get` with `apk add`:

  ```bash
  apk add build-base bison flex sqlite-dev
  ```

### Windows (MSYS2)

* All commands must be run from the **MSYS2 MinGW 64-bit** shell, not the default MSYS shell. The MinGW shell sets the correct compiler paths.
* Python tests require a Windows Python 3.8+ installation accessible from the MSYS2 PATH.
* The extension output is `build/graphqlite.dll`.

## Build Directory Layout

After a full build:

```
build/
├── graphqlite.dylib        # SQLite extension (macOS)
├── gqlite                  # Interactive CLI binary
├── test_runner             # CUnit test runner binary
├── parser/                 # Compiled parser objects
├── transform/              # Compiled transform objects
├── executor/               # Compiled executor objects
└── coverage/               # Coverage HTML report (after make coverage)
```

## Troubleshooting

### `bison: syntax error` during build

The system Bison is too old. Confirm the version:

```bash
bison --version
```

If it shows `2.x`, install Homebrew Bison (`brew install bison`) and prepend it to `PATH`.

### `flex: command not found`

Install flex: `brew install flex` / `sudo apt-get install flex`.

### `cannot open shared object file: libcunit.so`

CUnit is not installed or not on `LD_LIBRARY_PATH`. Install it (`libcunit1-dev`) or run `make extension` (which does not need CUnit) instead of `make test unit`.

### Python tests fail with `No module named graphqlite`

Install the Python package in development mode:

```bash
pip install -e bindings/python/
```

or install it from PyPI and use `GRAPHQLITE_EXTENSION_PATH` to point to your locally built extension:

```bash
export GRAPHQLITE_EXTENSION_PATH=$(pwd)/build/graphqlite.dylib
make test python
```
