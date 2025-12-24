"""GraphQLite - SQLite extension for graph queries using Cypher."""

import platform
from pathlib import Path
from typing import Optional

from .connection import Connection, connect, wrap
from .graph import Graph, graph, escape_string, sanitize_rel_type, CYPHER_RESERVED

__version__ = "0.1.0b2"
__all__ = [
    "Connection", "connect", "wrap", "load", "loadable_path",
    "Graph", "graph", "escape_string", "sanitize_rel_type", "CYPHER_RESERVED"
]


def loadable_path() -> str:
    """
    Return the path to the loadable GraphQLite extension.

    This is useful for loading the extension with sqlite3.Connection.load_extension()
    or apsw.Connection.loadextension().

    Returns:
        Path to the extension file (without file extension for SQLite compatibility)

    Example:
        >>> import sqlite3
        >>> import graphqlite
        >>> conn = sqlite3.connect(":memory:")
        >>> conn.enable_load_extension(True)
        >>> conn.load_extension(graphqlite.loadable_path())
    """
    system = platform.system()

    if system == "Darwin":
        ext_name = "graphqlite.dylib"
    elif system == "Linux":
        ext_name = "graphqlite.so"
    elif system == "Windows":
        ext_name = "graphqlite.dll"
    else:
        raise OSError(f"Unsupported platform: {system}")

    # Look for bundled extension first
    bundled = Path(__file__).parent / ext_name
    if bundled.exists():
        # Return path without extension (SQLite convention)
        return str(bundled.parent / bundled.stem)

    # Fall back to development build
    dev_build = Path(__file__).parent.parent.parent.parent.parent / "build" / ext_name
    if dev_build.exists():
        return str(dev_build.parent / dev_build.stem)

    raise FileNotFoundError(
        f"GraphQLite extension not found. Searched:\n"
        f"  - {bundled}\n"
        f"  - {dev_build}\n"
        f"Build with 'make extension' or install from PyPI."
    )


def load(conn, entry_point: Optional[str] = None) -> None:
    """
    Load the GraphQLite extension into an existing SQLite connection.

    This provides a simple way to add GraphQLite support to any sqlite3 or apsw
    connection, similar to how sqlite-vec works.

    Args:
        conn: A sqlite3.Connection or apsw.Connection object
        entry_point: Optional entry point function name (default: auto-detect)

    Example:
        >>> import sqlite3
        >>> import graphqlite
        >>> conn = sqlite3.connect(":memory:")
        >>> graphqlite.load(conn)
        >>> cursor = conn.execute("SELECT cypher('RETURN 1 AS x')")
        >>> print(cursor.fetchone())
    """
    ext_path = loadable_path()

    # Detect connection type and use appropriate API
    conn_type = type(conn).__module__

    if "apsw" in conn_type:
        # apsw connection
        conn.enableloadextension(True)
        conn.loadextension(ext_path, entry_point)
        conn.enableloadextension(False)
    else:
        # sqlite3 connection
        conn.enable_load_extension(True)
        if entry_point:
            conn.load_extension(ext_path, entry_point)
        else:
            conn.load_extension(ext_path)
        conn.enable_load_extension(False)
