"""
Backend adapter for the Python binding (`graphqlite` package).

Drives a long-lived worker subprocess (`tests/tck/_python_binding_worker.py`)
with the same one-JSON-per-line protocol as the extension and Rust workers,
plus a per-execute watchdog. Subprocess isolation matters because the
binding loads the SQLite extension *in-process*; any infinite loop in the
extension would otherwise hang the whole harness with no way out.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import threading
from pathlib import Path
from typing import Any

from .base import Backend, BackendError, QueryResult


WORKER_MODULE = "tests.tck._python_binding_worker"


class PythonBindingBackend(Backend):
    name = "python"

    def __init__(self, debug_log: Path | None = None, keep_debug: bool = False,
                 python: str | None = None, execute_timeout: float = 10.0):
        self._debug_log = debug_log
        self._keep_debug = keep_debug
        self._python = python or sys.executable
        self._execute_timeout = execute_timeout
        self._proc: subprocess.Popen | None = None
        self._stderr_sink = None
        self._spawn()

    def reset(self) -> None:
        try:
            self._send({"cmd": "reset"})
        except BackendError:
            self._spawn()
            self._send({"cmd": "reset"})

    def load_named_graph(self, name: str) -> None:
        root = Path("vendor/tck/graphs") / name
        for candidate in (root / f"{name}.cypher", root / "cypher.cyp"):
            if candidate.exists():
                cyp = candidate
                break
        else:
            raise BackendError(f"named graph not found: {name!r}")
        for stmt in (s.strip() for s in cyp.read_text(encoding="utf-8").split(";")):
            if stmt:
                self.execute(stmt)

    def execute(self, query: str, parameters: dict[str, Any] | None = None) -> QueryResult:
        msg = {"cmd": "execute", "query": query}
        if parameters:
            msg["parameters"] = parameters
        try:
            resp = self._send(msg, timeout=self._execute_timeout)
        except _Timeout as e:
            self._spawn()
            return QueryResult(error="PythonBindingTimeout", error_message=str(e))
        except BackendError as e:
            self._spawn()
            return QueryResult(error="PythonBindingCrash", error_message=str(e))
        if not resp.get("ok"):
            return QueryResult(
                error=resp.get("error_class", "PythonBindingError"),
                error_message=resp.get("error_message", ""),
            )
        cols = list(resp.get("columns") or [])
        rows: list[list[Any]] = []
        for record in resp.get("rows") or []:
            if cols:
                rows.append([record.get(c) for c in cols])
            else:
                rows.append([record])
        return QueryResult(headers=cols, rows=rows)

    def close(self) -> None:
        if self._proc is not None and self._proc.poll() is None:
            try:
                self._send({"cmd": "shutdown"})
            except BackendError:
                pass
            try:
                self._proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self._proc.kill()
                self._proc.wait()
        self._proc = None
        if self._stderr_sink:
            try:
                self._stderr_sink.close()
            except OSError:
                pass
            self._stderr_sink = None

    # --- internals --------------------------------------------------------

    def _spawn(self) -> None:
        if self._proc is not None and self._proc.poll() is None:
            try:
                self._proc.kill()
            except OSError:
                pass
            self._proc.wait()
        if self._stderr_sink is not None:
            try:
                self._stderr_sink.close()
            except OSError:
                pass
            self._stderr_sink = None
        env = os.environ.copy()
        if self._debug_log and not self._keep_debug:
            env["GQLITE_TCK_DEBUG_LOG"] = str(self._debug_log)
        repo_root = Path(__file__).resolve().parents[3]
        env["PYTHONPATH"] = str(repo_root) + (os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else "")
        if self._debug_log and not self._keep_debug:
            self._debug_log.parent.mkdir(parents=True, exist_ok=True)
            self._stderr_sink = open(self._debug_log, "ab", buffering=0)
            stderr_target = self._stderr_sink
        else:
            stderr_target = subprocess.DEVNULL
        self._proc = subprocess.Popen(
            [self._python, "-m", WORKER_MODULE],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=stderr_target,
            text=True, bufsize=1, env=env,
        )

    def _send(self, msg: dict, timeout: float | None = None) -> dict:
        if self._proc is None or self._proc.stdin is None or self._proc.stdout is None:
            raise BackendError("python binding worker not running")
        try:
            self._proc.stdin.write(json.dumps(msg, default=str) + "\n")
            self._proc.stdin.flush()
        except (BrokenPipeError, OSError) as e:
            raise BackendError(f"python binding worker died: {e}") from e
        line = self._read_with_timeout(timeout)
        if not line:
            rc = self._proc.wait()
            raise BackendError(f"python binding worker exited (rc={rc})")
        try:
            return json.loads(line)
        except json.JSONDecodeError as e:
            raise BackendError(f"python binding worker emitted non-JSON: {line!r} ({e})") from e

    def _read_with_timeout(self, timeout: float | None) -> str:
        if timeout is None or timeout <= 0 or self._proc is None or self._proc.stdout is None:
            return self._proc.stdout.readline() if (self._proc and self._proc.stdout) else ""
        timed_out = {"v": False}

        def _kill():
            timed_out["v"] = True
            try:
                self._proc.kill()
            except OSError:
                pass

        t = threading.Timer(timeout, _kill)
        t.start()
        try:
            line = self._proc.stdout.readline()
        finally:
            t.cancel()
        if timed_out["v"]:
            raise _Timeout(f"python binding worker timed out after {timeout:.1f}s")
        return line


class _Timeout(BackendError):
    pass
