---
title: Installation
slug: 0.9.2/how-to/installation
---

GraphQLite is a SQLite extension that adds Cypher graph query support. It can be used from Python, Rust, the command line, or any language that can load SQLite extensions.

## Python (Recommended)

Install from PyPI:

```bash
pip install graphqlite
```

This installs pre-built binaries for:

- macOS (arm64, x86_64)
- Linux (x86_64, aarch64)
- Windows (x86_64)

### Optional Dependencies

The Leiden community detection algorithm requires `graspologic`:

```bash
pip install graphqlite[leiden]
```

For rustworkx-powered graph export:

```bash
pip install graphqlite[rustworkx]
```

Install everything:

```bash
pip install graphqlite[leiden,rustworkx]
```

## Rust

Add to your `Cargo.toml`:

```toml
[dependencies]
graphqlite = "0.3"
```

The `bundled-extension` feature is enabled by default. It bundles the compiled extension directly into your binary so no separate `.so`/`.dylib` file is needed at runtime:

```toml
[dependencies]
graphqlite = { version = "0.3", features = ["bundled-extension"] }
```

To use an external extension at runtime instead, disable the default features:

```toml
[dependencies]
graphqlite = { version = "0.3", default-features = false }
```

## From Source

Building from source produces the raw extension file you can load into any SQLite environment.

### Prerequisites

| Tool | Minimum Version | Purpose |
|------|----------------|---------|
| GCC or Clang | 9+ | C compiler |
| Bison | 3.0+ | Parser generator |
| Flex | 2.6+ | Lexer generator |
| SQLite development headers | 3.35+ | SQLite API |
| CUnit | 2.1+ | Unit tests (optional) |

### macOS

```bash
brew install bison flex sqlite cunit
export PATH="$(brew --prefix bison)/bin:$PATH"
git clone https://github.com/colliery-io/graphqlite
cd graphqlite
make extension
```

Homebrew installs a newer Bison than the one shipped with macOS. The `PATH` export ensures the build system picks up the correct version.

### Linux (Debian/Ubuntu)

```bash
sudo apt-get install build-essential bison flex libsqlite3-dev libcunit1-dev
git clone https://github.com/colliery-io/graphqlite
cd graphqlite
make extension
```

### Windows (MSYS2)

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-sqlite3 bison flex make
git clone https://github.com/colliery-io/graphqlite
cd graphqlite
make extension
```

### Debug vs. Release Build

The default build includes debug symbols and assertions. For a release build:

```bash
make extension RELEASE=1
```

Release builds enable `-O2` optimization and strip assertions. Use them for production deployments.

### Output Files

After building, the extension appears in `build/`:

| Platform | File |
|----------|------|
| macOS | `build/graphqlite.dylib` |
| Linux | `build/graphqlite.so` |
| Windows | `build/graphqlite.dll` |

See [Building from Source](../building-from-source/) for the full list of build targets and how to run tests.

## Verifying Installation

### Python

```python
import graphqlite

# Check version
print(graphqlite.__version__)

# Quick smoke test
from graphqlite import Graph

g = Graph(":memory:")
g.upsert_node("alice", {"name": "Alice", "age": 30}, label="Person")
g.upsert_node("bob", {"name": "Bob", "age": 25}, label="Person")
g.upsert_edge("alice", "bob", {"since": 2020}, rel_type="KNOWS")

print(g.stats())          # {'nodes': 2, 'edges': 1}
print(g.query("MATCH (n:Person) RETURN n.name ORDER BY n.name"))
```

### Rust

```rust
use graphqlite::Connection;

fn main() -> graphqlite::Result<()> {
    let conn = Connection::open_in_memory()?;
    conn.cypher("CREATE (n:Person {name: 'Alice'})")?;
    let rows = conn.cypher("MATCH (n:Person) RETURN n.name")?;
    for row in &rows {
        println!("{}", row.get::<String>("n.name")?);
    }
    Ok(())
}
```

### SQL (raw SQLite)

```bash
sqlite3
.load /path/to/build/graphqlite
SELECT cypher('RETURN 1 + 1 AS result');
SELECT cypher('CREATE (n:Test {value: 42})');
SELECT cypher('MATCH (n:Test) RETURN n.value');
```

## Extension Path Configuration

When GraphQLite cannot find the extension automatically, specify the path explicitly.

### Environment Variable

Set `GRAPHQLITE_EXTENSION_PATH` before running your application:

```bash
export GRAPHQLITE_EXTENSION_PATH=/path/to/build/graphqlite.dylib
python my_app.py
```

This works for Python, the CLI (`gqlite`), and any program that reads environment variables before loading the extension.

### Python: Explicit Path

```python
from graphqlite import connect, Graph, GraphManager

# connect()
conn = connect("graph.db", extension_path="/path/to/graphqlite.dylib")

# Graph()
g = Graph("graph.db", extension_path="/path/to/graphqlite.dylib")

# GraphManager / graphs()
from graphqlite import graphs
with graphs("./data", extension_path="/path/to/graphqlite.dylib") as gm:
    social = gm.open_or_create("social")
```

### Finding the Bundled Path

When using the Python package, GraphQLite ships a bundled extension. Get its path:

```python
import graphqlite

path = graphqlite.loadable_path()
print(path)  # e.g. /usr/local/lib/python3.12/site-packages/graphqlite/graphqlite.dylib
```

You can pass this path to other tools or languages that need to load the extension manually.

## Platform Notes

### macOS

- The extension is a Mach-O shared library with the `.dylib` suffix.
- If you see `Library not loaded` errors, ensure you are using Homebrew's SQLite rather than the system SQLite:

  ```bash
  export DYLD_LIBRARY_PATH="$(brew --prefix sqlite)/lib:$DYLD_LIBRARY_PATH"
  ```

- Apple Silicon (M1/M2/M3) and Intel builds are separate; PyPI ships a universal wheel containing both.

### Linux

- The extension is an ELF shared object with the `.so` suffix.
- Make sure `libsqlite3` is installed at runtime (not just `libsqlite3-dev`).
- On musl-based systems (Alpine Linux), build from source.

### Windows

- The extension is a DLL with the `.dll` suffix.
- Load it with the full path: `.load C:/path/to/graphqlite`.
- MSYS2 or WSL are the supported build environments.

## Troubleshooting

### Extension not found (Python)

```
FileNotFoundError: GraphQLite extension not found
```

Either the package was installed without wheels for your platform, or the environment variable points to the wrong file. Try:

```python
import graphqlite
print(graphqlite.loadable_path())  # See where Python looks
```

Then set `GRAPHQLITE_EXTENSION_PATH` or pass `extension_path` explicitly.

### Python sqlite3 extension support disabled

Some Python distributions compile `sqlite3` without extension loading support (e.g., certain Docker images):

```
AttributeError: 'sqlite3.Connection' object has no attribute 'enable_load_extension'
```

Rebuild Python from source with `--enable-loadable-sqlite-extensions`, or switch to a distribution that includes it (standard CPython builds from python.org do support it).

### Version mismatch

If you see unexpected query errors after upgrading, ensure the Python package and the loaded extension are from the same version:

```python
import graphqlite
print(graphqlite.__version__)   # Python package version
```

When building from source for use with the Rust crate, run `make install-bundled` after `make extension` to replace the bundled binary in the Rust crate's source tree.

### SQLite version too old

GraphQLite requires SQLite 3.35 or later for JSON function support. Check your SQLite version:

```python
import sqlite3
print(sqlite3.sqlite_version)  # Should be 3.35.0 or higher
```
