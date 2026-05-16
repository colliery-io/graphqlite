"""
SQLite extension backend — supervises a worker subprocess that hosts the
extension, so a crash in the C code is observable as a `BackendError` and
the harness can continue with the next scenario after respawning.

Talks one-JSON-per-line on stdin/stdout (`_extension_worker.py`).
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


DEFAULT_EXTENSION_PATH = Path("build/graphqlite")
WORKER_MODULE = "tests.tck._extension_worker"


class ExtensionBackend(Backend):
    name = "extension"

    def __init__(self, extension_path: Path | None = None, debug_log: Path | None = None,
                 keep_debug: bool = False, python: str | None = None,
                 execute_timeout: float = 10.0):
        self._ext_path = extension_path or DEFAULT_EXTENSION_PATH
        self._debug_log = debug_log
        self._keep_debug = keep_debug
        self._python = python or sys.executable
        self._execute_timeout = execute_timeout
        self._proc: subprocess.Popen | None = None
        self._spawn()

    # --- Backend API ------------------------------------------------------

    def reset(self) -> None:
        try:
            self._send({"cmd": "reset"})
        except BackendError:
            self._spawn()  # worker died on reset; retry once
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
        try:
            msg = {"cmd": "execute", "query": query}
            if parameters:
                msg["parameters"] = parameters
            resp = self._send(msg, timeout=self._execute_timeout)
        except _Timeout as e:
            self._spawn()
            return QueryResult(error="ExtensionTimeout", error_message=str(e))
        except BackendError as e:
            # Worker died (likely a crash in the extension). Respawn so the
            # next scenario starts clean, and surface as an error result.
            self._spawn()
            return QueryResult(error="ExtensionCrash", error_message=str(e))

        if not resp.get("ok"):
            return QueryResult(
                error=resp.get("error_class", "ExtensionError"),
                error_message=resp.get("error_message", ""),
            )
        columns = list(resp.get("columns") or [])
        rows = [list(r) for r in (resp.get("rows") or [])]
        return QueryResult(headers=columns, rows=rows)

    def close(self) -> None:
        if self._proc is None or self._proc.poll() is not None:
            return
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

    # --- internals --------------------------------------------------------

    def _spawn(self) -> None:
        if self._proc is not None and self._proc.poll() is None:
            try:
                self._proc.kill()
            except OSError:
                pass
            self._proc.wait()
        env = os.environ.copy()
        env["GQLITE_TCK_EXT"] = str(self._ext_path)
        if self._debug_log and not self._keep_debug:
            env["GQLITE_TCK_DEBUG_LOG"] = str(self._debug_log)
        # Ensure the worker can import the harness package.
        repo_root = Path(__file__).resolve().parents[3]
        env["PYTHONPATH"] = str(repo_root) + (os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else "")
        stderr_target = subprocess.DEVNULL if self._keep_debug is False and self._debug_log is None else None
        if self._debug_log and not self._keep_debug:
            stderr_target = open(self._debug_log, "ab", buffering=0)
        self._proc = subprocess.Popen(
            [self._python, "-m", WORKER_MODULE],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=stderr_target,
            text=True, bufsize=1, env=env,
        )

    def _send(self, msg: dict, timeout: float | None = None) -> dict:
        if self._proc is None or self._proc.stdin is None or self._proc.stdout is None:
            raise BackendError("extension worker not running")
        try:
            self._proc.stdin.write(json.dumps(msg) + "\n")
            self._proc.stdin.flush()
        except (BrokenPipeError, OSError) as e:
            rc = self._proc.poll()
            raise BackendError(f"extension worker died (rc={rc}): {e}") from e

        line = self._read_with_timeout(timeout)
        if not line:
            rc = self._proc.wait()
            raise BackendError(f"extension worker exited unexpectedly (rc={rc}, signal={_signal_of(rc)})")
        try:
            return json.loads(line)
        except json.JSONDecodeError as e:
            raise BackendError(f"extension worker emitted non-JSON: {line!r} ({e})") from e

    def _read_with_timeout(self, timeout: float | None) -> str:
        """Read one line from worker stdout; SIGKILL the worker on timeout.

        Implemented with a threading.Timer that kills the process; when killed,
        the worker's stdout EOFs and readline returns "" — which is then
        translated into a _Timeout by the caller observing the watchdog flag.
        """
        if timeout is None or timeout <= 0:
            return self._proc.stdout.readline()

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
            raise _Timeout(f"extension worker timed out after {timeout:.1f}s")
        return line


class _Timeout(BackendError):
    pass


def _signal_of(rc: int) -> str:
    # subprocess returns negative for signal-killed; on macOS rc can also be
    # 128+signal in some shells. Surface both.
    if rc is None:
        return "?"
    if rc < 0:
        return f"SIG{-rc}"
    if rc > 128:
        return f"SIG{rc - 128}"
    return f"exit={rc}"
