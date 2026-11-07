"""Where the metamorphic harness never goes (GQLITE-T-0497).

Reads the gcov counters of the coverage build (`make extension-cov`, written
by a harness run against build/cov/graphqlite) and reports, for the
transform and executor code: line coverage per file, the functions the run
never entered (largest first), and the large functions it entered but
mostly missed. Those are where a new generator or relation pays off.

    python -m tests.metamorphic.coverage_report [--build build/cov] [--top 40] [--out FILE]
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

DIRS = ("transform", "executor")
FUNC = re.compile(r"^Function '(.+)'$")
FILE = re.compile(r"^File '(.+)'$")
LINES = re.compile(r"^Lines executed:([0-9.]+)% of (\d+)$")


def gcov_cmd() -> list[str]:
    if sys.platform == "darwin" and shutil.which("xcrun"):
        return ["xcrun", "llvm-cov", "gcov"]
    return ["gcov"]


def read_object(obj: Path) -> tuple[list[tuple[str, float, int]], tuple[str, float, int] | None]:
    """Functions (name, percent, lines) and the object's own source file."""
    obj = obj.resolve()
    out = subprocess.run([*gcov_cmd(), "-f", "-n", "-o", str(obj.parent), str(obj)],
                         capture_output=True, text=True, cwd=obj.parent).stdout.splitlines()
    funcs, files, name, kind = [], [], None, None
    for line in out:
        if m := FUNC.match(line):
            name, kind = m.group(1), "func"
        elif m := FILE.match(line):
            name, kind = m.group(1), "file"
        elif (m := LINES.match(line)) and name:
            entry = (name, float(m.group(1)), int(m.group(2)))
            (funcs if kind == "func" else files).append(entry)
            name = None
    stem = obj.name.split(".")[0]
    own = next((f for f in files if Path(f[0]).stem == stem), None)
    return funcs, own


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="tests.metamorphic.coverage_report")
    p.add_argument("--build", default="build/cov", type=Path)
    p.add_argument("--top", default=40, type=int)
    p.add_argument("--out", default=None, type=Path)
    args = p.parse_args(argv)

    files, unreached, partial = [], [], []
    for d in DIRS:
        for obj in sorted((args.build / d).glob("*.gcda")):
            funcs, own = read_object(obj)
            src = f"{d}/{obj.name.split('.')[0]}.c"
            if own:
                files.append((src, own[1], own[2]))
            for name, pct, n in funcs:
                if pct == 0.0:
                    unreached.append((n, name, src))
                elif n >= 40 and pct < 50.0:
                    partial.append((n, pct, name, src))
    if not files:
        print(f"no coverage data in {args.build} (run the harness on {args.build}/graphqlite)",
              file=sys.stderr)
        return 2

    lines = []
    total = sum(n for _, _, n in files)
    hit = sum(pct * n / 100 for _, pct, n in files)
    lines.append(f"Harness line coverage of {', '.join(DIRS)}: {100 * hit / total:.1f}% of {total} lines")
    lines.append("")
    lines.append("Per file (lowest first):")
    for src, pct, n in sorted(files, key=lambda f: f[1]):
        lines.append(f"  {pct:6.1f}%  {n:6d}  {src}")
    lines.append("")
    lines.append(f"Never entered ({len(unreached)} functions, {sum(u[0] for u in unreached)} lines), largest first:")
    for n, name, src in sorted(unreached, reverse=True)[: args.top]:
        lines.append(f"  {n:5d}  {name}  ({src})")
    lines.append("")
    lines.append("Entered but under 50% (40+ lines), largest first:")
    for n, pct, name, src in sorted(partial, reverse=True)[: args.top]:
        lines.append(f"  {n:5d}  {pct:5.1f}%  {name}  ({src})")
    text = "\n".join(lines)
    print(text)
    if args.out:
        args.out.write_text(text + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
