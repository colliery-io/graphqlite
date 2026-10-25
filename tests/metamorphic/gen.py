"""Random graphs, patterns and expressions.

Queries are templates: a variable is written ⟨a⟩ and rendered through a
name map, so a relation can rename every variable (to another name, or to
a keyword) without parsing Cypher.
"""

from __future__ import annotations

import random
import re
from dataclasses import dataclass, field

LABELS = ["A", "B", "C"]
TYPES = ["R", "S"]
STRINGS = ["x", "y", "z"]

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
            props = ", ".join(f"{k}: {lit(v)}" for k, v in n["props"].items())
            parts.append(f"(n{n['id']}{labels} {{{props}}})")
        for e in self.edges:
            props = ", ".join(f"{k}: {lit(v)}" for k, v in e["props"].items())
            parts.append(f"(n{e['src']})-[:{e['type']} {{{props}}}]->(n{e['dst']})")
        return "CREATE " + ", ".join(parts)

    def without_edge(self, i: int) -> "Graph":
        return Graph(list(self.nodes), self.edges[:i] + self.edges[i + 1:])

    def without_node(self, nid: int) -> "Graph":
        return Graph([n for n in self.nodes if n["id"] != nid],
                     [e for e in self.edges if nid not in (e["src"], e["dst"])])


def lit(v) -> str:
    if v is None:
        return "null"
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, str):
        return "'" + v.replace("\\", "\\\\").replace("'", "\\'") + "'"
    if isinstance(v, list):
        return "[" + ", ".join(lit(x) for x in v) + "]"
    return repr(v)


def random_graph(rng: random.Random) -> Graph:
    g = Graph()
    for i in range(rng.randint(0, 8)):
        props = {"id": i}
        if rng.random() < 0.8:
            props["v"] = rng.randint(0, 3)
        if rng.random() < 0.7:
            props["s"] = rng.choice(STRINGS)
        labels = [l for l in LABELS if rng.random() < 0.4]
        g.nodes.append({"id": i, "labels": labels, "props": props})
    if g.nodes:
        for _ in range(rng.randint(0, 2 * len(g.nodes))):
            props = {"w": rng.randint(0, 3)} if rng.random() < 0.8 else {}
            g.edges.append({"src": rng.choice(g.nodes)["id"],
                            "dst": rng.choice(g.nodes)["id"],
                            "type": rng.choice(TYPES), "props": props})
    return g


# ------------------------------------------------------------------ patterns

def node_label(rng: random.Random, disjunction: bool = True) -> str:
    opts = ["", "", ":A", ":B"] + ([":A|B"] if disjunction else [])
    return rng.choice(opts)


def rel(rng: random.Random, var: str | None) -> str:
    """A relationship step such as '-[⟨r⟩:R]->'; no variable when var is None."""
    t = rng.choice(["", ":R", ":S", ":R|S"])
    body = f"[⟨{var}⟩{t}]" if var else f"[{t}]"
    d = rng.choice(["->", "<-", "-"])
    if d == "->":
        return f"-{body}->"
    if d == "<-":
        return f"<-{body}-"
    return f"-{body}-"


@dataclass
class Pattern:
    text: str
    nodes: list[str]
    rels: list[str]


def pattern(rng: random.Random, nodes: list[str], rels: list[str],
            start: str | None = None, hops: int | None = None,
            disjunction: bool = True) -> Pattern:
    """A path pattern over the given fresh variable names. `start` is an
    already-bound node variable written without a label."""
    hops = rng.choice([0, 1, 1, 2]) if hops is None else hops
    hops = min(hops, len(nodes) - (0 if start else 1), len(rels))
    used_nodes = []
    if start:
        text = f"(⟨{start}⟩)"
    else:
        v = nodes[0]
        used_nodes.append(v)
        text = f"(⟨{v}⟩{node_label(rng, disjunction)})"
    fresh = [n for n in nodes if n not in used_nodes]
    used_rels = []
    for h in range(hops):
        r, v = rels[h], fresh[h]
        if rng.random() < 0.25:
            r = None                      # anonymous relationship
        else:
            used_rels.append(r)
        used_nodes.append(v)
        text += rel(rng, r) + f"(⟨{v}⟩{node_label(rng, disjunction)})"
    return Pattern(text, used_nodes, used_rels)


# --------------------------------------------------------------- expressions

def int_expr(rng: random.Random, nodes: list[str], rels: list[str]) -> str:
    n = f"⟨{rng.choice(nodes)}⟩"
    opts = [f"{n}.v", f"{n}.id", f"{n}.v + 1", f"coalesce({n}.v, -1)",
            f"CASE WHEN {n}.v > 1 THEN 10 ELSE 20 END",
            f"CASE {n}.s WHEN 'x' THEN 1 WHEN 'y' THEN 2 ELSE 0 END",
            f"{n}.id % 2", "size(labels(" + n + "))"]
    if rels:
        opts.append(f"⟨{rng.choice(rels)}⟩.w")
    return rng.choice(opts)


def str_expr(rng: random.Random, nodes: list[str], rels: list[str]) -> str:
    n = f"⟨{rng.choice(nodes)}⟩"
    opts = [f"{n}.s", f"toString({n}.id)", f"{n}.s + 'q'",
            f"CASE WHEN {n}.v > 1 THEN 'hi' ELSE 'lo' END",
            f"coalesce({n}.s, 'none')"]
    if rels:
        opts.append(f"type(⟨{rng.choice(rels)}⟩)")
    return rng.choice(opts)


def scalar_expr(rng: random.Random, nodes: list[str], rels: list[str]) -> str:
    return (int_expr if rng.random() < 0.5 else str_expr)(rng, nodes, rels)


def aggregate(rng: random.Random, nodes: list[str], rels: list[str]) -> str:
    e = scalar_expr(rng, nodes, rels)
    i = int_expr(rng, nodes, rels)
    n = f"⟨{rng.choice(nodes)}⟩"
    return rng.choice([
        "count(*)", f"count({n})", f"count({e})", f"count(DISTINCT {e})",
        f"collect({e})", f"collect(DISTINCT {e})", f"sum({i})",
        f"min({i})", f"max({e})"])
