"""Backend adapter for the in-process Python binding (`graphqlite` package)."""

from __future__ import annotations

import contextlib
import os
import sys
from pathlib import Path
from typing import Any

from .base import Backend, BackendError, QueryResult


class PythonBindingBackend(Backend):
    name = "python"

    def __init__(self, debug_log: Path | None = None, keep_debug: bool = False):
        # Ensure the local bindings/python source layout is importable when
        # running from a checkout (preferred over any system-installed package).
        repo_root = Path(__file__).resolve().parents[3]
        src = repo_root / "bindings" / "python" / "src"
        if src.exists() and str(src) not in sys.path:
            sys.path.insert(0, str(src))
        try:
            import graphqlite  # noqa: F401
        except ImportError as e:
            raise BackendError(f"python binding not importable: {e}") from e

        self._graphqlite = __import__("graphqlite")
        self._debug_log = debug_log
        self._keep_debug = keep_debug
        self._conn = None
        self.reset()

    def reset(self) -> None:
        with self._silenced():
            if self._conn is not None:
                self._conn.close()
            self._conn = self._graphqlite.connect(":memory:")

    def load_named_graph(self, name: str) -> None:
        cyp = Path("vendor/tck/graphs") / name / "cypher.cyp"
        if not cyp.exists():
            raise BackendError(f"named graph not found: {name!r}")
        for stmt in (s.strip() for s in cyp.read_text(encoding="utf-8").split(";")):
            if stmt:
                self.execute(stmt)

    def execute(self, query: str, parameters: dict[str, Any] | None = None) -> QueryResult:
        assert self._conn is not None
        try:
            with self._silenced():
                res = self._conn.cypher(query, parameters or None)
        except Exception as e:
            return QueryResult(error=type(e).__name__, error_message=str(e))

        cols = list(res.columns) if hasattr(res, "columns") else []
        rows: list[list[Any]] = []
        for record in res:
            if cols:
                rows.append([record.get(c) for c in cols])
            else:
                rows.append([record])
        return QueryResult(headers=cols, rows=rows)

    def close(self) -> None:
        if self._conn is not None:
            with self._silenced():
                self._conn.close()
            self._conn = None

    @contextlib.contextmanager
    def _silenced(self):
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
