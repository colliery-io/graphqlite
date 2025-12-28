# Installation

## Python (Recommended)

```bash
pip install graphqlite
```

This installs pre-built binaries for:
- Linux (x86_64)
- macOS (arm64, x86_64)
- Windows (x86_64)

## Rust

Add to your `Cargo.toml`:

```toml
[dependencies]
graphqlite = "0.1"
```

## From Source

Building from source requires:
- GCC or Clang
- Bison (3.0+)
- Flex
- SQLite development headers

### macOS

```bash
brew install bison flex sqlite
export PATH="$(brew --prefix bison)/bin:$PATH"
make extension RELEASE=1
```

### Linux (Debian/Ubuntu)

```bash
sudo apt-get install build-essential bison flex libsqlite3-dev
make extension RELEASE=1
```

### Windows (MSYS2)

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-sqlite3 bison flex make
make extension RELEASE=1
```

The extension will be built to:
- `build/graphqlite.dylib` (macOS)
- `build/graphqlite.so` (Linux)
- `build/graphqlite.dll` (Windows)

## Verifying Installation

### Python

```python
import graphqlite
print(graphqlite.__version__)

# Quick test
from graphqlite import Graph
g = Graph(":memory:")
g.upsert_node("test", {"name": "Test"})
print(g.stats())  # {'nodes': 1, 'edges': 0}
```

### SQL

```bash
sqlite3
.load /path/to/graphqlite
SELECT cypher('RETURN 1 + 1 AS result');
```

## Troubleshooting

### Extension not found

If you get `FileNotFoundError: GraphQLite extension not found`:

1. Build the extension: `make extension RELEASE=1`
2. Set the path explicitly:
   ```python
   from graphqlite import connect
   conn = connect("graph.db", extension_path="/path/to/graphqlite.dylib")
   ```
3. Or set an environment variable:
   ```bash
   export GRAPHQLITE_EXTENSION_PATH=/path/to/graphqlite.dylib
   ```

### macOS: Library not loaded

If you see errors about missing SQLite libraries, ensure you're using Homebrew's Python or set `DYLD_LIBRARY_PATH`:

```bash
export DYLD_LIBRARY_PATH="$(brew --prefix sqlite)/lib:$DYLD_LIBRARY_PATH"
```
