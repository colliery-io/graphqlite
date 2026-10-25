"""Dimensions of the generated cases, the chooser, and covering arrays.

The generator asks a Chooser for each choice by dimension name. In random
mode the chooser picks at random; in cover mode it takes the value from a
row of a covering array, so every pair of values (and every triple of the
CORE dimensions) is run at least once per relation.
"""

from __future__ import annotations

import random
from itertools import combinations, product

DIR = ["->", "<-", "-"]
REL = ["named", "anon"]
TYPE = ["any", "one", "alt"]
LABEL = ["none", "one", "multi", "disj"]

DOMAINS: dict[str, list] = {
    "g.shape": ["empty", "isolated", "selfloops", "multi", "chain", "star", "random", "dense"],
    "g.nulls": ["few", "many"],
    "c1.hops": [0, 1, 2], "c1.dir": DIR, "c1.rel": REL, "c1.type": TYPE, "c1.label": LABEL,
    "c2.kind": ["none", "match", "optional"],
    "c2.hops": [1, 2], "c2.dir": DIR, "c2.rel": REL, "c2.type": TYPE, "c2.label": LABEL,
    "with.kind": ["none", "pass", "agg", "distinct", "where", "orderlimit"],
    "c3.kind": ["none", "match", "optional", "unwind"],
    "c3.hops": [1, 2], "c3.dir": DIR, "c3.rel": REL,
    "ret.kind": ["scalar", "agg", "mixed", "distinct"],
    "expr.kind": ["prop", "arith", "case", "func", "list", "map", "nullcheck", "entity", "string"],
    "pred.kind": ["cmp", "null", "bool", "pattern", "in", "string", "label"],
    "agg.kind": ["count_star", "count", "count_distinct", "collect", "collect_distinct",
                 "sum", "min", "max", "avg"],
}

# A dimension applies only when another one has one of these values.
APPLIES: dict[str, tuple[str, tuple]] = {
    "c1.dir": ("c1.hops", (1, 2)), "c1.rel": ("c1.hops", (1, 2)), "c1.type": ("c1.hops", (1, 2)),
    "c2.hops": ("c2.kind", ("match", "optional")), "c2.dir": ("c2.kind", ("match", "optional")),
    "c2.rel": ("c2.kind", ("match", "optional")), "c2.type": ("c2.kind", ("match", "optional")),
    "c2.label": ("c2.kind", ("match", "optional")),
    "c3.hops": ("c3.kind", ("match", "optional")), "c3.dir": ("c3.kind", ("match", "optional")),
    "c3.rel": ("c3.kind", ("match", "optional")),
}

# Clause-structure dimensions: covered three at a time.
CORE = ["c1.hops", "c1.rel", "c2.kind", "c2.hops", "c2.rel", "with.kind",
        "c3.kind", "c3.rel", "ret.kind"]


class Chooser:
    """Answers each choice from the row when it names the dimension, else
    at random. Records the dimensions it was asked for."""

    def __init__(self, rng: random.Random, row: dict | None = None):
        self.rng = rng
        self.row = row or {}
        self.used: dict[str, object] = {}

    def pick(self, dim: str, options: list | None = None):
        options = options if options is not None else DOMAINS[dim]
        v = self.row[dim] if dim in self.row else self.rng.choice(options)
        self.used.setdefault(dim, v)
        return v

    # passthroughs for choices that are not dimensions
    def random(self) -> float:
        return self.rng.random()

    def randint(self, a: int, b: int) -> int:
        return self.rng.randint(a, b)

    def choice(self, seq):
        return self.rng.choice(seq)

    def sample(self, seq, k):
        return self.rng.sample(seq, k)


def _applicable(dims: list[str], vals: tuple) -> bool:
    """A tuple is required only if each of its dimensions applies given the
    other dimensions in the tuple."""
    given = dict(zip(dims, vals))
    for d in dims:
        if d in APPLIES:
            on, allowed = APPLIES[d]
            if on in given and given[on] not in allowed:
                return False
    return True


def _row_applies(row: dict, d: str) -> bool:
    if d not in APPLIES:
        return True
    on, allowed = APPLIES[d]
    return on not in row or row[on] in allowed


def covering(dims: list[str], seed: int = 0, core_strength: int = 3) -> list[dict]:
    """Greedy covering array: all pairs over `dims`, and all
    `core_strength`-tuples over the CORE dimensions among them."""
    rng = random.Random(seed)
    dims = list(dims)
    groups = [c for c in combinations(dims, 2)]
    core = [d for d in CORE if d in dims]
    if core_strength >= 3 and len(core) >= 3:
        groups += [c for c in combinations(core, core_strength)]
    uncovered: set = set()
    for g in groups:
        for vals in product(*(DOMAINS[d] for d in g)):
            if _applicable(list(g), vals):
                uncovered.add((g, vals))
    by_dim: dict[str, list] = {d: [g for g in groups if d in g] for d in dims}

    def covered_by(row: dict) -> set:
        out = set()
        for g in groups:
            if all(d in row and _row_applies(row, d) for d in g):
                t = (g, tuple(row[d] for d in g))
                if t in uncovered:
                    out.add(t)
        return out

    order = sorted(uncovered, key=repr)      # deterministic seed tuples
    pos = 0
    rows: list[dict] = []
    while uncovered:
        while order[pos] not in uncovered:
            pos += 1
        g0, v0 = order[pos]
        row = dict(zip(g0, v0))
        # Values a controlling dimension may take so that the seed tuple's
        # dimensions still apply.
        allowed: dict[str, tuple] = {}
        for d in g0:
            if d in APPLIES and APPLIES[d][0] in dims and APPLIES[d][0] not in row:
                allowed[APPLIES[d][0]] = APPLIES[d][1]
        rest = [d for d in dims if d not in row]
        rng.shuffle(rest)
        for d in rest:
            best, best_n = [], -1
            for v in allowed.get(d, DOMAINS[d]):
                row[d] = v
                n = 0
                for g in by_dim[d]:
                    if all(x in row for x in g):
                        if (g, tuple(row[x] for x in g)) in uncovered:
                            n += 1
                if n > best_n:
                    best, best_n = [v], n
                elif n == best_n:
                    best.append(v)
            row[d] = rng.choice(best)
        got = covered_by(row)
        if (g0, v0) not in got:         # the seed cannot apply: not required
            uncovered.discard((g0, v0))
        if got:
            uncovered -= got
            rows.append(row)
    return rows
