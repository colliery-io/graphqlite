"""Seed corpus for the libFuzzer target (GQLITE-T-0496): every query of the
openCypher TCK features (vendor/tck/features) and every cypher('...') string
of the functional tests, one file each, named by content hash.

    python tests/fuzz/make_corpus.py OUT_DIR
"""

from __future__ import annotations

import hashlib
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOCSTRING = re.compile(r'"""\n(.*?)\n\s*"""', re.S)
CYPHER_CALL = re.compile(r"cypher\('((?:[^']|'')*)'")


def queries():
    for f in sorted((ROOT / "vendor" / "tck" / "features").rglob("*.feature")):
        for m in DOCSTRING.finditer(f.read_text(errors="replace")):
            yield "\n".join(line.strip() for line in m.group(1).splitlines())
    for f in sorted((ROOT / "tests" / "functional").glob("*.sql")):
        for m in CYPHER_CALL.finditer(f.read_text(errors="replace")):
            yield m.group(1).replace("''", "'")


def main(argv: list[str]) -> int:
    out = Path(argv[1] if len(argv) > 1 else "build/fuzz/corpus")
    out.mkdir(parents=True, exist_ok=True)
    n = 0
    for q in queries():
        q = q.strip()
        if not q or len(q) > 4096:
            continue
        data = q.encode()
        (out / hashlib.sha1(data).hexdigest()).write_bytes(data)
        n += 1
    print(f"{n} queries, {len(list(out.iterdir()))} files in {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
