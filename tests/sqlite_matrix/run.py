"""SQLite version matrix (GQLITE-T-0531): the functional tests and the
openCypher TCK against given SQLite releases.

    python -m tests.sqlite_matrix.run [--versions 3.45.1,3.54.1,snapshot] [--no-tck]

For each version: download the amalgamation from sqlite.org into
build/sqlite-matrix/<version>/ (kept between runs), build a sqlite3 shell
and a shared libsqlite3 from it, then

  * run `make test-functional SQLITE=<that shell>`;
  * run the TCK with Python loading that libsqlite3 (DYLD_LIBRARY_PATH on
    macOS, LD_LIBRARY_PATH on Linux; Python's own sqlite3 module must link
    libsqlite3 dynamically, as MacPorts and Ubuntu Pythons do).

The extension under test (build/graphqlite) is the one built with the
system headers: a loadable extension is called through the host's
sqlite3_api table, so one build serves every host version.

Exit code 1 when a version fails.
"""

from __future__ import annotations

import argparse
import io
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# The oldest SQLite GraphQLite supports (docs/…/installation.md), Ubuntu
# 24.04's (the Linux CI), and newer releases.
DEFAULT_VERSIONS = ["3.45.1", "3.46.1", "3.50.4", "3.53.4", "3.54.1", "snapshot"]
MINIMUM = "3.45.1"

YEARS = {  # release year, for the download URL
    "3.38": 2022, "3.39": 2022, "3.40": 2022, "3.41": 2023, "3.42": 2023, "3.43": 2023,
    "3.44": 2023, "3.45": 2024, "3.46": 2024, "3.47": 2024, "3.48": 2025, "3.49": 2025,
    "3.50": 2025, "3.51": 2025, "3.52": 2026, "3.53": 2026, "3.54": 2026,
}

CFLAGS = ["-O1", "-DSQLITE_ENABLE_MATH_FUNCTIONS", "-DSQLITE_ENABLE_JSON1", "-DSQLITE_THREADSAFE=1",
          "-DSQLITE_ENABLE_COLUMN_METADATA", "-DSQLITE_ENABLE_STAT4", "-DSQLITE_MAX_EXPR_DEPTH=10000"]


def version_code(v: str) -> str:
    a, b, c = (v.split(".") + ["0", "0"])[:3]
    return f"{int(a)}{int(b):02d}{int(c):02d}00"


def fetch(version: str, dest: Path) -> Path:
    """The directory holding sqlite3.c and shell.c."""
    marker = dest / "src"
    if (marker / "sqlite3.c").is_file():
        return marker
    dest.mkdir(parents=True, exist_ok=True)
    if version == "snapshot":
        page = urllib.request.urlopen("https://www.sqlite.org/download.html", timeout=60).read().decode()
        m = re.search(r"snapshot/sqlite-snapshot-\d+\.tar\.gz", page)
        if not m:
            raise RuntimeError("no snapshot on the download page")
        url = "https://www.sqlite.org/" + m.group(0)
        data = urllib.request.urlopen(url, timeout=300).read()
        tmp = dest / "snapshot.tar.gz"
        tmp.write_bytes(data)
        shutil.rmtree(dest / "x", ignore_errors=True)
        (dest / "x").mkdir()
        subprocess.run(["tar", "-xzf", str(tmp), "-C", str(dest / "x")], check=True)
        root = next(p.parent for p in (dest / "x").rglob("sqlite3.c"))
        if not (root / "shell.c").is_file():
            raise RuntimeError("snapshot without shell.c")
        shutil.move(str(root), str(marker))
        shutil.rmtree(dest / "x", ignore_errors=True)
        (dest / "url.txt").write_text(url + "\n")
        return marker
    year = YEARS[".".join(version.split(".")[:2])]
    url = f"https://www.sqlite.org/{year}/sqlite-amalgamation-{version_code(version)}.zip"
    data = urllib.request.urlopen(url, timeout=300).read()
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        z.extractall(dest / "x")
    root = next(p.parent for p in (dest / "x").rglob("sqlite3.c"))
    shutil.move(str(root), str(marker))
    shutil.rmtree(dest / "x", ignore_errors=True)
    return marker


def build(src: Path, dest: Path) -> tuple[Path, Path]:
    shell = dest / "sqlite3"
    libdir = dest / "lib"
    darwin = platform.system() == "Darwin"
    lib = libdir / ("libsqlite3.0.dylib" if darwin else "libsqlite3.so.0")
    cc = os.environ.get("CC", "cc")
    if not shell.is_file():
        subprocess.run([cc, *CFLAGS, "-DSQLITE_ENABLE_LOAD_EXTENSION", "-o", str(shell),
                        str(src / "shell.c"), str(src / "sqlite3.c"), "-lpthread", "-lm"]
                       + ([] if darwin else ["-ldl"]), check=True)
    if not lib.is_file():
        libdir.mkdir(parents=True, exist_ok=True)
        if darwin:
            cmd = [cc, *CFLAGS, "-dynamiclib", "-install_name", str(lib), "-o", str(lib), str(src / "sqlite3.c")]
        else:
            cmd = [cc, *CFLAGS, "-shared", "-fPIC", "-Wl,-soname,libsqlite3.so.0", "-o", str(lib),
                   str(src / "sqlite3.c"), "-lpthread", "-lm", "-ldl"]
        subprocess.run(cmd, check=True)
        if not darwin:
            (libdir / "libsqlite3.so").unlink(missing_ok=True)
            (libdir / "libsqlite3.so").symlink_to("libsqlite3.so.0")
    return shell, libdir


def run_version(version: str, work: Path, ext: str, tck: bool) -> tuple[bool, str]:
    dest = work / version
    try:
        src = fetch(version, dest)
        shell, libdir = build(src, dest)
    except Exception as e:  # noqa: BLE001 - a download or build failure is a result
        if version == "snapshot" and "no snapshot" in str(e):
            return True, "skipped: sqlite.org has no pre-release snapshot just now"
        return False, f"setup failed: {e}"
    real = subprocess.run([str(shell), ":memory:", "SELECT sqlite_version()"],
                          capture_output=True, text=True).stdout.strip()
    log = work / f"{version}.functional.log"
    r = subprocess.run(["make", "test-functional", f"SQLITE={shell}"], cwd=ROOT,
                       capture_output=True, text=True)
    log.write_text(r.stdout + r.stderr)
    parts = [f"sqlite {real}"]
    ok = r.returncode == 0
    if ok:
        parts.append("functional ok")
    else:
        failing = re.findall(r"^Running: (\S+)", r.stdout, re.M)
        parts.append(f"functional FAILED at {failing[-1] if failing else '?'} (see {log.relative_to(ROOT)})")
    if tck:
        env = os.environ.copy()
        key = "DYLD_LIBRARY_PATH" if platform.system() == "Darwin" else "LD_LIBRARY_PATH"
        env[key] = str(libdir) + (os.pathsep + env[key] if env.get(key) else "")
        env["PYTHONPATH"] = str(ROOT) + (os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else "")
        check = subprocess.run([sys.executable, "-c", "import sqlite3; print(sqlite3.sqlite_version)"],
                               env=env, capture_output=True, text=True).stdout.strip()
        if check != real:
            ok = False
            parts.append(f"TCK not run: Python loads SQLite {check or '?'}, not {real}")
        else:
            out = work / f"{version}.tck.json"
            t = subprocess.run([sys.executable, "-m", "tests.tck", "--ext", ext, "--quiet", "--out", str(out)],
                               cwd=ROOT, env=env, capture_output=True, text=True)
            summary = [line for line in t.stdout.splitlines() if line.startswith("TCK [")]
            parts.append(summary[-1] if summary else f"TCK failed to run: {t.stderr[-300:]}")
            results = json.loads(out.read_text()) if out.is_file() else []
            bad = [o for o in results if o.get("status") in ("fail", "error")]
            if bad or not summary:
                ok = False
                for o in bad[:5]:
                    parts.append(f"  {o['feature_file'].split('/')[-1]}: {o['scenario_name'][:60]}: "
                                 f"{str(o.get('diagnostic'))[:120]}")
    return ok, "; ".join(parts[:3]) + ("\n" + "\n".join(parts[3:]) if len(parts) > 3 else "")


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="tests.sqlite_matrix.run")
    p.add_argument("--versions", default=",".join(DEFAULT_VERSIONS))
    p.add_argument("--ext", default=str(ROOT / "build" / "graphqlite"))
    p.add_argument("--no-tck", action="store_true", help="functional tests only")
    p.add_argument("--dir", default=str(ROOT / "build" / "sqlite-matrix"))
    args = p.parse_args(argv)
    work = Path(args.dir)
    work.mkdir(parents=True, exist_ok=True)
    all_ok = True
    for v in args.versions.split(","):
        ok, msg = run_version(v.strip(), work, args.ext, not args.no_tck)
        all_ok &= ok
        print(f"{v}: {'ok' if ok else 'FAILED'}: {msg}", flush=True)
    print(f"sqlite matrix: {'all ok' if all_ok else 'failures'} (minimum supported {MINIMUM})")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
