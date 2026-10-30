"""Metamorphic relations: query variants that must give the same result.

Each relation takes a Chooser and builds a Case. A read case runs every
variant on one database holding the graph; a write case runs each variant
on its own fresh copy and compares a read-back query (a `read` variant in a
write case compares the rows it returns). A variant can be several queries
(`parts`) whose rows are combined, and can post-process its rows.

RELATIONS maps a name to (builder, dimensions read): cover mode builds a
covering array over those dimensions for each relation.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from typing import Callable

from .gen import (Pattern, Step, aggregate, expr, int_expr, label_text, label_where,
                  pattern, predicate, render, v, variables)
from .space import Chooser, DOMAINS

# Words that are keywords of the parser but not reserved in openCypher:
# Neo4j takes each of them as a variable name.
SOFT_KEYWORDS = ["yield", "csv", "fieldterminator", "headers", "load", "from",
                 "call", "foreach", "any", "none", "single", "reduce",
                 "pattern", "explain", "profile", "index", "shortestPath",
                 "count", "filter", "extract", "key", "type", "node"]
PLAIN_NAMES = ["n", "m", "x0", "foo", "bar", "node1", "zz", "p1", "q1", "k",
               "src", "dst", "item", "v2", "rel1", "tmp", "row", "acc", "t1", "w2"]

DOMAINS.update({
    "ld.form": ["match", "predicate", "comprehension"],
    "w.shape": ["create", "match", "match_unwind", "unwind", "match_with"],
    "w.kind": ["prop", "arith", "case", "func", "list", "map_value", "nullcheck", "string"],
    "vl.range": ["*1..1", "*1..2", "*2..2", "*1..3"],
    "rp.clause": ["match", "optional"],
    "del.form": ["node", "rel"],
    "om.form": ["null_rows", "count"],
})


@dataclass
class Variant:
    label: str
    query: str | None = None
    parts: list[str] | None = None          # queries whose rows are combined
    combine: Callable | None = None         # list of row lists -> rows
    post: Callable | None = None            # rows -> rows
    keymap: dict[str, str] = field(default_factory=dict)
    mode: str = "read"                      # read | write (write: compare readback)
    repeat: int = 1

    @property
    def queries(self) -> list[str]:
        return self.parts if self.parts else [self.query]


@dataclass
class Case:
    relation: str
    variants: list[Variant]
    write: bool = False
    readback: str | None = None
    compare: str = "bag"                    # bag | set | list


def concat(parts: list[list]) -> list:
    return [r for rows in parts for r in rows]


def sum_columns(parts: list[list]) -> list:
    """One row: each column summed over the single rows of the parts
    (null counts as 0, as sum() of no rows is 0)."""
    out: dict = {}
    for rows in parts:
        for r in rows:
            for k, x in r.items():
                out[k] = out.get(k, 0) + (x or 0)
    return [out]


def _equiv(v):
    """Cypher equivalence for DISTINCT and UNION: 1 and 1.0 are one value."""
    if isinstance(v, list):
        return [_equiv(x) for x in v]
    if isinstance(v, dict):
        return {k: _equiv(x) for k, x in v.items()}
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def equiv(rows: list) -> list:
    return [_equiv(r) for r in rows]


def dedupe(rows: list) -> list:
    """Rows once, under Cypher equivalence (numbers as equiv() gives them:
    which of 1 and 1.0 a DISTINCT keeps is not specified)."""
    seen, out = set(), []
    for r in equiv(rows):
        k = json.dumps(r, sort_keys=True)
        if k not in seen:
            seen.add(k)
            out.append(r)
    return out


G = ["g.shape", "g.nulls"]
C1 = ["c1.hops", "c1.dir", "c1.rel", "c1.type", "c1.label"]
C2 = ["c2.kind", "c2.hops", "c2.dir", "c2.rel", "c2.type", "c2.label"]
C2_FORCED = ["c2.hops", "c2.dir", "c2.rel", "c2.type", "c2.label"]
C3 = ["c3.kind", "c3.hops", "c3.dir", "c3.rel"]
CHAIN = G + C1 + C2 + ["with.kind"] + C3 + ["ret.kind", "expr.kind", "agg.kind"]
PRED_KINDS = DOMAINS["pred.kind"]
EXPR_KINDS = DOMAINS["expr.kind"]
THREE = lambda p: (p, f"NOT ({p})", f"({p}) IS NULL")


# ------------------------------------------------------------------ chains

@dataclass
class Chain:
    body: list[str]
    nodes: list[str]
    rels: list[str]
    values: list[str]
    items: list[str]
    distinct: bool

    def query(self, extra: list[str] | None = None, items: list[str] | None = None,
              distinct: bool | None = None, tail: str = "") -> str:
        d = self.distinct if distinct is None else distinct
        ret = "RETURN " + ("DISTINCT " if d else "") + ", ".join(items or self.items)
        return " ".join(self.body + (extra or []) + [ret]) + tail


def _item(ch: Chooser, nodes, rels, values, first: bool) -> str:
    if values and ch.random() < 0.3:
        return v(ch.choice(values))
    return expr(ch, nodes, rels, kind=None if first else ch.choice(EXPR_KINDS))


def chain(ch: Chooser, n_items: int | None = None, ret_kinds: list | None = None) -> Chain:
    """MATCH, then [OPTIONAL] MATCH, a WITH, [OPTIONAL] MATCH or UNWIND,
    and a RETURN, each shaped by its dimensions."""
    p1 = pattern(ch, "c1", ["a", "b", "c"], ["r", "q"])
    body = [f"MATCH {p1.text()}"]
    nodes, rels, values = p1.node_vars, p1.rel_vars, []
    if ch.random() < 0.25:
        body[-1] += f" WHERE {predicate(ch, nodes, rels, kind=ch.choice(PRED_KINDS))}"
    k2 = ch.pick("c2.kind")
    if k2 != "none":
        p2 = pattern(ch, "c2", ["d", "e"], ["p", "t"], start=ch.choice(nodes))
        body.append(("OPTIONAL MATCH " if k2 == "optional" else "MATCH ") + p2.text())
        nodes, rels = nodes + p2.node_vars, rels + p2.rel_vars
    wk = ch.pick("with.kind")
    if wk == "pass":
        body.append("WITH *")
    elif wk == "agg":
        keep = ch.sample(nodes, ch.randint(1, len(nodes)))
        body.append("WITH " + ", ".join(v(x) for x in keep) +
                    f", {aggregate(ch, nodes, rels)} AS {v('w')}")
        nodes, rels, values = keep, [], ["w"]
    elif wk == "distinct":
        keep = ch.sample(nodes, ch.randint(1, len(nodes)))
        body.append("WITH DISTINCT " + ", ".join(v(x) for x in keep))
        nodes, rels = keep, []
    elif wk == "where":
        body.append(f"WITH * WHERE {predicate(ch, nodes, rels, kind=ch.choice(PRED_KINDS))}")
    elif wk == "orderlimit":
        keys = ", ".join(f"id({v(x)})" for x in nodes + rels)
        body.append(f"WITH * ORDER BY {keys} SKIP {ch.randint(0, 1)} LIMIT {ch.randint(1, 4)}")
    k3 = ch.pick("c3.kind")
    if k3 in ("match", "optional"):
        p3 = pattern(ch, "c3", ["f", "h"], ["u", "x"], start=ch.choice(nodes))
        body.append(("OPTIONAL MATCH " if k3 == "optional" else "MATCH ") + p3.text())
        nodes, rels = nodes + p3.node_vars, rels + p3.rel_vars
    elif k3 == "unwind":
        n = v(ch.choice(nodes))
        src = ch.choice(["[1, 2]", f"range(0, coalesce({n}.v, 1))", f"[{n}.id, null]", "[]"])
        body.append(f"UNWIND {src} AS {v('i')}")
        values = values + ["i"]
    rk = ch.pick("ret.kind", ret_kinds) if ret_kinds else ch.pick("ret.kind")
    n = n_items or ch.randint(1, 3)
    if rk in ("scalar", "distinct"):
        exprs = [_item(ch, nodes, rels, values, i == 0) for i in range(n)]
    elif rk == "agg":
        exprs = [aggregate(ch, nodes, rels, kind=None if i == 0 else ch.choice(DOMAINS["agg.kind"]))
                 for i in range(n)]
    else:
        exprs = [_item(ch, nodes, rels, values, i == 0) for i in range(n - 1)]
        exprs.append(aggregate(ch, nodes, rels))
    items = [f"{e} AS c{i}" for i, e in enumerate(exprs)]
    return Chain(body, nodes, rels, values, items, rk == "distinct")


def _items(ch, nodes, rels, n=None) -> str:
    n = n or ch.randint(1, 3)
    return ", ".join(f"{expr(ch, nodes, rels, kind=None if i == 0 else ch.choice(EXPR_KINDS))} AS c{i}"
                     for i in range(n))


def _named(p: Pattern, names=("r", "q", "r2", "q2")) -> Pattern:
    """The pattern with every relationship named (for id comparisons)."""
    free = iter(n for n in names if n not in p.rel_vars)
    return Pattern(p.nodes, [Step(s.rel or next(free), s.type, s.dir) for s in p.steps],
                   p.bound_start)


def _arrow(d: str, body: str) -> str:
    return {"->": f"-[{body}]->", "<-": f"<-[{body}]-", "-": f"-[{body}]-"}[d]


# ---------------------------------------------------------- query structure

def with_star(ch: Chooser) -> Case:
    """A pass-through `WITH *` between two clauses changes nothing."""
    c = chain(ch)
    at = ch.randint(1, len(c.body))
    star = Chain(c.body[:at] + ["WITH *"] + c.body[at:], c.nodes, c.rels, c.values,
                 c.items, c.distinct)
    return Case("with_star", [Variant("base", render(c.query())),
                              Variant("with *", render(star.query()))])


def rename(ch: Chooser) -> Case:
    """Renaming every variable, by a permutation of the same names or by
    new ones, changes nothing."""
    base = chain(ch).query()
    vs = variables(base)
    pool = vs if ch.random() < 0.5 else PLAIN_NAMES
    names = dict(zip(vs, ch.sample(pool, len(vs))))
    return Case("rename", [Variant("base", render(base)),
                           Variant("renamed", render(base, names))])


def keyword_name(ch: Chooser) -> Case:
    """A keyword that openCypher does not reserve works as a variable name."""
    base = chain(ch).query()
    var = ch.choice(variables(base))
    kw = ch.choice(SOFT_KEYWORDS)
    return Case(f"keyword_name/{kw.lower()}",
                [Variant("base", render(base)),
                 Variant(f"{var} -> {kw}", render(base, {var: kw}))])


def predicate_move(ch: Chooser) -> Case:
    """A predicate on the first pattern's variables gives the same rows
    attached to the first MATCH, the second MATCH, or a WITH."""
    p1 = pattern(ch, "c1", ["a", "b", "c"], ["r", "q"])
    p2 = pattern(ch, "c2", ["d", "e"], ["p", "t"], start=ch.choice(p1.node_vars))
    pr = predicate(ch, p1.node_vars, p1.rel_vars)
    items = _items(ch, p1.node_vars + p2.node_vars, p1.rel_vars + p2.rel_vars)
    m1, m2 = f"MATCH {p1.text()}", f"MATCH {p2.text()}"
    return Case("predicate_move", [
        Variant("first", render(f"{m1} WHERE {pr} {m2} RETURN {items}")),
        Variant("second", render(f"{m1} {m2} WHERE {pr} RETURN {items}")),
        Variant("with", render(f"{m1} {m2} WITH * WHERE {pr} RETURN {items}"))])


def reverse_pattern(ch: Chooser) -> Case:
    """A pattern written right to left matches the same paths."""
    form = ch.pick("rp.clause")
    if form == "match":
        p = pattern(ch, "c1", ["a", "b", "c"], ["r", "q"])
        items = _items(ch, p.node_vars, p.rel_vars)
        q = lambda rev: f"MATCH {p.text(rev)} RETURN {items}"
    else:
        p1 = pattern(ch, "c1", ["a"], [], hops=0)
        p = pattern(ch, "c2", ["d", "e"], ["p", "t"], start="a")
        items = _items(ch, ["a"] + p.node_vars, p.rel_vars)
        q = lambda rev: f"MATCH {p1.text()} OPTIONAL MATCH {p.text(rev)} RETURN {items}"
    return Case(f"reverse_pattern/{form}", [Variant("forward", render(q(False))),
                                             Variant("reversed", render(q(True)))])


def split_path(ch: Chooser) -> Case:
    """A two-hop pattern equals two MATCH clauses with distinct edges."""
    p = _named(pattern(ch, "c1", ["a", "b", "c"], ["r", "q"], hops=2))
    (a, la), (b, lb), (c, lc) = p.nodes
    s1, s2 = p.steps
    first = Pattern([(a, la), (b, lb)], [s1])
    second = Pattern([(b, ""), (c, lc)], [s2], bound_start=True)
    items = _items(ch, p.node_vars, p.rel_vars)
    return Case("split_path", [
        Variant("one", render(f"MATCH {p.text()} RETURN {items}")),
        Variant("two", render(f"MATCH {first.text()} MATCH {second.text()} "
                              f"WHERE id({v(s1.rel)}) <> id({v(s2.rel)}) RETURN {items}"))])


def varlen(ch: Chooser) -> Case:
    """A variable-length relationship equals the union of fixed-length
    paths with distinct edges."""
    rng_ = ch.pick("vl.range")
    p = pattern(ch, "c1", ["a", "b"], ["r"], hops=1)
    (a, la), (b, lb) = p.nodes
    s = p.steps[0]
    items = _items(ch, [a, b], [])
    lo, hi = {"*1..1": (1, 1), "*1..2": (1, 2), "*2..2": (2, 2), "*1..3": (1, 3)}[rng_]
    vl = f"MATCH ({v(a)}{la}){_arrow(s.dir, s.type + rng_)}({v(b)}{lb}) RETURN {items}"
    parts = []
    for n in range(lo, hi + 1):
        rs = [f"e{i}" for i in range(n)]
        path = f"({v(a)}{la})"
        for i, rv in enumerate(rs):
            path += _arrow(s.dir, f"{v(rv)}{s.type}") + (f"({v(b)}{lb})" if i == n - 1 else "()")
        distinct = " AND ".join(f"id({v(x)}) <> id({v(y)})"
                                for i, x in enumerate(rs) for y in rs[i + 1:])
        parts.append(f"MATCH {path}" + (f" WHERE {distinct}" if distinct else "") + f" RETURN {items}")
    return Case(f"varlen/{rng_}", [
        Variant("varlen", render(vl)),
        Variant("fixed", parts=[render(x) for x in parts], combine=concat)])


# ---------------------------------------------------------------- partition

def tlp_where(ch: Chooser) -> Case:
    """Ternary logic partitioning: the rows of Q are the rows of Q WHERE p,
    of Q WHERE NOT p and of Q WHERE p IS NULL."""
    c = chain(ch, ret_kinds=["scalar", "distinct"])
    pr = predicate(ch, c.nodes, c.rels)
    parts = [render(c.query([f"WITH * WHERE {w}"])) for w in THREE(pr)]
    return Case("tlp_where", [Variant("whole", render(c.query())),
                              Variant("partitions", parts=parts, combine=concat)],
                compare="set" if c.distinct else "bag")


def tlp_count(ch: Chooser) -> Case:
    """count(*) and sum() of Q equal the sums over the three partitions."""
    c = chain(ch, ret_kinds=["scalar"])
    pr = predicate(ch, c.nodes, c.rels)
    items = ["count(*) AS c0", f"sum({int_expr(ch, c.nodes, c.rels)}) AS c1"]
    parts = [render(c.query([f"WITH * WHERE {w}"], items=items, distinct=False)) for w in THREE(pr)]
    return Case("tlp_count", [
        Variant("whole", render(c.query(items=items, distinct=False))),
        Variant("partitions", parts=parts, combine=sum_columns)])


def limit_slice(ch: Chooser) -> Case:
    """ORDER BY all columns: SKIP/LIMIT is a slice of the full order, and
    DESC is its reverse."""
    c = chain(ch, ret_kinds=["scalar", "distinct"])
    cols = [f"c{i}" for i in range(len(c.items))]
    asc = " ORDER BY " + ", ".join(cols)
    full = render(c.query(tail=asc))
    if ch.random() < 0.5:
        s, l = ch.randint(0, 2), ch.randint(1, 3)
        return Case("limit_slice/skip_limit", [
            Variant("slice of full", full, post=lambda rows: rows[s:s + l]),
            Variant("skip/limit", render(c.query(tail=f"{asc} SKIP {s} LIMIT {l}")))],
            compare="list")
    desc = " ORDER BY " + ", ".join(f"{x} DESC" for x in cols)
    return Case("limit_slice/desc", [
        Variant("reversed full", full, post=lambda rows: list(reversed(rows))),
        Variant("desc", render(c.query(tail=desc)))], compare="list")


def distinct_set(ch: Chooser) -> Case:
    """RETURN DISTINCT gives each row of RETURN once."""
    c = chain(ch, ret_kinds=["scalar"])
    return Case("distinct_set", [
        Variant("deduped", render(c.query(distinct=False)), post=dedupe),
        Variant("distinct", render(c.query(distinct=True)), post=equiv)])


def union(ch: Chooser) -> Case:
    """Q1 UNION ALL Q2 is the rows of both; UNION is those rows once."""
    n = ch.randint(1, 3)
    q1, q2 = render(chain(ch, n_items=n).query()), render(chain(ch, n_items=n).query())
    if ch.random() < 0.5:
        return Case("union/all", [Variant("concat", parts=[q1, q2], combine=concat),
                                  Variant("union all", f"{q1} UNION ALL {q2}")])
    return Case("union/distinct", [Variant("concat", parts=[q1, q2], combine=concat, post=dedupe),
                                   Variant("union", f"{q1} UNION {q2}", post=equiv)])


def distinct_agg(ch: Chooser) -> Case:
    """collect/count(DISTINCT e) equals the aggregate over WITH DISTINCT."""
    p = pattern(ch, "c1", ["a", "b", "c"], ["r", "q"])
    e = expr(ch, p.node_vars, p.rel_vars)
    fn = ch.choice(["collect", "count"])
    if ch.random() < 0.6:
        g = expr(ch, p.node_vars, p.rel_vars, kind=ch.choice(EXPR_KINDS))
        base = f"MATCH {p.text()} RETURN {g} AS c0, {fn}(DISTINCT {e}) AS c1"
        alt = f"MATCH {p.text()} WITH DISTINCT {g} AS c0, {e} AS x RETURN c0, {fn}(x) AS c1"
    else:
        base = f"MATCH {p.text()} RETURN {fn}(DISTINCT {e}) AS c1"
        alt = f"MATCH {p.text()} WITH DISTINCT {e} AS x RETURN {fn}(x) AS c1"
    vs = [Variant("base", render(base)), Variant("with distinct", render(alt))]
    if fn == "count":
        vs.append(Variant("size(collect)", render(base.replace(
            f"count(DISTINCT {e})", f"size(collect(DISTINCT {e}))"))))
    return Case("distinct_agg", vs)


def alias_shadow(ch: Chooser) -> Case:
    """A RETURN alias that reuses a variable name changes only the key."""
    p = pattern(ch, "c1", ["a", "b", "c"], ["r", "q"])
    g = expr(ch, p.node_vars, p.rel_vars)
    agg = aggregate(ch, p.node_vars, p.rel_vars)
    base = f"MATCH {p.text()} RETURN {g} AS c0, {agg} AS c1"
    var = ch.choice(p.node_vars + p.rel_vars)
    which = ch.choice(["c0", "c1"])
    shadow = base.replace(f"AS {which}", f"AS {v(var)}")
    return Case("alias_shadow", [
        Variant("base", render(base)),
        Variant(f"{which} AS {var}", render(shadow), keymap={var: which})])


# ------------------------------------------------------------------ patterns

def _step(ch: Chooser) -> tuple[str, str]:
    t = ch.pick("c1.type")
    return ({"any": "", "one": ":" + ch.choice(["R", "S"]), "alt": ":R|S"}[t],
            ch.pick("c1.dir"))


def label_disjunction(ch: Chooser) -> Case:
    """`:A|B` equals `WHERE x:A OR x:B`, in a MATCH, a pattern predicate and
    a pattern comprehension."""
    form = ch.pick("ld.form")
    if form == "match":
        p = pattern(ch, "c1", ["a", "b", "c"], ["r", "q"])
        i = ch.randint(0, len(p.nodes) - 1)
        var = p.nodes[i][0]
        disj = Pattern([(n, ":A|B" if j == i else l) for j, (n, l) in enumerate(p.nodes)], p.steps)
        plain = Pattern([(n, "" if j == i else l) for j, (n, l) in enumerate(p.nodes)], p.steps)
        items = _items(ch, p.node_vars, p.rel_vars)
        base = f"MATCH {disj.text()} RETURN {items}"
        alt = f"MATCH {plain.text()} WHERE {v(var)}:A OR {v(var)}:B RETURN {items}"
    else:
        lab = label_text(ch.pick("c1.label"), ch)
        t, d = _step(ch)
        step = _arrow(d, t)
        if form == "predicate":
            base = f"MATCH (⟨a⟩{lab}) WHERE (⟨a⟩){step}(:A|B) RETURN ⟨a⟩.id AS c0"
            alt = (f"MATCH (⟨a⟩{lab}) WHERE EXISTS {{ MATCH (⟨a⟩){step}(⟨m⟩) "
                   f"WHERE ⟨m⟩:A OR ⟨m⟩:B }} RETURN ⟨a⟩.id AS c0")
        else:
            base = f"MATCH (⟨a⟩{lab}) RETURN ⟨a⟩.id AS c0, [(⟨a⟩){step}(⟨m⟩:A|B) | ⟨m⟩.id] AS c1"
            alt = (f"MATCH (⟨a⟩{lab}) RETURN ⟨a⟩.id AS c0, "
                   f"[(⟨a⟩){step}(⟨m⟩) WHERE ⟨m⟩:A OR ⟨m⟩:B | ⟨m⟩.id] AS c1")
    return Case(f"label_disjunction/{form}",
                [Variant("A|B", render(base)), Variant("OR", render(alt))])


def inline_props(ch: Chooser) -> Case:
    """`(a {v: k})` equals `(a) WHERE a.v = k`."""
    p = pattern(ch, "c1", ["a", "b", "c"], ["r", "q"])
    i = ch.randint(0, len(p.nodes) - 1)
    var = p.nodes[i][0]
    key, val = ch.choice([("v", str(ch.randint(0, 3))), ("s", repr(ch.choice(["x", "y"]))),
                          ("b", ch.choice(["true", "false"]))])
    inline = Pattern([(n, l + (f" {{{key}: {val}}}" if j == i else ""))
                      for j, (n, l) in enumerate(p.nodes)], p.steps)
    items = _items(ch, p.node_vars, p.rel_vars)
    return Case("inline_props", [
        Variant("inline", render(f"MATCH {inline.text()} RETURN {items}")),
        Variant("where", render(f"MATCH {p.text()} WHERE {v(var)}.{key} = {val} RETURN {items}"))])


def optional_filter(ch: Chooser) -> Case:
    """OPTIONAL MATCH filtered to matched rows equals MATCH."""
    p1 = pattern(ch, "c1", ["a", "b", "c"], ["r", "q"])
    p2 = pattern(ch, "c2", ["d", "e"], ["p", "t"], start=ch.choice(p1.node_vars))
    items = _items(ch, p1.node_vars + p2.node_vars, p1.rel_vars + p2.rel_vars)
    new = p2.node_vars[-1]
    return Case("optional_filter", [
        Variant("match", render(f"MATCH {p1.text()} MATCH {p2.text()} RETURN {items}")),
        Variant("optional+filter", render(f"MATCH {p1.text()} OPTIONAL MATCH {p2.text()} "
                                          f"WITH * WHERE {v(new)} IS NOT NULL RETURN {items}"))])


def optional_null(ch: Chooser) -> Case:
    """OPTIONAL MATCH gives a null row exactly for the nodes with no match,
    and as many rows as matches otherwise."""
    form = ch.pick("om.form")
    p1 = pattern(ch, "c1", ["a"], [], hops=0)
    p2 = pattern(ch, "c2", ["d", "e"], ["p", "t"], start="a")
    x = v(p2.node_vars[-1])
    if form == "null_rows":
        return Case("optional_null/null_rows", [
            Variant("optional", render(f"MATCH {p1.text()} OPTIONAL MATCH {p2.text()} "
                                       f"WITH ⟨a⟩, {x} WHERE {x} IS NULL RETURN ⟨a⟩.id AS c0")),
            Variant("not exists", render(f"MATCH {p1.text()} WHERE NOT EXISTS {{ MATCH {p2.text()} }} "
                                         f"RETURN ⟨a⟩.id AS c0"))])
    return Case("optional_null/count", [
        Variant("optional", render(f"MATCH {p1.text()} OPTIONAL MATCH {p2.text()} "
                                   f"RETURN ⟨a⟩.id AS c0, count({x}) AS c1")),
        Variant("comprehension", render(f"MATCH {p1.text()} RETURN ⟨a⟩.id AS c0, "
                                        f"size([{p2.text()} | 1]) AS c1"))])


# -------------------------------------------------------------------- writes

def write_expr(ch: Chooser, nodes: list[str], scope_i: bool) -> str:
    """A storable property value of the kind chosen for w.kind."""
    kind = ch.pick("w.kind")
    if not nodes:
        return {"prop": "⟨i⟩", "arith": "⟨i⟩ * 2 + 1",
                "case": "CASE WHEN ⟨i⟩ = 1 THEN 'a' ELSE 'b' END",
                "func": "toString(⟨i⟩)", "list": "[⟨i⟩, 1]", "map_value": "{x: ⟨i⟩}.x",
                "nullcheck": "⟨i⟩ IS NULL", "string": "'s' + toString(⟨i⟩)"}[kind]
    n0 = v(nodes[0])
    if kind == "map_value":
        e = ch.choice([f"{{x: {n0}.v}}.x", f"properties({n0}).s"])
    else:
        e = expr(ch, nodes, [], kind=kind)
        if kind == "list" and e.startswith("size("):
            e = f"[{n0}.id, 1]"
    if scope_i and ch.random() < 0.4:
        e = (f"[{n0}.id, ⟨i⟩]" if kind == "list" else
             ch.choice([f"CASE WHEN ⟨i⟩ = 1 THEN {e} ELSE null END", f"{n0}.id * 10 + ⟨i⟩"]))
    return e


def _write_prefix(ch: Chooser, exclude_new: bool = False):
    """The read part before a write. With exclude_new the MATCH skips the
    :New nodes a previous run of the same query created."""
    shape = ch.pick("w.shape")
    if shape == "create":
        return shape, "", [], []
    if shape == "unwind":
        return shape, "UNWIND [1, 2] AS ⟨i⟩", [], ["i"]
    p = pattern(ch, "c1", ["a", "b"], ["r"], hops=ch.pick("c1.hops", [0, 1]))
    nodes, rels = p.node_vars, p.rel_vars
    m = f"MATCH {p.text()}"
    if exclude_new:
        m += " WHERE " + " AND ".join(f"NOT {v(x)}:New" for x in nodes)
    if shape == "match":
        return shape, m, nodes, nodes + rels
    if shape == "match_unwind":
        return shape, f"{m} UNWIND [1, 2] AS ⟨i⟩", nodes, nodes + rels + ["i"]
    keep = ", ".join(v(x) for x in nodes + rels)
    return shape, f"{m} WITH {keep}", nodes, nodes + rels


def write_expression(ch: Chooser) -> Case:
    """A property expression written inline equals the same value computed
    in a WITH first, and equals what a RETURN of it gives."""
    shape, pre, nodes, scope = _write_prefix(ch)
    if shape == "create":
        e = write_expr(ch, [], False).replace("⟨i⟩", "2")
        return Case(f"write_expression/{ch.used['w.kind']}/create", [
            Variant("inline", render(f"CREATE (:New {{k: {e}}})"), mode="write"),
            Variant("return", render(f"RETURN {e} AS k"))],
            write=True, readback="MATCH (n:New) RETURN n.k AS k")
    e = write_expr(ch, nodes, "i" in scope)
    keep = ", ".join(v(x) for x in scope)
    return Case(f"write_expression/{ch.used['w.kind']}/{shape}", [
        Variant("inline", render(f"{pre} CREATE (:New {{k: {e}}})"), mode="write"),
        Variant("with", render(f"{pre} WITH {keep}, {e} AS ⟨val⟩ CREATE (:New {{k: ⟨val⟩}})"),
                mode="write"),
        Variant("return", render(f"{pre} RETURN {e} AS k"))],
        write=True, readback="MATCH (n:New) RETURN n.k AS k")


def merge_idempotent(ch: Chooser) -> Case:
    """MERGE twice leaves what MERGE once does: one node per distinct value."""
    shape, pre, nodes, scope = _write_prefix(ch, exclude_new=True)
    if shape == "create":
        pre, nodes, scope = "UNWIND [1, 2, 2] AS ⟨i⟩", [], ["i"]
    e = write_expr(ch, nodes, "i" in scope)
    q = render(f"{pre} WITH * WHERE ({e}) IS NOT NULL MERGE (:New {{k: {e}}})")
    return Case(f"merge_idempotent/{ch.used['w.kind']}", [
        Variant("once", q, mode="write"),
        Variant("twice", q, mode="write", repeat=2),
        Variant("distinct values",
                render(f"{pre} WITH {e} AS k WHERE k IS NOT NULL RETURN DISTINCT k"))],
        write=True, readback="MATCH (n:New) RETURN n.k AS k")


def set_readback(ch: Chooser) -> Case:
    """SET a.k = e stores the value of e for each matched node."""
    lab = label_text(ch.pick("c1.label"), ch)
    e = write_expr(ch, ["a"], False)
    cond = label_where(lab, "a")
    setq = (f"MATCH (⟨a⟩{lab}) SET ⟨a⟩.k = {e}" if ch.random() < 0.5 else
            f"MATCH (⟨a⟩{lab}) SET ⟨a⟩ += {{k: {e}}}")
    return Case(f"set_readback/{ch.used['w.kind']}", [
        Variant("set", render(setq), mode="write"),
        Variant("computed", render(f"MATCH (⟨a⟩) RETURN ⟨a⟩.id AS id, "
                                   f"CASE WHEN {cond} THEN {e} ELSE null END AS k"))],
        write=True, readback="MATCH (n) RETURN n.id AS id, n.k AS k")


def delete_count(ch: Chooser) -> Case:
    """DETACH DELETE removes exactly the matched nodes (or relationships)."""
    form = ch.pick("del.form")
    if form == "node":
        lab = label_text(ch.pick("c1.label"), ch)
        return Case("delete_count/node", [
            Variant("delete", render(f"MATCH (⟨a⟩{lab}) DETACH DELETE ⟨a⟩"), mode="write"),
            Variant("rest", render(f"MATCH (⟨a⟩) WHERE NOT {label_where(lab, 'a')} "
                                   f"RETURN ⟨a⟩.id AS id"))],
            write=True, readback="MATCH (n) RETURN n.id AS id")
    t, d = _step(ch)
    types = t[1:].split("|") if t else []
    keep = f"NOT type(⟨r⟩) IN {json.dumps(types)}" if types else "false"
    return Case("delete_count/rel", [
        Variant("delete", render(f"MATCH ()" + _arrow(d, f"⟨r⟩{t}") + "() DELETE ⟨r⟩"), mode="write"),
        Variant("rest", render(f"MATCH ()-[⟨r⟩]->() WHERE {keep} RETURN ⟨r⟩.w AS w"))],
        write=True, readback="MATCH ()-[r]->() RETURN r.w AS w")


RELATIONS: dict[str, tuple[Callable, list[str]]] = {
    "with_star": (with_star, CHAIN),
    "rename": (rename, CHAIN),
    "keyword_name": (keyword_name, CHAIN),
    "predicate_move": (predicate_move, G + C1 + C2_FORCED + ["pred.kind", "expr.kind"]),
    "reverse_pattern": (reverse_pattern, G + C1 + C2_FORCED + ["rp.clause", "expr.kind"]),
    "split_path": (split_path, G + ["c1.dir", "c1.type", "c1.label", "expr.kind"]),
    "varlen": (varlen, G + ["c1.dir", "c1.type", "c1.label", "vl.range", "expr.kind"]),
    "tlp_where": (tlp_where, [d for d in CHAIN if d != "ret.kind"] + ["pred.kind"]),
    "tlp_count": (tlp_count, [d for d in CHAIN if d != "ret.kind"] + ["pred.kind"]),
    "limit_slice": (limit_slice, [d for d in CHAIN if d not in ("ret.kind", "agg.kind")]),
    "distinct_set": (distinct_set, [d for d in CHAIN if d != "ret.kind"]),
    "union": (union, CHAIN),
    "distinct_agg": (distinct_agg, G + C1 + ["expr.kind"]),
    "alias_shadow": (alias_shadow, G + C1 + ["expr.kind", "agg.kind"]),
    "label_disjunction": (label_disjunction, G + C1 + ["ld.form", "expr.kind"]),
    "inline_props": (inline_props, G + C1 + ["expr.kind"]),
    "optional_filter": (optional_filter, G + C1 + C2_FORCED + ["expr.kind"]),
    "optional_null": (optional_null, G + ["c1.label"] + C2_FORCED + ["om.form"]),
    "write_expression": (write_expression, G + C1 + ["w.shape", "w.kind"]),
    "merge_idempotent": (merge_idempotent, G + C1 + ["w.shape", "w.kind"]),
    "set_readback": (set_readback, G + ["c1.label", "w.kind"]),
    "delete_count": (delete_count, G + ["c1.label", "c1.type", "c1.dir", "del.form"]),
}
