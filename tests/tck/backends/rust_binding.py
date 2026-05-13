"""
Backend adapter for the Rust binding.

Drives `bindings/rust/examples/tck_runner` — a long-lived REPL that speaks
one JSON object per line:

    request:  {"cmd": "reset"}
    request:  {"cmd": "execute", "query": "..."}
    response: {"ok": bool, "columns": [...], "rows": [...]} or {"ok": false, "error": "..."}
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


RUNNER_REL = Path("bindings/rust/target/debug/examples/tck_runner")


class RustBindingBackend(Backend):
    name = "rust"

    def __init__(self, runner: Path | None = None, debug_log: Path | None = None,
                 execute_timeout: float = 10.0):
        path = runner or RUNNER_REL
        if not path.exists():
            raise BackendError(
                f"rust tck_runner not found at {path}. Build with: "
                f"cargo build --example tck_runner --manifest-path bindings/rust/Cargo.toml"
            )
        self._runner_path = path
        self._debug_log = debug_log
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
        cyp = Path("vendor/tck/graphs") / name / "cypher.cyp"
        if not cyp.exists():
            raise BackendError(f"named graph not found: {name!r}")
        for stmt in (s.strip() for s in cyp.read_text(encoding="utf-8").split(";")):
            if stmt:
                self.execute(stmt)

    def execute(self, query: str, parameters: dict[str, Any] | None = None) -> QueryResult:
        # parameters are currently dropped; see TCK-06 triage. v1 contract is
        # equivalent to the extension backend.
        try:
            resp = self._send({"cmd": "execute", "query": query}, timeout=self._execute_timeout)
        except _Timeout as e:
            self._spawn()
            return QueryResult(error="RustBindingTimeout", error_message=str(e))
        except BackendError as e:
            self._spawn()
            return QueryResult(error="RustBindingCrash", error_message=str(e))
        if not resp.get("ok"):
            return QueryResult(error="RustBindingError", error_message=resp.get("error", ""))
        cols = list(resp.get("columns") or [])
        rows = []
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
        stderr_target: Any
        if self._debug_log:
            self._debug_log.parent.mkdir(parents=True, exist_ok=True)
            self._stderr_sink = open(self._debug_log, "ab", buffering=0)
            stderr_target = self._stderr_sink
        else:
            stderr_target = subprocess.DEVNULL
        self._proc = subprocess.Popen(
            [str(self._runner_path)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=stderr_target,
            text=True, bufsize=1,
        )

    def _send(self, msg: dict[str, Any], timeout: float | None = None) -> dict[str, Any]:
        if self._proc is None or self._proc.stdin is None or self._proc.stdout is None:
            raise BackendError("rust runner pipes closed")
        try:
            self._proc.stdin.write(json.dumps(msg) + "\n")
            self._proc.stdin.flush()
        except (BrokenPipeError, OSError) as e:
            raise BackendError(f"rust runner died: {e}") from e
        line = self._read_with_timeout(timeout)
        if not line:
            raise BackendError("rust runner closed stdout unexpectedly")
        try:
            return json.loads(line)
        except json.JSONDecodeError as e:
            raise BackendError(f"rust runner emitted non-JSON: {line!r} ({e})") from e

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
            raise _Timeout(f"rust runner timed out after {timeout:.1f}s")
        return line


class _Timeout(BackendError):
    pass
