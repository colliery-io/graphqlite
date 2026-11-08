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

from .gen import (AGGS, LABELS, Pattern, Step, aggregate, bare_names, expr, int_expr, label_text, label_where, lit,
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
    "w.shape": ["create", "match", "match_unwind", "unwind", "match_with", "match_match"],
    "w.kind": ["prop", "arith", "case", "func", "list", "map_value", "nullcheck", "string"],
    "vl.range": ["*1..1", "*1..2", "*2..2", "*1..3"],
    "rp.clause": ["match", "optional"],
    "np.carry": ["none", "star", "with_p"],
    "del.form": ["node", "rel"],
    "om.form": ["null_rows", "count", "outer_where"],
    "ec.ctx": ["with", "unwind", "unwind_var", "listidx", "map", "case", "comp", "head",
               "reduce", "call", "order", "limit", "union"],
    "ec.kind": ["prop", "arith", "case", "func", "list", "map", "nullcheck", "entity", "string",
                "math", "strfn", "listfn",
                "bool", "nested", "strconv", "float", "literal"],
    "wo.write": ["set_m", "set_a", "create", "create_rel", "set_label"],
    "sf.form": ["replace", "merge_map", "items", "inline_key"],
    "wr.base": ["write_expression", "set_readback", "set_forms", "write_optional",
                "merge_direct", "delete_count"],
    # GQLITE-T-0526: relations over writes
    "wb.form": ["create", "set"],
    "wu.form": ["create", "set", "label", "rel"],
    "wrb.form": ["unwind_create", "set_overflow", "merge_overflow", "rollback"],
    # GQLITE-T-0506: MERGE in pipelines
    "mr.end": ["bound", "unbound"],
    "mo.form": ["node", "rel"],
    # GQLITE-T-0507: CALL { } subqueries
    "cs.form": ["uncorrelated", "union_all", "union", "union_count", "correlated_union",
                "after_agg", "write", "write_union", "alone", "after_write"],
    # GQLITE-T-0508: write forms
    "wf.form": ["set_param_merge", "set_param_replace", "set_map_value", "delete_return",
                "remove_label", "foreach_set", "foreach_merge", "foreach_list"],
    # GQLITE-T-0509: temporal values
    "tm.kind": ["date", "datetime", "localdatetime", "localtime", "time", "duration"],
    "tm.form": ["context", "stored", "roundtrip", "order", "compare", "component", "truncate"],
    # GQLITE-T-0511: with and without an index
    "ix.base": ["predicate_move", "tlp_where", "inline_props", "label_disjunction", "optional_filter",
                "with_star", "merge_direct", "set_readback", "write_expression", "delete_count"],
    "ix.ddl": ["index", "composite", "unique", "mixed"],
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
    expect_error: bool = False              # the write must fail and leave nothing
    params: dict | None = None              # $parameters of every query (GQLITE-T-0508)

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

def named_path(ch: Chooser) -> Case:
    """A named path gives the nodes and relationships of its pattern, also
    carried through `WITH *` or `WITH p` (GQLITE-T-0471)."""
    p = _named(pattern(ch, "c1", ["a", "b", "c"], ["r", "q"], hops=ch.pick("c1.hops", [1, 2])))
    carry = ch.pick("np.carry")
    keep = ", ".join(v(x) for x in ["pp"] + p.node_vars)
    mid = {"none": "", "star": " WITH *", "with_p": f" WITH {keep}"}[carry]
    hops = len(p.steps)
    node_vs = ", ".join(f"{v(x)}.v" for x in p.node_vars)
    rel_ws = ", ".join(f"{v(x)}.w" for x in p.rel_vars)
    return Case(f"named_path/{carry}", [
        Variant("path", render(f"MATCH ⟨pp⟩ = {p.text()}{mid} RETURN length(⟨pp⟩) AS c0, "
                               f"[⟨x⟩ IN nodes(⟨pp⟩) | ⟨x⟩.v] AS c1, "
                               f"[⟨x⟩ IN relationships(⟨pp⟩) | ⟨x⟩.w] AS c2, {v(p.node_vars[0])}.id AS c3")),
        Variant("variables", render(f"MATCH {p.text()} RETURN {hops} AS c0, [{node_vs}] AS c1, "
                                    f"[{rel_ws}] AS c2, {v(p.node_vars[0])}.id AS c3"))])


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
    # A name the template uses unmarked (a comprehension or reduce variable)
    # is taken: renaming h to x in `[x IN l WHERE x > h.v]` reads the
    # comprehension's x (GQLITE-T-0492). A variable with such a name keeps it.
    taken = bare_names(base)
    keep = [s for s in vs if s in taken]
    free = [s for s in vs if s not in taken]
    pool = free if ch.random() < 0.5 else [n for n in PLAIN_NAMES if n not in taken]
    names = {s: s for s in keep}
    names.update(zip(free, ch.sample(pool, len(free))))
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


def varlen_comprehension(ch: Chooser) -> Case:
    """A pattern comprehension with a variable-length relationship has one
    element per walk of the OPTIONAL MATCH (GQLITE-T-0472)."""
    rng_ = ch.pick("vl.range")
    p = pattern(ch, "c1", ["a", "b"], ["r"], hops=1)
    (a, la), (b, lb) = p.nodes
    s = p.steps[0]
    walk = f"({v(a)}){_arrow(s.dir, s.type + rng_)}({v(b)}{lb})"
    return Case(f"varlen_comprehension/{rng_}", [
        Variant("comprehension", render(f"MATCH ({v(a)}{la}) RETURN {v(a)}.id AS c0, "
                                        f"size([{walk} | 1]) AS c1")),
        Variant("optional", render(f"MATCH ({v(a)}{la}) OPTIONAL MATCH {walk} "
                                   f"RETURN {v(a)}.id AS c0, count({v(b)}) AS c1"))])


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
    if form == "outer_where":
        # A WHERE on the outer variable only: a failing row is one null row,
        # whatever the matches, anchored at ⟨a⟩ or not (GQLITE-T-0470).
        anchored = ch.random() < 0.5
        p2 = (pattern(ch, "c2", ["d", "e"], ["p", "t"], start="a") if anchored else
              pattern(ch, "c2", ["d", "e", "f"], ["p", "t"], hops=ch.choice([1, 2])))
        x = v(p2.node_vars[-1])
        pred = f"coalesce(⟨a⟩.v > 1, false)"
        return Case(f"optional_null/outer_where/{'anchored' if anchored else 'free'}", [
            Variant("where", render(f"MATCH {p1.text()} OPTIONAL MATCH {p2.text()} WHERE {pred} "
                                    f"RETURN ⟨a⟩.id AS c0, count({x}) AS c1, count(*) AS c2")),
            Variant("after", render(f"MATCH {p1.text()} OPTIONAL MATCH {p2.text()} "
                                    f"WITH ⟨a⟩, count({x}) AS ⟨n⟩ "
                                    f"RETURN ⟨a⟩.id AS c0, CASE WHEN {pred} THEN ⟨n⟩ ELSE 0 END AS c1, "
                                    f"CASE WHEN {pred} AND ⟨n⟩ > 0 THEN ⟨n⟩ ELSE 1 END AS c2"))])
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
    if exclude_new:
        # A second run must not match the :M relationships the first made
        # (GQLITE-T-0506): name each relationship to exclude them.
        p = _named(p)
    nodes, rels = p.node_vars, p.rel_vars
    m = f"MATCH {p.text()}"
    if exclude_new:
        m += " WHERE " + " AND ".join([f"NOT {v(x)}:New" for x in nodes] +
                                      [f"type({v(x)}) <> 'M'" for x in rels])
    if shape == "match":
        return shape, m, nodes, nodes + rels
    if shape == "match_unwind":
        return shape, f"{m} UNWIND [1, 2] AS ⟨i⟩", nodes, nodes + rels + ["i"]
    if shape == "match_match":
        # A second MATCH whose WHERE reads the first one (GQLITE-T-0469).
        m2 = f"{m} MATCH (⟨c⟩) WHERE ⟨c⟩.v = {v(nodes[0])}.v"
        if exclude_new:
            m2 += " AND NOT ⟨c⟩:New"
        return shape, m2, nodes + ["c"], nodes + rels + ["c"]
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
    # After two MATCH clauses the new node hangs off a matched one, so the
    # CREATE reads the bound variables (GQLITE-T-0469).
    link = "(⟨c⟩)-[:L]->" if shape == "match_match" else ""
    return Case(f"write_expression/{ch.used['w.kind']}/{shape}", [
        Variant("inline", render(f"{pre} CREATE {link}(:New {{k: {e}}})"), mode="write"),
        Variant("with", render(f"{pre} WITH {keep}, {e} AS ⟨val⟩ CREATE {link}(:New {{k: ⟨val⟩}})"),
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


def merge_direct(ch: Chooser) -> Case:
    """MERGE right after the read part (no WITH), once or twice, leaves one
    node per distinct value; coalesce keeps the value non-null
    (GQLITE-T-0461: a computed value was written as nothing)."""
    shape, pre, nodes, scope = _write_prefix(ch, exclude_new=True)
    if shape == "create":
        pre, nodes, scope = "UNWIND [1, 2, 2] AS ⟨i⟩", [], ["i"]
    e = f"coalesce({write_expr(ch, nodes, 'i' in scope)}, 'none')"
    q = render(f"{pre} MERGE (:New {{k: {e}}})")
    return Case(f"merge_direct/{ch.used['w.kind']}", [
        Variant("once", q, mode="write"),
        Variant("twice", q, mode="write", repeat=2),
        Variant("distinct values", render(f"{pre} RETURN DISTINCT {e} AS k"))],
        write=True, readback="MATCH (n:New) RETURN n.k AS k")


def _merge_prefix(ch: Chooser):
    """The read part before a MERGE; skips the :New nodes of an earlier run."""
    shape, pre, nodes, scope = _write_prefix(ch, exclude_new=True)
    if shape == "create":
        pre, nodes, scope = "UNWIND [1, 2, 2] AS ⟨i⟩", [], ["i"]
    e = f"coalesce({write_expr(ch, nodes, 'i' in scope)}, 'none')"
    return shape, pre, nodes, e


def merge_rel(ch: Chooser) -> Case:
    """MERGE of a relationship (both ends bound, or a new end) once or twice
    leaves one relationship per distinct key, the same graph as MATCH-else-
    CREATE (GQLITE-T-0506)."""
    shape, pre, nodes, e = _merge_prefix(ch)
    end = ch.pick("mr.end")
    if len(nodes) >= 2 and end == "bound":
        a, b = v(nodes[0]), v(nodes[1])
        merge = f"MERGE ({a})-[:M {{k: {e}}}]->({b})"
        dist = f"RETURN DISTINCT {a}.id AS a, {b}.id AS b, {e} AS k"
        alt = (f"WITH DISTINCT {a} AS ⟨x⟩, {b} AS ⟨y⟩, {e} AS ⟨k⟩ "
               f"WHERE NOT (⟨x⟩)-[:M {{k: ⟨k⟩}}]->(⟨y⟩) CREATE (⟨x⟩)-[:M {{k: ⟨k⟩}}]->(⟨y⟩)")
        readback = "MATCH (a)-[r:M]->(b) RETURN a.id AS a, b.id AS b, r.k AS k"
    elif nodes:
        a = v(nodes[0])
        merge = f"MERGE ({a})-[:M]->(:New {{k: {e}}})"
        dist = f"RETURN DISTINCT {a}.id AS a, {e} AS k"
        alt = (f"WITH DISTINCT {a} AS ⟨x⟩, {e} AS ⟨k⟩ "
               f"WHERE NOT (⟨x⟩)-[:M]->(:New {{k: ⟨k⟩}}) CREATE (⟨x⟩)-[:M]->(:New {{k: ⟨k⟩}})")
        readback = "MATCH (a)-[:M]->(n:New) RETURN a.id AS a, n.k AS k"
    else:
        merge = f"MERGE (:New {{k: {e}}})-[:M]->(:End)"
        dist = f"RETURN DISTINCT {e} AS k"
        alt = f"WITH DISTINCT {e} AS ⟨k⟩ CREATE (:New {{k: ⟨k⟩}})-[:M]->(:End)"
        readback = "MATCH (n:New)-[:M]->(:End) RETURN n.k AS k"
    return Case(f"merge_rel/{end if nodes else 'none'}/{shape}", [
        Variant("once", render(f"{pre} {merge}"), mode="write"),
        Variant("twice", render(f"{pre} {merge}"), mode="write", repeat=2),
        Variant("match else create", render(f"{pre} {alt}"), mode="write"),
        Variant("distinct", render(f"{pre} {dist}"))],
        write=True, readback=readback)


def merge_on(ch: Chooser) -> Case:
    """MERGE ... ON CREATE SET / ON MATCH SET, run twice: each distinct key
    is created once and matched by every other row of both runs
    (GQLITE-T-0506)."""
    shape, pre, nodes, e = _merge_prefix(ch)
    form = ch.pick("mo.form")
    if form == "rel" and len(nodes) >= 2:
        a, b = v(nodes[0]), v(nodes[1])
        merge = (f"MERGE ({a})-[⟨m⟩:M {{k: {e}}}]->({b}) ON CREATE SET ⟨m⟩.c = 1 "
                 f"ON MATCH SET ⟨m⟩.n = coalesce(⟨m⟩.n, 0) + 1")
        computed = (f"WITH {a}.id AS ⟨x⟩, {b}.id AS ⟨y⟩, {e} AS ⟨k⟩ WITH ⟨x⟩, ⟨y⟩, ⟨k⟩, count(*) AS ⟨cnt⟩ "
                    f"RETURN ⟨x⟩ AS a, ⟨y⟩ AS b, ⟨k⟩ AS k, 1 AS c, 2 * ⟨cnt⟩ - 1 AS n")
        readback = "MATCH (a)-[r:M]->(b) RETURN a.id AS a, b.id AS b, r.k AS k, r.c AS c, r.n AS n"
        form = "rel"
    else:
        merge = (f"MERGE (⟨m⟩:New {{k: {e}}}) ON CREATE SET ⟨m⟩.c = 1 "
                 f"ON MATCH SET ⟨m⟩.n = coalesce(⟨m⟩.n, 0) + 1")
        computed = (f"WITH {e} AS ⟨k⟩ WITH ⟨k⟩, count(*) AS ⟨cnt⟩ "
                    f"RETURN ⟨k⟩ AS k, 1 AS c, 2 * ⟨cnt⟩ - 1 AS n")
        readback = "MATCH (n:New) RETURN n.k AS k, n.c AS c, n.n AS n"
        form = "node"
    return Case(f"merge_on/{form}/{shape}", [
        Variant("twice", render(f"{pre} {merge}"), mode="write", repeat=2),
        Variant("computed", render(f"{pre} {computed}"))],
        write=True, readback=readback)


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


# -------------------------------------------------------- expression context

# Values that must keep their type wherever they go (GQLITE-T-0481/0482).
LITERALS = ["'1'", "'1.5'", "'true'", "'[1]'", "'{\"a\": 1}'", "''", "true", "false", "null",
            "[1, 'a']", "['1', '2']", "[true, null]", "[[1], []]", "{a: 1}", "{a: '1', b: [true]}",
            "1.5", "-7", "toString(12)", "'1' + '2'"]


def context_expr(ch: Chooser, nodes: list[str], rels: list[str]) -> str:
    """An expression of the kind chosen for ec.kind: gen.expr's kinds plus
    booleans, nested lists and maps, number-like strings, floats and
    literals."""
    kind = ch.pick("ec.kind")
    if kind in EXPR_KINDS:
        return expr(ch, nodes, rels, kind=kind)
    n, m = v(ch.choice(nodes)), v(ch.choice(nodes))
    if kind == "bool":
        return ch.choice([predicate(ch, nodes, rels, k) for k in ("cmp", "null", "bool", "in", "string", "label")]
                         + [f"{n}.b", f"any(x IN [1, 2] WHERE x = {n}.v)"])
    if kind == "nested":
        return ch.choice([f"[reverse([{n}.id, 1])]", f"{{a: {n}.v, b: [{n}.s]}}", f"[{{a: {n}.v}}]",
                          f"{{l: reverse([{n}.id, {m}.id])}}", f"[[{n}.v], [{n}.s, {m}.b]]",
                          f"{n} {{.v, .s}}", f"[tail([{n}.id, 2])]"])
    if kind == "strconv":
        return ch.choice([f"toString({n}.v)", f"toString({n}.id) + {m}.s", f"toString({n}.b)",
                          f"reduce(acc = '', x IN [{n}.id, 1] | acc + toString(x))",
                          f"{n}.s + toString({n}.v)", f"toString({n}.v * 1.5)"])
    if kind == "float":
        return ch.choice([f"{n}.v * 1.5", f"toFloat({n}.v) / 2", f"{n}.v / 0.0", f"{n}.v ^ 0.5",
                          f"-{n}.v / 0.0", f"(0.0 * {n}.v) / 0.0", f"toFloat({n}.id) - 0.5"])
    return ch.choice(LITERALS)


def expr_context(ch: Chooser) -> Case:
    """An expression gives the same value in RETURN as in a WITH, an UNWIND
    of a one-item list, a list index, a map key, a CASE, a comprehension,
    head(), reduce(), CALL {}, and after ORDER BY, LIMIT or a UNION branch
    with no rows."""
    p = pattern(ch, "c1", ["a", "b", "c"], ["r", "q"])
    nodes, rels = p.node_vars, p.rel_vars
    e = context_expr(ch, nodes, rels)
    ctx = ch.pick("ec.ctx")
    keys = ", ".join(f"{v(x)}.id AS k{i}" for i, x in enumerate(nodes))
    keep = ", ".join(v(x) for x in nodes + rels)
    m = f"MATCH {p.text()}"
    base = f"{m} RETURN {keys}, {e} AS x"
    alt = {
        "with": f"{m} WITH {keep}, {e} AS ⟨x⟩ RETURN {keys}, ⟨x⟩ AS x",
        "unwind": f"{m} UNWIND [{e}] AS ⟨x⟩ RETURN {keys}, ⟨x⟩ AS x",
        "unwind_var": f"{m} WITH {keep}, {e} AS ⟨y⟩ UNWIND [⟨y⟩] AS ⟨x⟩ RETURN {keys}, ⟨x⟩ AS x",
        "listidx": f"{m} RETURN {keys}, [{e}][0] AS x",
        "map": f"{m} RETURN {keys}, {{y: {e}}}.y AS x",
        "case": f"{m} RETURN {keys}, CASE WHEN true THEN {e} END AS x",
        "comp": f"{m} RETURN {keys}, [⟨z⟩ IN [1] | {e}][0] AS x",
        "head": f"{m} RETURN {keys}, head([{e}]) AS x",
        "reduce": f"{m} RETURN {keys}, reduce(⟨acc⟩ = null, ⟨z⟩ IN [1] | {e}) AS x",
        "call": f"{m} CALL {{ WITH {keep} RETURN {e} AS ⟨x⟩ }} RETURN {keys}, ⟨x⟩ AS x",
        "order": f"{m} WITH {keep} ORDER BY {v(nodes[0])}.id DESC RETURN {keys}, {e} AS x",
        "limit": f"{m} WITH {keep} LIMIT 1000 RETURN {keys}, {e} AS x",
        "union": (f"{base} UNION ALL MATCH (⟨u⟩) WHERE false RETURN "
                  + ", ".join(f"null AS k{i}" for i in range(len(nodes))) + ", null AS x"),
    }[ctx]
    return Case(f"expr_context/{ctx}/{ch.used['ec.kind']}",
                [Variant("return", render(base)), Variant(ctx, render(alt))])


# ------------------------------------------------------------ CALL { } subqueries

def call_subquery(ch: Chooser) -> Case:
    """A query inside CALL { } gives the rows it gives on its own: alone, as
    a UNION, correlated with UNION branches, after an aggregation, and with
    writes inside (GQLITE-T-0507)."""
    form = ch.pick("cs.form")
    n = ch.randint(1, 3)
    cols = ", ".join(f"c{i}" for i in range(n))
    if form in ("uncorrelated", "union_all", "union", "union_count"):
        q1 = render(chain(ch, n_items=n).query())
        if form == "uncorrelated":
            return Case("call_subquery/uncorrelated", [
                Variant("plain", q1), Variant("call", f"CALL {{ {q1} }} RETURN {cols}")])
        q2 = render(chain(ch, n_items=n).query())
        if form == "union_all":
            return Case("call_subquery/union_all", [
                Variant("union all", f"{q1} UNION ALL {q2}"),
                Variant("call", f"CALL {{ {q1} UNION ALL {q2} }} RETURN {cols}")])
        if form == "union":
            return Case("call_subquery/union", [
                Variant("union", f"{q1} UNION {q2}", post=equiv),
                Variant("call", f"CALL {{ {q1} UNION {q2} }} RETURN {cols}", post=equiv)])
        return Case("call_subquery/union_count", [
            Variant("parts", parts=[q1, q2], combine=lambda ps: [{"n": len(ps[0]) + len(ps[1])}]),
            Variant("call", f"CALL {{ {q1} UNION ALL {q2} }} RETURN count(*) AS n")])
    p = pattern(ch, "c1", ["a", "b"], ["r"])
    nodes, rels = p.node_vars, p.rel_vars
    m = f"MATCH {p.text()}"
    keys = ", ".join(f"{v(x)}.id AS k{i}" for i, x in enumerate(nodes))
    keep = ", ".join(v(x) for x in nodes + rels)
    if form == "correlated_union":
        e1, e2 = expr(ch, nodes, rels), expr(ch, nodes, rels)
        return Case("call_subquery/correlated_union", [
            Variant("unwind", render(f"{m} UNWIND [{e1}, {e2}] AS ⟨x⟩ RETURN {keys}, ⟨x⟩ AS x")),
            Variant("call", render(f"{m} CALL {{ WITH {keep} RETURN {e1} AS ⟨x⟩ UNION ALL "
                                   f"WITH {keep} RETURN {e2} AS ⟨x⟩ }} RETURN {keys}, ⟨x⟩ AS x"))])
    if form == "after_agg":
        return Case("call_subquery/after_agg", [
            Variant("inline", render(f"{m} RETURN count(*) * 2 AS m")),
            Variant("call", render(f"{m} WITH count(*) AS ⟨k⟩ CALL {{ WITH ⟨k⟩ RETURN ⟨k⟩ * 2 AS ⟨y⟩ }} "
                                   f"RETURN ⟨y⟩ AS m"))])
    e1 = write_expr(ch, nodes, False)
    if form == "alone":
        # The whole query is one CALL (the CALL handler, not the row pipeline).
        return Case(f"call_subquery/alone/{ch.used['w.kind']}", [
            Variant("inline", render(f"{m} CREATE (:New {{k: {e1}}})"), mode="write"),
            Variant("call", render(f"CALL {{ {m} CREATE (:New {{k: {e1}}}) }}"), mode="write")],
            write=True, readback="MATCH (n:New) RETURN n.k AS k")
    if form == "after_write":
        # A CALL after a write sees what the write made.
        return Case("call_subquery/after_write", [
            Variant("count", render(f"{m} RETURN count(*) AS c")),
            Variant("call", render(f"{m} CREATE (:New) WITH count(*) AS ⟨w⟩ "
                                   f"CALL {{ MATCH (⟨z⟩:New) RETURN count(⟨z⟩) AS ⟨y⟩ }} RETURN ⟨y⟩ AS c"))])
    if form == "write":
        return Case(f"call_subquery/write/{ch.used['w.kind']}", [
            Variant("inline", render(f"{m} CREATE (:New {{k: {e1}}})"), mode="write"),
            Variant("call", render(f"{m} CALL {{ WITH {keep} CREATE (:New {{k: {e1}}}) }}"), mode="write")],
            write=True, readback="MATCH (n:New) RETURN n.k AS k")
    e2 = write_expr(ch, nodes, False)
    return Case(f"call_subquery/write_union/{ch.used['w.kind']}", [
        Variant("two writes", render(f"{m} CREATE (:New {{k: {e1}}}) CREATE (:New {{k: {e2}}})"), mode="write"),
        Variant("call", render(f"{m} CALL {{ WITH {keep} CREATE (:New {{k: {e1}}}) UNION ALL "
                               f"WITH {keep} CREATE (:New {{k: {e2}}}) }}"), mode="write")],
        write=True, readback="MATCH (n:New) RETURN n.k AS k")


# ------------------------------------------------- writes after OPTIONAL MATCH

def write_optional(ch: Chooser) -> Case:
    """A write after OPTIONAL MATCH sees the rows of the whole chain: a null
    row writes nothing through the null variable (GQLITE-T-0477)."""
    lab = label_text(ch.pick("c1.label"), ch)
    p2 = pattern(ch, "c2", ["b", "c"], ["r", "q"], start="a")
    mvar = p2.node_vars[-1]
    pre = f"MATCH (⟨a⟩{lab})"
    opt = f"{pre} OPTIONAL MATCH {p2.text()}"
    inner = f"{pre} MATCH {p2.text()}"
    form = ch.pick("wo.write")
    m = v(mvar)
    if form == "set_m":
        w, rb = f"SET {m}.z = 1", "MATCH (n) RETURN n.id AS id, n.z AS z"
        variants = [Variant("inner", render(f"{inner} {w}"), mode="write")]
    elif form == "set_label":
        w, rb = f"SET {m}:Hit", "MATCH (n:Hit) RETURN n.id AS id"
        variants = [Variant("inner", render(f"{inner} {w}"), mode="write")]
    elif form == "set_a":
        w, rb = "SET ⟨a⟩.z = 1", "MATCH (n) RETURN n.id AS id, n.z AS z"
        variants = [Variant("match", render(f"{pre} {w}"), mode="write")]
    elif form == "create":
        w, rb = f"CREATE (:New {{a: ⟨a⟩.id, m: {m}.id}})", "MATCH (n:New) RETURN n.a AS a, n.m AS m"
        variants = [Variant("rows", render(f"{opt} RETURN ⟨a⟩.id AS a, {m}.id AS m"))]
    else:
        w = f"CREATE (⟨a⟩)-[:T {{m: {m}.id}}]->(:New)"
        rb = "MATCH (x)-[t:T]->(:New) RETURN x.id AS a, t.m AS m"
        variants = [Variant("rows", render(f"{opt} RETURN ⟨a⟩.id AS a, {m}.id AS m"))]
    return Case(f"write_optional/{form}",
                [Variant("optional", render(f"{opt} {w}"), mode="write")] + variants
                + [Variant("optional+return", render(f"{opt} {w} RETURN count(*) AS cnt"), mode="write")],
                write=True, readback=rb)


# ------------------------------------------------------------- SET forms

def set_forms(ch: Chooser) -> Case:
    """SET a = map equals REMOVE then SET; SET a += map equals SET of items;
    one SET of two items equals two SET clauses; an inline MATCH key that a
    later item rewrites equals a WHERE (GQLITE-T-0478/0479)."""
    lab = label_text(ch.pick("c1.label"), ch)
    e = write_expr(ch, ["a"], False)
    form = ch.pick("sf.form")
    a = "⟨a⟩"
    rb = "MATCH (n) RETURN n.id AS id, properties(n) AS p"
    if form == "replace":
        writes = [f"MATCH ({a}{lab}) SET {a} = {{id: {a}.id, k: {e}}}",
                  f"MATCH ({a}{lab}) WITH {a}, {e} AS ⟨val⟩ REMOVE {a}.v, {a}.s, {a}.b SET {a}.k = ⟨val⟩"]
    elif form == "merge_map":
        writes = [f"MATCH ({a}{lab}) SET {a} += {{k: {e}, v2: {a}.v}}",
                  f"MATCH ({a}{lab}) SET {a}.k = {e}, {a}.v2 = {a}.v"]
    elif form == "items":
        writes = [f"MATCH ({a}{lab}) SET {a}.k = {e}, {a}.v2 = {a}.v",
                  f"MATCH ({a}{lab}) SET {a}.k = {e} SET {a}.v2 = {a}.v"]
    else:   # inline match key rewritten by a later item
        writes = [f"MATCH ({a}{lab} {{id: 0}}) SET {a}.k = {e}, {a}.id = {a}.id",
                  f"MATCH ({a}{lab}) WHERE {a}.id = 0 SET {a}.k = {e}"]
    variants = [Variant(f"w{i}", render(q), mode="write") for i, q in enumerate(writes)]
    variants.append(Variant("w0+return", render(writes[0] + " RETURN count(*) AS cnt"), mode="write"))
    return Case(f"set_forms/{form}/{ch.used['w.kind']}", variants, write=True, readback=rb)


# ---------------------------------------------------- with and without an index

def _ddl(ch: Chooser) -> list[str]:
    form = ch.pick("ix.ddl")
    out = []
    for _ in range(ch.randint(1, 3)):
        lab, prop = ch.choice(LABELS), ch.choice(["v", "s", "id", "b"])
        kind = form if form != "mixed" else ch.choice(["index", "composite", "unique"])
        if kind == "index":
            out.append(f"CREATE INDEX IF NOT EXISTS FOR (n:{lab}) ON (n.{prop})")
        elif kind == "composite":
            out.append(f"CREATE INDEX IF NOT EXISTS FOR (n:{lab}) ON (n.v, n.s)")
        else:   # every generated node has its own id
            out.append(f"CREATE CONSTRAINT IF NOT EXISTS FOR (n:{lab}) REQUIRE n.id IS UNIQUE")
    return out


def indexed(ch: Chooser) -> Case:
    """An index or a uniqueness constraint changes the plan, not the answer:
    each variant of another relation gives the same rows (or leaves the
    same graph) after CREATE INDEX / CREATE CONSTRAINT (GQLITE-T-0511)."""
    base = ch.pick("ix.base")
    case = RELATIONS[base][0](ch)
    ddl = _ddl(ch)
    extra = []
    for var in case.variants:
        n = len(ddl)
        comb = (lambda ps, c=var.combine, n=n: c(ps[n:])) if var.combine else (lambda ps: ps[-1])
        extra.append(Variant(var.label + "+index", parts=ddl + var.queries, combine=comb, post=var.post,
                             keymap=var.keymap, mode=var.mode, repeat=var.repeat,
                             expect_error=var.expect_error, params=var.params))
    case.variants = case.variants + extra
    case.relation = f"indexed/{ch.used['ix.ddl']}/" + case.relation
    return case


# ---------------------------------------------------- aggregates after a write

def write_agg(ch: Chooser) -> Case:
    """An aggregate over what a write made equals the aggregate of the
    values written (the row pipeline aggregates; GQLITE-T-0510)."""
    shape, pre, nodes, scope = _write_prefix(ch)
    e = write_expr(ch, nodes, "i" in scope)
    if shape == "create":
        e = _sub_i(e, 2)
    kind = ch.pick("agg.kind")
    numeric = ch.used.get("w.kind") in ("prop", "arith") and not (nodes and "." in e and e.endswith(".s"))
    if kind in ("sum", "avg", "stdev", "stdevp", "percentile_cont", "percentile_disc", "min") and not numeric:
        kind = "count"
    agg = AGGS[kind]
    link = "(⟨c⟩)-[:L]->" if shape == "match_match" else ""
    return Case(f"write_agg/{kind}/{shape}", [
        Variant("computed", render(f"{pre} WITH {e} AS ⟨k⟩ RETURN {agg('⟨k⟩', '⟨k⟩', None)} AS a")),
        Variant("after create", render(f"{pre} CREATE {link}(⟨n⟩:New {{k: {e}}}) "
                                       f"RETURN {agg('⟨n⟩.k', '⟨n⟩.k', None)} AS a"))],
        write=True)


# ---------------------------------------------------- temporal values

def _temporal(ch: Chooser, kind: str) -> tuple[str, dict]:
    """A temporal constructor of the kind, from a string or a map, and the
    components it was built from."""
    y, mo, d = ch.randint(1990, 2030), ch.randint(1, 12), ch.randint(1, 28)
    h, mi, s = ch.randint(0, 23), ch.randint(0, 59), ch.randint(0, 59)
    c = {"year": y, "month": mo, "day": d, "hour": h, "minute": mi, "second": s}
    by_map = ch.random() < 0.5
    if kind == "date":
        return (f"date({{year: {y}, month: {mo}, day: {d}}})" if by_map else
                f"date('{y:04d}-{mo:02d}-{d:02d}')"), c
    if kind == "datetime":
        return (f"datetime({{year: {y}, month: {mo}, day: {d}, hour: {h}, minute: {mi}, second: {s}, "
                f"timezone: 'Z'}})" if by_map else
                f"datetime('{y:04d}-{mo:02d}-{d:02d}T{h:02d}:{mi:02d}:{s:02d}Z')"), c
    if kind == "localdatetime":
        return (f"localdatetime({{year: {y}, month: {mo}, day: {d}, hour: {h}, minute: {mi}, second: {s}}})"
                if by_map else f"localdatetime('{y:04d}-{mo:02d}-{d:02d}T{h:02d}:{mi:02d}:{s:02d}')"), c
    if kind == "localtime":
        return (f"localtime({{hour: {h}, minute: {mi}, second: {s}}})" if by_map else
                f"localtime('{h:02d}:{mi:02d}:{s:02d}')"), c
    if kind == "time":
        return (f"time({{hour: {h}, minute: {mi}, second: {s}, timezone: '+01:00'}})" if by_map else
                f"time('{h:02d}:{mi:02d}:{s:02d}+01:00')"), c
    dd, dh = ch.randint(0, 40), ch.randint(0, 30)
    return (f"duration({{days: {dd}, hours: {dh}}})" if by_map else f"duration('P{dd}DT{dh}H')"), \
        {"days": dd, "hours": dh}


_FIELDS = {"date": ["year", "month", "day"], "datetime": ["year", "month", "day", "hour", "minute", "second"],
           "localdatetime": ["year", "month", "day", "hour", "minute", "second"],
           "localtime": ["hour", "minute", "second"], "time": ["hour", "minute", "second"]}


def temporal(ch: Chooser) -> Case:
    """Temporal values: the same through WITH / UNWIND / a stored property,
    d + dur - dur = d, ordering and comparison agree with the components,
    components read back what built the value, truncate agrees with a
    constructor (GQLITE-T-0509)."""
    kind = ch.pick("tm.kind")
    form = ch.pick("tm.form")
    e, c = _temporal(ch, kind)
    name = f"temporal/{form}/{kind}"
    if form == "context":
        return Case(name, [Variant("return", render(f"RETURN {e} AS t")),
                           Variant("with", render(f"WITH {e} AS ⟨x⟩ RETURN ⟨x⟩ AS t")),
                           Variant("unwind", render(f"UNWIND [{e}] AS ⟨x⟩ RETURN ⟨x⟩ AS t")),
                           Variant("list", render(f"RETURN [{e}][0] AS t"))])
    if form == "stored":
        return Case(name, [Variant("write", render(f"CREATE (:New {{k: {e}}})"), mode="write"),
                           Variant("return", render(f"RETURN {e} AS k"))],
                    write=True, readback="MATCH (n:New) RETURN n.k AS k")
    if kind == "duration" and form in ("order", "compare", "truncate"):
        form, name = "component", f"temporal/component/{kind}"
    if form == "roundtrip":
        if kind in ("localtime", "time"):
            dur = f"duration({{hours: {ch.randint(0, 5)}, minutes: {ch.randint(0, 59)}}})"
        elif kind == "duration":
            dur = f"duration({{days: {ch.randint(0, 9)}}})"
        else:
            dur = f"duration({{days: {ch.randint(0, 400)}, hours: {ch.randint(0, 30)}}})"
        return Case(name, [Variant("roundtrip", render(f"WITH {e} AS ⟨x⟩ RETURN (⟨x⟩ + {dur}) - {dur} = ⟨x⟩ AS ok")),
                           Variant("true", "RETURN true AS ok")])
    if form == "component":
        if kind == "duration":
            checks = [f"⟨x⟩.days = {c['days']}", f"⟨x⟩.hours = {c['hours']}"]
        else:
            checks = [f"⟨x⟩.{f} = {c[f]}" for f in _FIELDS[kind]]
        return Case(name, [Variant("fields", render(f"WITH {e} AS ⟨x⟩ RETURN {' AND '.join(checks)} AS ok")),
                           Variant("direct", render(f"RETURN {' AND '.join(x.replace('⟨x⟩', '(' + e + ')') for x in checks)} AS ok")),
                           Variant("true", "RETURN true AS ok")])
    key = f"⟨x⟩.{_FIELDS[kind][0]}"
    for f in _FIELDS[kind][1:]:
        key = f"({key}) * 100 + ⟨x⟩.{f}"
    if form == "order":
        es = [e] + [_temporal(ch, kind)[0] for _ in range(ch.randint(1, 3))]
        return Case(name, [
            Variant("by value", render(f"UNWIND [{', '.join(es)}] AS ⟨x⟩ RETURN ⟨x⟩ AS t ORDER BY ⟨x⟩")),
            Variant("by key", render(f"UNWIND [{', '.join(es)}] AS ⟨x⟩ RETURN ⟨x⟩ AS t ORDER BY {key}"))],
            compare="list")
    if form == "compare":
        e2 = _temporal(ch, kind)[0]
        key2 = key.replace("⟨x⟩", "⟨y⟩")
        return Case(name, [
            Variant("values", render(f"WITH {e} AS ⟨x⟩, {e2} AS ⟨y⟩ RETURN ⟨x⟩ < ⟨y⟩ AS lt, ⟨x⟩ = ⟨y⟩ AS eq")),
            Variant("keys", render(f"WITH {e} AS ⟨x⟩, {e2} AS ⟨y⟩ RETURN {key} < {key2} AS lt, {key} = {key2} AS eq"))])
    # truncate
    if kind == "date":
        tr, alt = "date.truncate('month', ⟨x⟩)", "date({year: ⟨x⟩.year, month: ⟨x⟩.month, day: 1})"
    elif kind in ("datetime", "localdatetime"):
        tz = ", timezone: 'Z'" if kind == "datetime" else ""
        tr = f"{kind}.truncate('day', ⟨x⟩)"
        alt = f"{kind}({{year: ⟨x⟩.year, month: ⟨x⟩.month, day: ⟨x⟩.day{tz}}})"
    else:
        tz = ", timezone: '+01:00'" if kind == "time" else ""
        tr = f"{kind}.truncate('hour', ⟨x⟩)"
        alt = f"{kind}({{hour: ⟨x⟩.hour{tz}}})"
    return Case(name, [Variant("truncate", render(f"WITH {e} AS ⟨x⟩ RETURN {tr} AS t")),
                       Variant("constructor", render(f"WITH {e} AS ⟨x⟩ RETURN {alt} AS t"))])


# ---------------------------------------------------- more write forms

def write_forms(ch: Chooser) -> Case:
    """SET from a $map or a map value, DELETE ... RETURN, REMOVE of a label
    and FOREACH (SET, MERGE, over a computed list) equal the plain forms
    (GQLITE-T-0508)."""
    form = ch.pick("wf.form")
    lab = label_text(ch.pick("c1.label"), ch)
    cond = label_where(lab, "a")
    a = "⟨a⟩"
    m = f"MATCH ({a}{lab})"
    props = "MATCH (n) RETURN properties(n) AS p"
    if form in ("set_param_merge", "set_param_replace"):
        k = ch.choice([1, "x", 1.5, True, [1, 2]])
        if form == "set_param_merge":
            pm = {"k": k, "t": "q", "v": None}
            return Case("write_forms/set_param_merge", [
                Variant("param", render(f"{m} SET {a} += $props"), mode="write", params={"props": pm}),
                Variant("items", render(f"{m} SET {a}.k = {lit(k)}, {a}.t = 'q' REMOVE {a}.v"), mode="write")],
                write=True, readback=props)
        return Case("write_forms/set_param_replace", [
            Variant("param", render(f"{m} SET {a} = $props"), mode="write", params={"props": {"k": k}}),
            Variant("items", render(f"{m} REMOVE {a}.id, {a}.v, {a}.s, {a}.b SET {a}.k = {lit(k)}"),
                    mode="write")],
            write=True, readback=props)
    if form == "set_map_value":
        e = write_expr(ch, ["a"], False)
        return Case(f"write_forms/set_map_value/{ch.used['w.kind']}", [
            Variant("map", render(f"{m} WITH {a}, {{k: {e}, w: {a}.id}} AS ⟨mm⟩ SET {a} += ⟨mm⟩"), mode="write"),
            Variant("items", render(f"{m} SET {a}.k = {e}, {a}.w = {a}.id"), mode="write")],
            write=True, readback="MATCH (n) RETURN n.id AS id, properties(n) AS p")
    if form == "delete_return":
        # Rows of a DELETE ... RETURN (each variant runs on a fresh graph).
        return Case("write_forms/delete_return", [
            Variant("count", render(f"MATCH ({a}) WHERE {cond} RETURN count(*) AS c, sum({a}.id) AS s")),
            Variant("delete", render(f"{m} WITH {a}, {a}.id AS ⟨i⟩ DETACH DELETE {a} "
                                     f"RETURN count(*) AS c, sum(⟨i⟩) AS s"))],
            write=True)
    if form == "remove_label":
        return Case("write_forms/remove_label", [
            Variant("remove", render(f"{m} REMOVE {a}:A"), mode="write"),
            Variant("set+remove", render(f"{m} SET {a}:Tmp REMOVE {a}:A, {a}:Tmp"), mode="write"),
            Variant("computed", render(f"MATCH ({a}) RETURN {a}.id AS id, CASE WHEN {cond} THEN "
                                       f"[⟨l⟩ IN labels({a}) WHERE ⟨l⟩ <> 'A'] ELSE labels({a}) END AS l"))],
            write=True, readback="MATCH (n) RETURN n.id AS id, labels(n) AS l")
    if form == "foreach_set":
        e = write_expr(ch, ["a"], False)
        return Case(f"write_forms/foreach_set/{ch.used['w.kind']}", [
            Variant("set", render(f"{m} SET {a}.k = {e}"), mode="write"),
            Variant("foreach", render(f"{m} WITH collect({a}) AS ⟨ns⟩ FOREACH ({a} IN ⟨ns⟩ | SET {a}.k = {e})"),
                    mode="write")],
            write=True, readback="MATCH (n) RETURN n.id AS id, n.k AS k")
    if form == "foreach_merge":
        xs = [ch.randint(0, 3) for _ in range(ch.randint(1, 4))]
        return Case("write_forms/foreach_merge", [
            Variant("unwind", render(f"UNWIND {xs} AS ⟨i⟩ MERGE (:New {{k: ⟨i⟩}})"), mode="write"),
            Variant("foreach", render(f"FOREACH (⟨i⟩ IN {xs} | MERGE (:New {{k: ⟨i⟩}}))"), mode="write"),
            Variant("distinct", render(f"UNWIND {xs} AS ⟨i⟩ RETURN DISTINCT ⟨i⟩ AS k"))],
            write=True, readback="MATCH (n:New) RETURN n.k AS k")
    src = ch.choice([f"range(0, coalesce({a}.v, 0) % 3)", f"[{a}.id, {a}.id + 1]",
                     f"[x IN [1, 2, 3] WHERE x <= coalesce({a}.v, 0)]", "[]"])
    return Case("write_forms/foreach_list", [
        Variant("unwind", render(f"{m} UNWIND {src} AS ⟨i⟩ CREATE (:New {{k: {a}.id * 10 + ⟨i⟩}})"), mode="write"),
        Variant("foreach", render(f"{m} FOREACH (⟨i⟩ IN {src} | CREATE (:New {{k: {a}.id * 10 + ⟨i⟩}}))"),
                mode="write")],
        write=True, readback="MATCH (n:New) RETURN n.k AS k")


# ---------------------------------------------------- write with RETURN

def write_return(ch: Chooser) -> Case:
    """A write leaves the same graph with a trailing RETURN as without one:
    the two take different paths through the engine."""
    base = ch.pick("wr.base")
    case = RELATIONS[base][0](ch)
    extra = [Variant(var.label + "+return", query=var.query and var.query + " RETURN count(*) AS cnt",
                     parts=var.parts, mode="write", repeat=var.repeat)
             for var in case.variants if var.mode == "write" and "RETURN" not in (var.query or "")]
    case.variants = [v_ for v_ in case.variants if v_.mode == "write"
                     and "RETURN" not in (v_.query or "")][:1] + extra[:1]
    case.relation = "write_return/" + case.relation
    return case


# ----------------------------------------------- writes: order, batching, undo
# GQLITE-T-0526. The read-back of these is the whole graph.

DUMP = ("MATCH (n) OPTIONAL MATCH (n)-[r]->(m) RETURN labels(n) AS l, properties(n) AS p, "
        "type(r) AS t, properties(r) AS rp, properties(m) AS mp")
NOOP = "MATCH (n) WHERE false SET n.zz = 1"


def _sub_i(e: str, x) -> str:
    return e.replace("⟨i⟩", str(x))


def write_order(ch: Chooser) -> Case:
    """Independent writes in two orders leave the same graph."""
    writes = []
    for j in range(3):
        if ch.random() < 0.5:
            e = _sub_i(write_expr(ch, [], False), j + 1)
            writes.append(f"CREATE (:New {{k: {e}, j: {j}}})")
        else:
            e = write_expr(ch, ["a"], False)
            writes.append(f"MATCH (⟨a⟩) WHERE ⟨a⟩.id = {j} SET ⟨a⟩.k{j} = {e}")
    order = writes[:]
    while order == writes:
        ch.rng.shuffle(order)
    return Case(f"write_order/{ch.used['w.kind']}", [
        Variant("order1", parts=[render(q) for q in writes], mode="write"),
        Variant("order2", parts=[render(q) for q in order], mode="write")],
        write=True, readback=DUMP)


def write_batching(ch: Chooser) -> Case:
    """N one-row writes equal one UNWIND write, and one FOREACH or IN-list write."""
    form = ch.pick("wb.form")
    xs = sorted(ch.sample(range(0, 6), 3))
    if form == "create":
        e = write_expr(ch, [], False)
        return Case(f"write_batching/create/{ch.used['w.kind']}", [
            Variant("single", parts=[render(f"CREATE (:New {{k: {_sub_i(e, x)}}})") for x in xs], mode="write"),
            Variant("unwind", render(f"UNWIND {xs} AS ⟨i⟩ CREATE (:New {{k: {e}}})"), mode="write"),
            Variant("foreach", render(f"FOREACH (⟨i⟩ IN {xs} | CREATE (:New {{k: {e}}}))"), mode="write")],
            write=True, readback="MATCH (n:New) RETURN n.k AS k")
    e = write_expr(ch, ["a"], False)
    return Case(f"write_batching/set/{ch.used['w.kind']}", [
        Variant("single", parts=[render(f"MATCH (⟨a⟩) WHERE ⟨a⟩.id = {x} SET ⟨a⟩.k = {e}") for x in xs],
                mode="write"),
        Variant("unwind", render(f"UNWIND {xs} AS ⟨i⟩ MATCH (⟨a⟩) WHERE ⟨a⟩.id = ⟨i⟩ SET ⟨a⟩.k = {e}"),
                mode="write"),
        Variant("in_list", render(f"MATCH (⟨a⟩) WHERE ⟨a⟩.id IN {xs} SET ⟨a⟩.k = {e}"), mode="write")],
        write=True, readback="MATCH (n) RETURN n.id AS id, n.k AS k")


def write_undo(ch: Chooser) -> Case:
    """A write followed by its inverse leaves the graph as it was."""
    form = ch.pick("wu.form")
    lab = label_text(ch.pick("c1.label"), ch)
    if form == "create":
        shape, pre, nodes, scope = _write_prefix(ch)
        e = write_expr(ch, nodes, "i" in scope)
        if shape == "create":
            e = _sub_i(e, 2)
        link = "(⟨c⟩)-[:L]->" if shape == "match_match" else ""
        do = f"{pre} CREATE {link}(:New {{k: {e}}})"
        undo = "MATCH (n:New) DETACH DELETE n"
    elif form == "set":
        e = write_expr(ch, ["a"], False)
        do = f"MATCH (⟨a⟩{lab}) SET ⟨a⟩.k = {e}"
        undo = "MATCH (n) REMOVE n.k"
    elif form == "label":
        do = f"MATCH (⟨a⟩{lab}) SET ⟨a⟩:Tmp"
        undo = "MATCH (n:Tmp) REMOVE n:Tmp"
    else:
        do = (f"MATCH (⟨a⟩{lab}), (⟨b⟩) WHERE ⟨b⟩.id = 0 "
              f"CREATE (⟨a⟩)-[:TMP {{w: ⟨a⟩.id}}]->(⟨b⟩)")
        undo = "MATCH ()-[r:TMP]->() DELETE r"
    return Case(f"write_undo/{form}", [
        Variant("nothing", NOOP, mode="write"),
        Variant("do+undo", parts=[render(do), undo], mode="write")],
        write=True, readback=DUMP)


def write_rollback(ch: Chooser) -> Case:
    """A write that fails part-way, or is rolled back, leaves nothing."""
    form = ch.pick("wrb.form")
    big = 9223372036854775807
    if form == "unwind_create":
        q = f"UNWIND [1, 2, {big}] AS ⟨x⟩ CREATE (:New {{k: ⟨x⟩ + 1}})"
    elif form == "merge_overflow":
        q = f"UNWIND [1, {big}] AS ⟨x⟩ MERGE (:New {{k: ⟨x⟩ + 1}})"
    elif form == "set_overflow":
        # The UNWIND makes rows on any graph: the first row writes, the
        # second overflows.
        q = (f"UNWIND [0, 1] AS ⟨x⟩ OPTIONAL MATCH (⟨a⟩) WHERE ⟨a⟩.id = ⟨x⟩ SET ⟨a⟩.k = 1 "
             f"CREATE (:New {{k: CASE WHEN ⟨x⟩ = 1 THEN {big} + ⟨x⟩ ELSE 0 END}})")
    else:
        e = _sub_i(write_expr(ch, [], False), 1)
        return Case("write_rollback/rollback", [
            Variant("nothing", NOOP, mode="write"),
            Variant("rolled back", parts=["SQL:BEGIN", render(f"CREATE (:New {{k: {e}}})"),
                                          "MATCH (n) SET n.k2 = 1", "SQL:ROLLBACK"], mode="write")],
            write=True, readback=DUMP)
    return Case(f"write_rollback/{form}", [
        Variant("nothing", NOOP, mode="write"),
        Variant("fails", render(q), mode="write", expect_error=True)],
        write=True, readback=DUMP)


def create_return(ch: Chooser) -> Case:
    """CREATE ... RETURN gives what a MATCH of the created node reads."""
    shape, pre, nodes, scope = _write_prefix(ch)
    e = write_expr(ch, nodes, "i" in scope)
    if shape == "create":
        e = _sub_i(e, 2)
    link = "(⟨c⟩)-[:L]->" if shape == "match_match" else ""
    return Case(f"create_return/{ch.used['w.kind']}/{shape}", [
        Variant("write", render(f"{pre} CREATE {link}(:New {{k: {e}}})"), mode="write"),
        Variant("returned", render(f"{pre} CREATE {link}(⟨n⟩:New {{k: {e}}}) RETURN ⟨n⟩.k AS k"))],
        write=True, readback="MATCH (n:New) RETURN n.k AS k")


RELATIONS: dict[str, tuple[Callable, list[str]]] = {
    "with_star": (with_star, CHAIN),
    "named_path": (named_path, G + ["c1.hops", "c1.label", "c1.dir", "c1.type", "np.carry"]),
    "rename": (rename, CHAIN),
    "keyword_name": (keyword_name, CHAIN),
    "predicate_move": (predicate_move, G + C1 + C2_FORCED + ["pred.kind", "expr.kind"]),
    "reverse_pattern": (reverse_pattern, G + C1 + C2_FORCED + ["rp.clause", "expr.kind"]),
    "split_path": (split_path, G + ["c1.dir", "c1.type", "c1.label", "expr.kind"]),
    "varlen": (varlen, G + ["c1.dir", "c1.type", "c1.label", "vl.range", "expr.kind"]),
    "varlen_comprehension": (varlen_comprehension, G + ["c1.dir", "c1.type", "c1.label", "vl.range"]),
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
    "merge_direct": (merge_direct, G + C1 + ["w.shape", "w.kind"]),
    "merge_rel": (merge_rel, G + C1 + ["w.shape", "w.kind", "mr.end"]),
    "merge_on": (merge_on, G + C1 + ["w.shape", "w.kind", "mo.form"]),
    "call_subquery": (call_subquery, CHAIN + ["cs.form", "w.kind"]),
    "write_forms": (write_forms, G + ["c1.label", "w.kind", "wf.form"]),
    "temporal": (temporal, ["tm.kind", "tm.form"]),
    "write_agg": (write_agg, G + C1 + ["w.shape", "w.kind", "agg.kind"]),
    "indexed": (indexed, CHAIN + ["ix.base", "ix.ddl", "pred.kind", "expr.kind", "w.shape", "w.kind",
                                  "del.form", "ld.form"]),
    "set_readback": (set_readback, G + ["c1.label", "w.kind"]),
    "delete_count": (delete_count, G + ["c1.label", "c1.type", "c1.dir", "del.form"]),
    "expr_context": (expr_context, G + C1 + ["ec.ctx", "ec.kind"]),
    "write_optional": (write_optional, G + ["c1.label"] + C2_FORCED + ["wo.write"]),
    "set_forms": (set_forms, G + ["c1.label", "w.kind", "sf.form"]),
    "write_return": (write_return, G + C1 + ["c1.label", "w.shape", "w.kind", "wr.base"]),
    "write_order": (write_order, G + ["w.kind"]),
    "write_batching": (write_batching, G + ["wb.form", "w.kind"]),
    "write_undo": (write_undo, G + C1 + ["wu.form", "w.shape", "w.kind"]),
    "write_rollback": (write_rollback, G + ["wrb.form", "w.kind"]),
    "create_return": (create_return, G + C1 + ["w.shape", "w.kind"]),
}
