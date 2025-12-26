"""GraphQLite connection wrapper for SQLite."""

import json
import os
import platform
import sqlite3
from pathlib import Path
from typing import Any, Iterator, Optional, Union


class CypherResult:
    """Result from a Cypher query, iterable as rows."""

    def __init__(self, data: list[dict[str, Any]], columns: list[str]):
        self._data = data
        self._columns = columns

    def __iter__(self) -> Iterator[dict[str, Any]]:
        return iter(self._data)

    def __len__(self) -> int:
        return len(self._data)

    def __getitem__(self, index: int) -> dict[str, Any]:
        return self._data[index]

    @property
    def columns(self) -> list[str]:
        """Column names from the query."""
        return self._columns

    def to_list(self) -> list[dict[str, Any]]:
        """Return results as a list of dictionaries."""
        return self._data


class Connection:
    """GraphQLite database connection with Cypher query support."""

    def __init__(self, conn: sqlite3.Connection, extension_path: Optional[str] = None):
        """
        Initialize GraphQLite connection.

        Args:
            conn: SQLite database connection
            extension_path: Path to graphqlite extension (auto-detected if None)
        """
        self._conn = conn
        self._load_extension(extension_path)

    def _find_extension(self) -> str:
        """Find the GraphQLite extension library."""
        system = platform.system()

        if system == "Darwin":
            ext_name = "graphqlite.dylib"
        elif system == "Linux":
            ext_name = "graphqlite.so"
        elif system == "Windows":
            ext_name = "graphqlite.dll"
        else:
            raise OSError(f"Unsupported platform: {system}")

        # Search paths in order of preference
        search_paths = [
            # Bundled with package
            Path(__file__).parent / ext_name,
            # Development build
            Path(__file__).parent.parent.parent.parent.parent / "build" / ext_name,
            # System-wide
            Path("/usr/local/lib") / ext_name,
            Path("/usr/lib") / ext_name,
        ]

        # Check GRAPHQLITE_EXTENSION_PATH environment variable
        env_path = os.environ.get("GRAPHQLITE_EXTENSION_PATH")
        if env_path:
            search_paths.insert(0, Path(env_path))

        for path in search_paths:
            if path.exists():
                return str(path.resolve())

        raise FileNotFoundError(
            f"GraphQLite extension not found. Searched: {[str(p) for p in search_paths]}\n"
            f"Set GRAPHQLITE_EXTENSION_PATH or build the extension with 'make extension'"
        )

    def _load_extension(self, extension_path: Optional[str] = None) -> None:
        """Load the GraphQLite SQLite extension."""
        if extension_path is None:
            extension_path = self._find_extension()

        # Enable extension loading
        try:
            self._conn.enable_load_extension(True)
        except AttributeError as e:
            raise RuntimeError(
                "SQLite extension loading not available. "
                "Your Python's sqlite3 module may not support extensions.\n"
                "On macOS with MacPorts/Homebrew, try:\n"
                "  DYLD_LIBRARY_PATH=/opt/local/lib python your_script.py"
            ) from e

        # Load extension (remove file extension for SQLite)
        ext_path = Path(extension_path)
        load_path = str(ext_path.parent / ext_path.stem)

        try:
            self._conn.load_extension(load_path)
        except sqlite3.OperationalError as e:
            error_msg = str(e).lower()
            if "not authorized" in error_msg:
                raise RuntimeError(
                    "SQLite extension loading is disabled. "
                    "The system SQLite may not allow extensions.\n"
                    "On macOS, try using Homebrew or MacPorts Python with:\n"
                    "  DYLD_LIBRARY_PATH=/opt/local/lib python your_script.py"
                ) from e
            raise

        # Verify extension loaded
        cursor = self._conn.execute("SELECT graphqlite_test()")
        result = cursor.fetchone()
        if not result or "successfully" not in result[0].lower():
            raise RuntimeError("Failed to initialize GraphQLite extension")

    def cypher(self, query: str, params: Optional[dict[str, Any]] = None) -> CypherResult:
        """
        Execute a Cypher query with optional parameters.

        Args:
            query: Cypher query string, may contain $param placeholders
            params: Optional dictionary of parameter values

        Returns:
            CypherResult object with query results

        Raises:
            sqlite3.Error: If the query fails

        Example:
            >>> db.cypher("MATCH (n) WHERE n.name = $name RETURN n", {"name": "Alice"})
        """
        if params:
            params_json = json.dumps(params)
            cursor = self._conn.execute("SELECT cypher(?, ?)", (query, params_json))
        else:
            cursor = self._conn.execute("SELECT cypher(?)", (query,))
        row = cursor.fetchone()

        if row is None or row[0] is None:
            return CypherResult([], [])

        result_str = row[0]

        # Parse JSON result
        try:
            data = json.loads(result_str)
        except json.JSONDecodeError:
            # Non-JSON result (error message or scalar)
            if result_str.startswith("Error"):
                raise sqlite3.Error(result_str)
            return CypherResult([{"result": result_str}], ["result"])

        # Handle different result formats
        if isinstance(data, list):
            if len(data) == 0:
                return CypherResult([], [])
            if isinstance(data[0], dict):
                columns = list(data[0].keys()) if data else []
                return CypherResult(data, columns)
            # List of scalars - this happens when C returns raw JSON array
            # for single-cell queries (e.g., range(), tail(), graph algorithms)
            # Treat as single row with the original JSON string as value
            return CypherResult([{"result": result_str}], ["result"])
        elif isinstance(data, dict):
            return CypherResult([data], list(data.keys()))
        else:
            return CypherResult([{"result": data}], ["result"])

    def execute(self, sql: str, parameters: tuple = ()) -> sqlite3.Cursor:
        """Execute a raw SQL query."""
        return self._conn.execute(sql, parameters)

    def commit(self) -> None:
        """Commit the current transaction."""
        self._conn.commit()

    def rollback(self) -> None:
        """Rollback the current transaction."""
        self._conn.rollback()

    def close(self) -> None:
        """Close the database connection."""
        self._conn.close()

    def __enter__(self) -> "Connection":
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.close()

    @property
    def sqlite_connection(self) -> sqlite3.Connection:
        """Access the underlying SQLite connection."""
        return self._conn


def connect(
    database: Union[str, Path] = ":memory:",
    extension_path: Optional[str] = None,
    **kwargs
) -> Connection:
    """
    Open a GraphQLite database connection.

    Args:
        database: Path to database file or ":memory:" for in-memory database
        extension_path: Path to graphqlite extension (auto-detected if None)
        **kwargs: Additional arguments passed to sqlite3.connect()

    Returns:
        Connection object with Cypher query support

    Example:
        >>> db = connect("graph.db")
        >>> db.cypher("CREATE (n:Person {name: 'Alice'})")
        >>> results = db.cypher("MATCH (n:Person) RETURN n.name")
        >>> for row in results:
        ...     print(row["n.name"])
    """
    conn = sqlite3.connect(str(database), **kwargs)
    return Connection(conn, extension_path)


def wrap(conn: sqlite3.Connection, extension_path: Optional[str] = None) -> Connection:
    """
    Wrap an existing SQLite connection with GraphQLite support.

    Args:
        conn: Existing SQLite connection
        extension_path: Path to graphqlite extension (auto-detected if None)

    Returns:
        Connection object with Cypher query support

    Example:
        >>> import sqlite3
        >>> conn = sqlite3.connect("graph.db")
        >>> db = wrap(conn)
        >>> db.cypher("MATCH (n) RETURN count(n)")
    """
    return Connection(conn, extension_path)
