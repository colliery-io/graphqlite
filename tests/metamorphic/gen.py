"""Random graphs, patterns, expressions and predicates.

Queries are templates: a variable is written ⟨a⟩ and rendered through a
name map, so a relation can rename every variable (to another name, or to
a keyword) without parsing Cypher. Every choice goes through a Chooser
(space.py), by dimension name where the choice is a dimension.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field

from .space import Chooser

LABELS = ["A", "B", "C"]
TYPES = ["R", "S"]
STRINGS = ["x", "y", "z", "xy"]

_VAR = re.compile(r"⟨(\w+)⟩")


def render(template: str, names: dict[str, str] | None = None) -> str:
    names = names or {}
    return _VAR.sub(lambda m: names.get(m.group(1), m.group(1)), template)


def variables(template: str) -> list[str]:
    seen: list[str] = []
    for m in _VAR.finditer(template):
        if m.group(1) not in seen:
            seen.append(m.group(1))
    return seen


def v(name: str) -> str:
    return f"⟨{name}⟩"


# --------------------------------------------------------------------- graph

@dataclass
class Graph:
    nodes: list[dict] = field(default_factory=list)   # {id, labels, props}
    edges: list[dict] = field(default_factory=list)   # {src, dst, type, props}

    def create(self) -> str | None:
        if not self.nodes:
            return None
        parts = []
        for n in self.nodes:
            labels = "".join(":" + l for l in n["labels"])
            props = ", ".join(f"{k}: {lit(x)}" for k, x in n["props"].items())
            parts.append(f"(n{n['id']}{labels} {{{props}}})")
        for e in self.edges:
            props = ", ".join(f"{k}: {lit(x)}" for k, x in e["props"].items())
            parts.append(f"(n{e['src']})-[:{e['type']} {{{props}}}]->(n{e['dst']})")
        return "CREATE " + ", ".join(parts)

    def without_edge(self, i: int) -> "Graph":
        return Graph(list(self.nodes), self.edges[:i] + self.edges[i + 1:])

    def without_node(self, nid: int) -> "Graph":
        return Graph([n for n in self.nodes if n["id"] != nid],
                     [e for e in self.edges if nid not in (e["src"], e["dst"])])


def lit(x) -> str:
    if x is None:
        return "null"
    if isinstance(x, bool):
        return "true" if x else "false"
    if isinstance(x, str):
        return "'" + x.replace("\\", "\\\\").replace("'", "\\'") + "'"
    if isinstance(x, list):
        return "[" + ", ".join(lit(y) for y in x) + "]"
    return repr(x)


def random_graph(ch: Chooser) -> Graph:
    shape = ch.pick("g.shape")
    p_prop = 0.85 if ch.pick("g.nulls") == "few" else 0.4
    g = Graph()
    n = {"empty": 0, "isolated": ch.randint(1, 5)}.get(shape, ch.randint(1, 7))
    for i in range(n):
        props = {"id": i}
        if ch.random() < p_prop:
            props["v"] = ch.randint(0, 3)
        if ch.random() < p_prop:
            props["s"] = ch.choice(STRINGS)
        if ch.random() < p_prop / 3:
            props["b"] = ch.random() < 0.5
        labels = [l for l in LABELS if ch.random() < 0.45]
        g.nodes.append({"id": i, "labels": labels, "props": props})

    def edge(src, dst):
        props = {"w": ch.randint(0, 3)} if ch.random() < p_prop else {}
        g.edges.append({"src": src, "dst": dst, "type": ch.choice(TYPES), "props": props})

    ids = [x["id"] for x in g.nodes]
    if shape == "selfloops":
        for i in ids:
            if ch.random() < 0.6:
                edge(i, i)
        for _ in range(len(ids)):
            edge(ch.choice(ids), ch.choice(ids))
    elif shape == "multi":
        for _ in range(len(ids)):
            a, b = ch.choice(ids), ch.choice(ids)
            for _ in range(ch.randint(2, 3)):
                edge(a, b)
    elif shape == "chain":
        for a, b in zip(ids, ids[1:]):
            edge(a, b) if ch.random() < 0.7 else edge(b, a)
    elif shape == "star":
        for b in ids[1:]:
            edge(ids[0], b) if ch.random() < 0.6 else edge(b, ids[0])
    elif shape == "random":
        for _ in range(ch.randint(0, 2 * len(ids))):
            edge(ch.choice(ids), ch.choice(ids))
    elif shape == "dense":
        for a in ids:
            for b in ids:
                if ch.random() < 0.5:
                    edge(a, b)
    return g


# ------------------------------------------------------------------ patterns

def label_text(kind: str, ch: Chooser) -> str:
    if kind == "none":
        return ""
    if kind == "one":
        return ":" + ch.choice(LABELS[:2])
    if kind == "multi":
        return ":A:B" if ch.random() < 0.5 else ":A:C"
    return ":A|B" if ch.random() < 0.7 else ":B|C"


def label_where(label: str, var: str) -> str:
    """The predicate that a label expression in a pattern means."""
    if not label:
        return "true"
    if "|" in label:
        return "(" + " OR ".join(f"{v(var)}:{l}" for l in label[1:].split("|")) + ")"
    return f"{v(var)}{label}"


@dataclass
class Step:
    rel: str | None        # variable, or None (anonymous)
    type: str              # '', ':R', ':R|S'
    dir: str               # '->', '<-', '-'


@dataclass
class Pattern:
    nodes: list[tuple[str, str]]      # (var, label text); first may be a bound start
    steps: list[Step]
    bound_start: bool = False

    @property
    def node_vars(self) -> list[str]:
        return [n for n, _ in self.nodes[1:]] if self.bound_start else [n for n, _ in self.nodes]

    @property
    def rel_vars(self) -> list[str]:
        return [s.rel for s in self.steps if s.rel]

    def text(self, reverse: bool = False) -> str:
        nodes, steps = self.nodes, self.steps
        if reverse:
            nodes = list(reversed(nodes))
            flip = {"->": "<-", "<-": "->", "-": "-"}
            steps = [Step(s.rel, s.type, flip[s.dir]) for s in reversed(steps)]
        out = _node(nodes[0], bare=self.bound_start and not reverse)
        for i, s in enumerate(steps):
            body = f"[{v(s.rel) if s.rel else ''}{s.type}]"
            out += {"->": f"-{body}->", "<-": f"<-{body}-", "-": f"-{body}-"}[s.dir]
            last = i + 1 == len(steps)
            out += _node(nodes[i + 1], bare=self.bound_start and reverse and last)
        return out


def _node(n: tuple[str, str], bare: bool) -> str:
    var, label = n
    return f"({v(var)})" if bare else f"({v(var)}{label})"


def pattern(ch: Chooser, prefix: str, nodes: list[str], rels: list[str],
            start: str | None = None, hops: int | None = None) -> Pattern:
    """A path over fresh variable names; `start` is a bound node variable.
    Direction, relationship naming, type and labels are dimensions of the
    clause `prefix` (one value for each hop of the clause)."""
    if hops is None:
        hops = ch.pick(f"{prefix}.hops", [0, 1, 2] if not start else [1, 2])
    hops = min(hops, len(rels), len(nodes) - (0 if start else 1))
    label = ch.pick(f"{prefix}.label", ["none", "one", "multi", "disj"])
    fresh = list(nodes)
    pnodes = [(start, "")] if start else [(fresh.pop(0), label_text(label, ch))]
    steps = []
    for h in range(hops):
        named = ch.pick(f"{prefix}.rel", ["named", "named", "anon"]) == "named"
        t = ch.pick(f"{prefix}.type", ["any", "one", "alt"])
        steps.append(Step(rels[h] if named else None,
                          {"any": "", "one": ":" + ch.choice(TYPES), "alt": ":R|S"}[t],
                          ch.pick(f"{prefix}.dir", ["->", "<-", "-"])))
        pnodes.append((fresh.pop(0), label_text(label, ch) if ch.random() < 0.6 else ""))
    return Pattern(pnodes, steps, bound_start=bool(start))


# --------------------------------------------------------------- expressions

def int_expr(ch: Chooser, nodes: list[str], rels: list[str]) -> str:
    n = v(ch.choice(nodes))
    opts = [f"{n}.v", f"{n}.id", f"{n}.v + 1", f"{n}.id % 2"]
    if rels:
        opts.append(f"{v(ch.choice(rels))}.w")
    return ch.choice(opts)


def expr(ch: Chooser, nodes: list[str], rels: list[str], kind: str | None = None,
         dim: str = "expr.kind") -> str:
    """A scalar expression of the kind chosen for `dim`."""
    kind = kind or ch.pick(dim)
    n = v(ch.choice(nodes))
    r = v(ch.choice(rels)) if rels else None
    m = v(ch.choice(nodes))
    table = {
        "prop": [f"{n}.v", f"{n}.s", f"{n}.id", f"{n}.b"] + ([f"{r}.w"] if r else []),
        "arith": [f"{n}.v + 1", f"{n}.id % 2", f"{n}.v * 2 - {n}.id", f"{n}.v + {m}.v",
                  f"-{n}.v", f"{n}.id / 2", f"{n}.v ^ 2"],
        "case": [f"CASE WHEN {n}.v > 1 THEN 'hi' ELSE 'lo' END",
                 f"CASE {n}.s WHEN 'x' THEN 1 WHEN 'y' THEN 2 ELSE 0 END",
                 f"CASE WHEN {n}.v IS NULL THEN -1 WHEN {n}.v > 2 THEN 1 END"],
        "func": [f"coalesce({n}.v, -1)", f"toString({n}.id)", f"size({n}.s)",
                 f"toUpper({n}.s)", f"abs({n}.v - 2)", f"toInteger({n}.s)",
                 f"coalesce({n}.s, {m}.s, 'none')"],
        "list": [f"[{n}.id, 1]", f"[x IN [1, 2, 3] WHERE x > {n}.v | x * 2]",
                 f"range(0, {n}.id)", f"size([{n}.v, {m}.v])", f"[{n}.id, {m}.id][0]",
                 f"reverse([{n}.id, 2])"],
        "map": [f"{{x: {n}.v}}.x", f"{{a: {n}.id, b: {n}.s}}", f"properties({n}).v"],
        "nullcheck": [f"{n}.v IS NULL", f"{n}.s IS NOT NULL", f"{n}.v = {m}.v"],
        "entity": [f"labels({n})", f"size(keys({n}))", f"{n}:A"]
                  + ([f"type({r})", f"startNode({r}).id"] if r else []),
        "string": [f"{n}.s + 'q'", f"{n}.s STARTS WITH 'x'", f"{n}.s CONTAINS 'y'",
                   f"substring({n}.s, 0, 1)", f"{n}.s =~ 'x.*'"],
    }
    return ch.choice(table[kind])


def predicate(ch: Chooser, nodes: list[str], rels: list[str], kind: str | None = None) -> str:
    """A three-valued predicate of the kind chosen for pred.kind."""
    kind = kind or ch.pick("pred.kind")
    n = v(ch.choice(nodes))
    m = v(ch.choice(nodes))
    if kind == "cmp":
        opts = [f"{n}.v > 1", f"{n}.v = {m}.v", f"{n}.id <> {m}.id", f"{n}.v <= {m}.id",
                f"{n}.s < 'y'"]
        if rels:
            opts.append(f"{v(ch.choice(rels))}.w >= 2")
        return ch.choice(opts)
    if kind == "null":
        return ch.choice([f"{n}.v IS NULL", f"{n}.s IS NOT NULL", f"{n}.b IS NULL"])
    if kind == "bool":
        a = predicate(ch, nodes, rels, "cmp")
        b = predicate(ch, nodes, rels, ch.choice(["null", "cmp", "in"]))
        return ch.choice([f"({a} AND {b})", f"({a} OR NOT {b})", f"({a} XOR {b})",
                          f"NOT ({a} OR {b})", f"{n}.b"])
    if kind == "pattern":
        return ch.choice([f"({n})-->()", f"NOT ({n})<-[:R]-()", f"({n})-[:S]-(:A)",
                          f"EXISTS {{ MATCH ({n})-->(zq) WHERE zq.v > 1 }}"])
    if kind == "in":
        return ch.choice([f"{n}.v IN [1, 2]", f"{n}.s IN ['x', null]", f"{n}.id IN []",
                          f"{n}.v IN [{m}.v, 3]"])
    if kind == "string":
        return ch.choice([f"{n}.s STARTS WITH 'x'", f"{n}.s ENDS WITH 'y'",
                          f"{n}.s CONTAINS 'x'", f"{n}.s =~ '.y'"])
    return ch.choice([f"{n}:A", f"{n}:A:B", f"({n}:A OR {n}:C)", f"NOT {n}:B"])


AGGS = {
    "count_star": lambda e, i, n: "count(*)",
    "count": lambda e, i, n: f"count({e})",
    "count_distinct": lambda e, i, n: f"count(DISTINCT {e})",
    "collect": lambda e, i, n: f"collect({e})",
    "collect_distinct": lambda e, i, n: f"collect(DISTINCT {e})",
    "sum": lambda e, i, n: f"sum({i})",
    "min": lambda e, i, n: f"min({i})",
    "max": lambda e, i, n: f"max({e})",
    "avg": lambda e, i, n: f"avg({i})",
}


def aggregate(ch: Chooser, nodes: list[str], rels: list[str], kind: str | None = None) -> str:
    kind = kind or ch.pick("agg.kind")
    e = expr(ch, nodes, rels, kind=ch.choice(["prop", "arith", "case", "func", "string"]))
    i = int_expr(ch, nodes, rels)
    return AGGS[kind](e, i, v(ch.choice(nodes)))
