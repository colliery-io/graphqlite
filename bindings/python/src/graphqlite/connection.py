"""GraphQLite connection wrapper for SQLite."""

import json
import os
import platform
import sqlite3
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterator, NoReturn, Optional, Union


class CypherError(sqlite3.Error):
    """
    A Cypher query failed in the extension.

    Subclasses ``sqlite3.Error`` so existing ``except sqlite3.Error`` handlers
    keep working, and adds the structured fields the extension reports
    (GitHub #16):

    - ``code``: ``PARSE_ERROR``, ``VALIDATION_ERROR``, ``EXECUTION_ERROR``,
      ``NOT_IMPLEMENTED``, ``MEMORY_ERROR`` or ``INTERNAL_ERROR``.
    - ``line`` / ``column``: 1-based location of the offending token for
      parse errors, else ``None``.
    """

    def __init__(
        self,
        message: str,
        code: Optional[str] = None,
        line: Optional[int] = None,
        column: Optional[int] = None,
    ):
        super().__init__(message)
        self.message = message
        self.code = code
        self.line = line
        self.column = column


def _raise_structured(e: sqlite3.Error) -> NoReturn:
    """Re-raise an extension error as CypherError when it carries the JSON shape."""
    try:
        err_data = json.loads(str(e))
    except (json.JSONDecodeError, TypeError):
        raise e
    if isinstance(err_data, dict) and "error" in err_data:
        raise CypherError(
            err_data["error"],
            code=err_data.get("code"),
            line=err_data.get("line"),
            column=err_data.get("column"),
        ) from None
    raise e


@dataclass(frozen=True)
class Diagnostic:
    """One validation diagnostic: a stable code, a message and an optional location."""

    code: str
    message: str
    line: Optional[int] = None
    column: Optional[int] = None

    def __str__(self) -> str:
        if self.line is not None and self.column is not None:
            return f"{self.code} at {self.line}:{self.column}: {self.message}"
        if self.line is not None:
            return f"{self.code} at line {self.line}: {self.message}"
        return f"{self.code}: {self.message}"


@dataclass(frozen=True)
class Capabilities:
    """What this build of the extension supports (:meth:`Connection.capabilities`).

    ``schema_version`` changes only when a key of the document is added,
    renamed or removed; new feature flags do not change it. A flag that is
    missing from ``features`` must be read as ``False`` (see :meth:`supports`).
    """

    schema_version: int
    graphqlite_version: str
    cypher_dialect: str
    sqlite_version: str
    json1: bool
    neo4j_compat: bool
    features: dict

    def supports(self, flag: str) -> bool:
        """``True`` when ``flag`` is present and enabled."""
        return bool(self.features.get(flag, False))


@dataclass(frozen=True)
class ValidationResult:
    """Outcome of :meth:`Connection.validate`. Truthy when the query is valid."""

    valid: bool
    diagnostic: Optional[Diagnostic] = None

    def __bool__(self) -> bool:
        return self.valid


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
        try:
            if params:
                params_json = json.dumps(params)
                cursor = self._conn.execute("SELECT cypher(?, ?)", (query, params_json))
            else:
                cursor = self._conn.execute("SELECT cypher(?)", (query,))
        except sqlite3.Error as e:
            _raise_structured(e)

        row = cursor.fetchone()

        if row is None or row[0] is None:
            return CypherResult([], [])

        result_str = row[0]

        # Parse JSON result
        try:
            data = json.loads(result_str)
        except json.JSONDecodeError:
            # Non-JSON result (scalar or legacy error)
            if result_str.startswith("Error") or result_str.startswith("{\"error\""):
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

    def iter_rows(self, query: str, params: Optional[dict[str, Any]] = None) -> Iterator[dict[str, Any]]:
        """
        Stream a Cypher query's rows through the ``cypher_rows`` virtual table.

        Unlike :meth:`cypher`, which receives the whole result as one JSON
        string and decodes it, this yields one dict per row as SQLite steps
        the table, so peak memory is one row and consumers can stop early.
        Each dict has the same keys and value shapes ``cypher()`` produces
        (nodes, relationships, paths, lists and maps decoded from JSON).

        A write query without RETURN yields one dict of modification counts.

        Example:
            >>> for row in db.iter_rows("MATCH (n:Person) RETURN n.name AS name"):
            ...     print(row["name"])
        """
        params_json = json.dumps(params) if params else None
        try:
            cursor = self._conn.execute("SELECT row FROM cypher_rows(?, ?)", (query, params_json))
            for (row_json,) in cursor:
                yield json.loads(row_json)
        except sqlite3.Error as e:
            _raise_structured(e)

    def capabilities(self) -> Capabilities:
        """
        Report what this build of the extension supports.

        Wraps the SQL function ``cypher_capabilities()``; use it to detect
        features at run time instead of comparing version strings.

        Example:
            >>> caps = db.capabilities()
            >>> caps.graphqlite_version
            '0.9.2'
            >>> caps.supports("existential_subquery_full")
            True
        """
        cursor = self._conn.execute("SELECT cypher_capabilities()")
        data = json.loads(cursor.fetchone()[0])
        sqlite_info = data.get("sqlite") or {}
        return Capabilities(
            schema_version=int(data.get("schema_version", 0)),
            graphqlite_version=str(data.get("graphqlite_version", "")),
            cypher_dialect=str(data.get("cypher_dialect", "")),
            sqlite_version=str(sqlite_info.get("version", "")),
            json1=bool(sqlite_info.get("json1", False)),
            neo4j_compat=bool(data.get("neo4j_compat", False)),
            features=dict(data.get("features") or {}),
        )

    def validate(self, query: str) -> ValidationResult:
        """
        Validate a Cypher query without executing it.

        Runs the scanner, the grammar and the extension's compile-time
        semantic checks (the same pass ``cypher()`` runs before transform) and
        returns the outcome as data. The graph is never read or written, so a
        ``CREATE`` validates without creating anything.

        Syntax failures carry ``PARSE_ERROR`` with a 1-based ``line`` and
        ``column``; static semantic failures such as ``RETURN NOT 1`` carry
        ``VALIDATION_ERROR``. Errors that only surface during transform or
        execution (an unknown variable, for example) are not detected here.

        Example:
            >>> v = db.validate("MATCH (n:Person RETURN n.name")
            >>> v.valid
            False
            >>> (v.diagnostic.code, v.diagnostic.line, v.diagnostic.column)
            ('PARSE_ERROR', 1, 17)
        """
        cursor = self._conn.execute("SELECT cypher_validate(?)", (query,))
        row = cursor.fetchone()
        data = json.loads(row[0])
        if data.get("valid"):
            return ValidationResult(valid=True)
        return ValidationResult(
            valid=False,
            diagnostic=Diagnostic(
                code=data.get("code", "VALIDATION_ERROR"),
                message=data.get("error", "Unknown validation error"),
                line=data.get("line"),
                column=data.get("column"),
            ),
        )

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
