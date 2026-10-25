"""Metamorphic relations: query variants that must give the same result.

Each relation builds a Case from a random generator. A read case runs
every variant on one database holding the graph; a write case runs each
variant on its own fresh copy and compares a read-back query (or, for a
`read` variant, the rows it returns).
"""

from __future__ import annotations

import random
import re
from dataclasses import dataclass, field

from .gen import (aggregate, int_expr, node_label, pattern, rel, render,
                  scalar_expr, str_expr, variables)

# Words that are keywords of the parser but not reserved in openCypher:
# Neo4j takes each of them as a variable name.
SOFT_KEYWORDS = ["yield", "csv", "fieldterminator", "headers", "load", "from",
                 "call", "foreach", "any", "none", "single", "reduce",
                 "pattern", "explain", "profile", "index", "shortestPath",
                 "count", "filter", "extract", "key", "type", "node"]


@dataclass
class Variant:
    label: str
    query: str                       # rendered Cypher
    keymap: dict[str, str] = field(default_factory=dict)   # output key -> base key
    mode: str = "read"               # read | write (write: compare readback)


@dataclass
class Case:
    relation: str
    variants: list[Variant]
    write: bool = False
    readback: str | None = None


def _items(rng, nodes, rels, n=None) -> list[str]:
    n = n or rng.randint(1, 3)
    return [f"{scalar_expr(rng, nodes, rels)} AS c{i}" for i in range(n)]


# ------------------------------------------------------------------ chains

@dataclass
class Chain:
    clauses: list[str]
    nodes: list[str]
    rels: list[str]
    values: list[str]


def chain(rng: random.Random) -> Chain:
    """MATCH, then optional [OPTIONAL] MATCH, WITH + aggregate,
    [OPTIONAL] MATCH, and a RETURN."""
    p1 = pattern(rng, ["a", "b", "c"], ["r", "q"])
    clauses = [f"MATCH {p1.text}"]
    nodes, rels, values = list(p1.nodes), list(p1.rels), []
    if rng.random() < 0.7:
        p2 = pattern(rng, ["d", "e"], ["p", "t"], start=rng.choice(nodes),
                     hops=rng.choice([1, 2]))
        kw = "OPTIONAL MATCH" if rng.random() < 0.6 else "MATCH"
        clauses.append(f"{kw} {p2.text}")
        nodes += p2.nodes
        rels += p2.rels
    if rng.random() < 0.5:
        keep = rng.sample(nodes, rng.randint(1, len(nodes)))
        agg = aggregate(rng, nodes, rels)
        clauses.append("WITH " + ", ".join(f"⟨{v}⟩" for v in keep) + f", {agg} AS ⟨w⟩")
        nodes, rels, values = keep, [], ["w"]
    if rng.random() < 0.6:
        p3 = pattern(rng, ["f", "h"], ["u", "x"], start=rng.choice(nodes),
                     hops=rng.choice([1, 2]))
        kw = "OPTIONAL MATCH" if rng.random() < 0.6 else "MATCH"
        clauses.append(f"{kw} {p3.text}")
        nodes += p3.nodes
        rels += p3.rels
    items = _items(rng, nodes, rels)
    if values and rng.random() < 0.7:
        items.append(f"⟨w⟩ AS c{len(items)}")
    if rng.random() < 0.4:
        items.append(f"{aggregate(rng, nodes, rels)} AS c{len(items)}")
    clauses.append("RETURN " + ", ".join(items))
    return Chain(clauses, nodes, rels, values)


# --------------------------------------------------------------- relations

def with_star(rng: random.Random) -> Case:
    """A pass-through `WITH *` between two clauses changes nothing."""
    ch = chain(rng)
    base = " ".join(ch.clauses)
    at = rng.randint(1, len(ch.clauses) - 1)
    star = " ".join(ch.clauses[:at] + ["WITH *"] + ch.clauses[at:])
    return Case("with_star", [Variant("base", render(base)),
                              Variant("with *", render(star))])


PLAIN_NAMES = ["n", "m", "x0", "foo", "bar", "node1", "zz", "p1", "q1", "k",
                "src", "dst", "item", "v2", "rel1", "tmp", "row", "acc", "t1", "w2"]


def rename(rng: random.Random) -> Case:
    """Renaming every variable, by a permutation of the same names or by
    new ones, changes nothing."""
    ch = chain(rng)
    base = " ".join(ch.clauses)
    vs = variables(base)
    pool = vs if rng.random() < 0.5 else PLAIN_NAMES
    names = dict(zip(vs, rng.sample(pool, len(vs))))
    return Case("rename", [Variant("base", render(base)),
                           Variant("renamed", render(base, names))])


def keyword_name(rng: random.Random) -> Case:
    """A keyword that openCypher does not reserve works as a variable name."""
    ch = chain(rng)
    base = " ".join(ch.clauses)
    var = rng.choice(variables(base))
    kw = rng.choice(SOFT_KEYWORDS)
    return Case(f"keyword_name/{kw.lower()}",
                [Variant("base", render(base)),
                 Variant(f"{var} -> {kw}", render(base, {var: kw}))])


def alias_shadow(rng: random.Random) -> Case:
    """A RETURN alias that reuses a variable name changes only the key."""
    p = pattern(rng, ["a", "b", "c"], ["r", "q"])
    g = scalar_expr(rng, p.nodes, p.rels)
    agg = aggregate(rng, p.nodes, p.rels)
    base = f"MATCH {p.text} RETURN {g} AS c0, {agg} AS c1"
    var = rng.choice(p.nodes + p.rels)
    which = rng.choice(["c0", "c1"])
    shadow = base.replace(f"AS {which}", f"AS ⟨{var}⟩")
    return Case("alias_shadow", [
        Variant("base", render(base)),
        Variant(f"{which} AS {var}", render(shadow), keymap={var: which})])


def distinct_agg(rng: random.Random) -> Case:
    """collect/count(DISTINCT e) equals the aggregate over WITH DISTINCT."""
    p = pattern(rng, ["a", "b", "c"], ["r", "q"])
    e = scalar_expr(rng, p.nodes, p.rels)
    fn = rng.choice(["collect", "count"])
    if rng.random() < 0.6:
        g = scalar_expr(rng, p.nodes, p.rels)
        base = f"MATCH {p.text} RETURN {g} AS c0, {fn}(DISTINCT {e}) AS c1"
        alt = f"MATCH {p.text} WITH DISTINCT {g} AS c0, {e} AS x RETURN c0, {fn}(x) AS c1"
    else:
        base = f"MATCH {p.text} RETURN {fn}(DISTINCT {e}) AS c1"
        alt = f"MATCH {p.text} WITH DISTINCT {e} AS x RETURN {fn}(x) AS c1"
    vs = [Variant("base", render(base)), Variant("with distinct", render(alt))]
    if fn == "count":
        vs.append(Variant("size(collect)", render(base.replace(
            f"count(DISTINCT {e})", f"size(collect(DISTINCT {e}))"))))
    return Case("distinct_agg", vs)


def label_disjunction(rng: random.Random) -> Case:
    """`:A|B` equals `WHERE x:A OR x:B`, in a MATCH, a pattern predicate and
    a pattern comprehension."""
    where = rng.choice(["match", "predicate", "comprehension"])
    if where == "match":
        p = pattern(rng, ["a", "b", "c"], ["r", "q"], disjunction=False)
        v = rng.choice(p.nodes)
        plain = re.sub(rf"\(⟨{v}⟩[^)]*\)", f"(⟨{v}⟩)", p.text, count=1)
        disj = re.sub(rf"\(⟨{v}⟩[^)]*\)", f"(⟨{v}⟩:A|B)", p.text, count=1)
        items = ", ".join(_items(rng, p.nodes, p.rels))
        base = f"MATCH {disj} RETURN {items}"
        alt = f"MATCH {plain} WHERE ⟨{v}⟩:A OR ⟨{v}⟩:B RETURN {items}"
    else:
        lab = node_label(rng, disjunction=False)
        step = rel(rng, None)
        if where == "predicate":
            base = f"MATCH (⟨a⟩{lab}) WHERE (⟨a⟩){step}(:A|B) RETURN ⟨a⟩.id AS c0"
            alt = (f"MATCH (⟨a⟩{lab}) WHERE EXISTS {{ MATCH (⟨a⟩){step}(⟨m⟩) "
                   f"WHERE ⟨m⟩:A OR ⟨m⟩:B }} RETURN ⟨a⟩.id AS c0")
        else:
            base = f"MATCH (⟨a⟩{lab}) RETURN ⟨a⟩.id AS c0, [(⟨a⟩){step}(⟨m⟩:A|B) | ⟨m⟩.id] AS c1"
            alt = (f"MATCH (⟨a⟩{lab}) RETURN ⟨a⟩.id AS c0, "
                   f"[(⟨a⟩){step}(⟨m⟩) WHERE ⟨m⟩:A OR ⟨m⟩:B | ⟨m⟩.id] AS c1")
    return Case(f"label_disjunction/{where}",
                [Variant("A|B", render(base)), Variant("OR", render(alt))])


def optional_filter(rng: random.Random) -> Case:
    """OPTIONAL MATCH filtered to matched rows equals MATCH."""
    p1 = pattern(rng, ["a", "b", "c"], ["r", "q"])
    p2 = pattern(rng, ["d", "e"], ["p", "t"], start=rng.choice(p1.nodes),
                 hops=rng.choice([1, 2]))
    nodes, rels = p1.nodes + p2.nodes, p1.rels + p2.rels
    items = ", ".join(_items(rng, nodes, rels))
    new = p2.nodes[-1]
    base = f"MATCH {p1.text} MATCH {p2.text} RETURN {items}"
    alt = (f"MATCH {p1.text} OPTIONAL MATCH {p2.text} "
           f"WITH * WHERE ⟨{new}⟩ IS NOT NULL RETURN {items}")
    return Case("optional_filter", [Variant("match", render(base)),
                                    Variant("optional+filter", render(alt))])


def split_path(rng: random.Random) -> Case:
    """A two-hop pattern equals two MATCH clauses with distinct edges."""
    l1, l2, l3 = (node_label(rng) for _ in range(3))
    r1, r2 = rel(rng, "r"), rel(rng, "q")
    nodes, rels = ["a", "b", "c"], ["r", "q"]
    items = ", ".join(_items(rng, nodes, rels))
    base = f"MATCH (⟨a⟩{l1}){r1}(⟨b⟩{l2}){r2}(⟨c⟩{l3}) RETURN {items}"
    alt = (f"MATCH (⟨a⟩{l1}){r1}(⟨b⟩{l2}) MATCH (⟨b⟩){r2}(⟨c⟩{l3}) "
           f"WHERE id(⟨r⟩) <> id(⟨q⟩) RETURN {items}")
    return Case("split_path", [Variant("one", render(base)),
                               Variant("two", render(alt))])


def inline_props(rng: random.Random) -> Case:
    """`(a {v: k})` equals `(a) WHERE a.v = k`."""
    p = pattern(rng, ["a", "b", "c"], ["r", "q"])
    v = rng.choice(p.nodes)
    key, val = rng.choice([("v", str(rng.randint(0, 3))),
                           ("s", repr(rng.choice(["x", "y"])))])
    m = re.search(rf"\(⟨{v}⟩([^)]*)\)", p.text)
    inline = p.text.replace(m.group(0), f"(⟨{v}⟩{m.group(1)} {{{key}: {val}}})", 1)
    items = ", ".join(_items(rng, p.nodes, p.rels))
    base = f"MATCH {inline} RETURN {items}"
    alt = f"MATCH {p.text} WHERE ⟨{v}⟩.{key} = {val} RETURN {items}"
    return Case("inline_props", [Variant("inline", render(base)),
                                 Variant("where", render(alt))])


def write_expression(rng: random.Random) -> Case:
    """A property expression written inline equals the same value computed
    in a WITH first, and equals what a RETURN of it gives."""
    shape = rng.choice(["match", "match_unwind", "unwind", "match_with"])
    p = pattern(rng, ["a", "b"], ["r"], hops=rng.choice([0, 1]))
    nodes, rels = list(p.nodes), list(p.rels)
    if shape == "unwind":
        pre, nodes, rels, scope = "UNWIND [1, 2] AS ⟨i⟩", [], [], ["i"]
    elif shape == "match":
        pre, scope = f"MATCH {p.text}", nodes + rels
    elif shape == "match_unwind":
        pre, scope = f"MATCH {p.text} UNWIND [1, 2] AS ⟨i⟩", nodes + rels + ["i"]
    else:
        pre, scope = f"MATCH {p.text} WITH {', '.join(f'⟨{v}⟩' for v in nodes + rels)}", nodes + rels
    kinds = {}
    if nodes:
        n0 = f"⟨{nodes[0]}⟩"
        kinds = {"scalar": scalar_expr(rng, nodes, rels),
                 "case": f"CASE WHEN {n0}.v > 1 THEN 'a' ELSE 'b' END",
                 "list": f"[{n0}.id, 1]",
                 "map_value": f"{{x: {n0}.id}}.x"}
        if "i" in scope:
            kinds.update({"case": "CASE WHEN ⟨i⟩ = 1 THEN 'a' ELSE 'b' END",
                          "arith": f"{n0}.id * 10 + ⟨i⟩"})
    else:
        kinds = {"case": "CASE WHEN ⟨i⟩ = 1 THEN 'a' ELSE 'b' END",
                 "arith": "⟨i⟩ * 2", "scalar": "toString(⟨i⟩)", "list": "[⟨i⟩, 1]",
                 "map_value": "{x: ⟨i⟩}.x"}
    kind = rng.choice(sorted(kinds))
    e = kinds[kind]
    keep = ", ".join(f"⟨{v}⟩" for v in scope)
    inline = f"{pre} CREATE (:New {{k: {e}}})"
    staged = f"{pre} WITH {keep}, {e} AS ⟨val⟩ CREATE (:New {{k: ⟨val⟩}})"
    read = f"{pre} RETURN {e} AS k"
    return Case(f"write_expression/{kind}/{shape}", [
        Variant("inline", render(inline), mode="write"),
        Variant("with", render(staged), mode="write"),
        Variant("return", render(read))],
        write=True, readback="MATCH (n:New) RETURN n.k AS k")


RELATIONS = {
    "with_star": with_star,
    "rename": rename,
    "keyword_name": keyword_name,
    "alias_shadow": alias_shadow,
    "distinct_agg": distinct_agg,
    "label_disjunction": label_disjunction,
    "optional_filter": optional_filter,
    "split_path": split_path,
    "inline_props": inline_props,
    "write_expression": write_expression,
}
