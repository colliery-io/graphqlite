"""
SQLite extension backend.

Opens an in-memory SQLite, loads the GraphQLite extension, and routes
each Cypher query through `SELECT cypher(?)`.

The extension's stderr `[CYPHER_DEBUG]` chatter is redirected to a log
file for the duration of each call unless `keep_debug=True`.
"""

from __future__ import annotations

import contextlib
import os
import sqlite3
import sys
from pathlib import Path
from typing import Any

from .base import Backend, BackendError, QueryResult


DEFAULT_EXTENSION_PATH = Path("build/graphqlite")  # SQLite appends .dylib/.so/.dll


class ExtensionBackend(Backend):
    name = "extension"

    def __init__(self, extension_path: Path | None = None, debug_log: Path | None = None, keep_debug: bool = False):
        self._ext_path = extension_path or DEFAULT_EXTENSION_PATH
        self._debug_log = debug_log
        self._keep_debug = keep_debug
        self._conn: sqlite3.Connection | None = None
        self.reset()

    def reset(self) -> None:
        with self._silenced():
            if self._conn is not None:
                self._conn.close()
            conn = sqlite3.connect(":memory:")
            conn.enable_load_extension(True)
            conn.load_extension(str(self._ext_path))
            self._conn = conn

    def load_named_graph(self, name: str) -> None:
        # TCK named graphs live in vendor/tck/graphs/<name>/. v1: try a
        # cypher.cyp file in that dir and execute it as setup. If absent,
        # surface a backend error so the scenario is marked errored.
        root = Path("vendor/tck/graphs") / name
        cyp = root / "cypher.cyp"
        if not cyp.exists():
            raise BackendError(f"named graph not found or unsupported: {name!r}")
        text = cyp.read_text(encoding="utf-8")
        # Some named-graph files contain multiple statements separated by ';'.
        for stmt in (s.strip() for s in text.split(";")):
            if stmt:
                self.execute(stmt)

    def execute(self, query: str, parameters: dict[str, Any] | None = None) -> QueryResult:
        assert self._conn is not None
        # Parameter substitution is parameter-passing through the extension's
        # own mechanism, but v1 does textual substitution of $name placeholders
        # only when the extension does not accept parameters. We expose the
        # raw query and let the extension parse it; TCK's parameters tables
        # are recorded but plumbing them through cypher() is a TODO.
        if parameters:
            # TODO: thread parameters through cypher(); for now scenarios that
            # require them will likely fail / error and be triaged in TCK-06.
            pass
        try:
            with self._silenced():
                cur = self._conn.execute("SELECT cypher(?)", (query,))
                rows = cur.fetchall()
        except sqlite3.Error as e:
            return QueryResult(error=_classify_error(str(e)), error_message=str(e))

        # cypher() returns a single TEXT column. The shape (table vs scalar)
        # is opaque at this layer; the runner attempts to decode it via
        # values.parse_value. v1: keep the raw payload in `rows` and let the
        # runner unpack into headers/rows when the textual form looks like a
        # result set.
        raw = [r[0] for r in rows]
        return QueryResult(headers=[], rows=[[v] for v in raw])

    def close(self) -> None:
        if self._conn is not None:
            with self._silenced():
                self._conn.close()
            self._conn = None

    @contextlib.contextmanager
    def _silenced(self):
        """Redirect fds 1 and 2 to the debug log.

        The extension emits [CYPHER_DEBUG] lines to stdout (fd 1) via libc
        printf; some error paths use stderr (fd 2). We capture both for the
        duration of each call unless `keep_debug=True`.
        """
        if self._keep_debug:
            yield
            return
        target = self._debug_log or Path(os.devnull)
        if str(target) != os.devnull:
            target.parent.mkdir(parents=True, exist_ok=True)
        sys.stdout.flush()
        sys.stderr.flush()
        old_out, old_err = os.dup(1), os.dup(2)
        sink = open(target, "ab", buffering=0)
        try:
            os.dup2(sink.fileno(), 1)
            os.dup2(sink.fileno(), 2)
            yield
        finally:
            sys.stdout.flush()
            sys.stderr.flush()
            os.dup2(old_out, 1)
            os.dup2(old_err, 2)
            os.close(old_out)
            os.close(old_err)
            sink.close()


def _classify_error(msg: str) -> str:
    lower = msg.lower()
    if "syntax" in lower or "parse" in lower:
        return "SyntaxError"
    if "type" in lower:
        return "TypeError"
    if "not found" in lower:
        return "EntityNotFound"
    if "constraint" in lower:
        return "ConstraintViolation"
    return "GraphQLiteError"
