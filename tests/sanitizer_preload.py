"""Sanitizer runtime preload for the test runners (GQLITE-T-0495).

The ASan build of the extension (build/asan, `make extension-asan`) needs the
ASan runtime loaded before the host process starts. macOS removes
DYLD_INSERT_LIBRARIES from a Python process's environment once dyld has used
it, so the worker processes the runners start would load the extension
without the runtime. The runners call apply() first: it copies
GQLITE_SANITIZER_PRELOAD back to DYLD_INSERT_LIBRARIES (macOS) or LD_PRELOAD
(Linux), which their workers inherit.
"""

from __future__ import annotations

import os
import sys


def apply() -> None:
    path = os.environ.get("GQLITE_SANITIZER_PRELOAD")
    if not path:
        return
    var = "DYLD_INSERT_LIBRARIES" if sys.platform == "darwin" else "LD_PRELOAD"
    os.environ[var] = path
