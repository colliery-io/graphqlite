"""Value types round-trip through every write path and every read path.

A value written by one path must come back with the same type from every
read path: 1 stays an integer, 1.5 a float, true a boolean, and the text
'123' a string. #122, #123, #124, #128 and GQLITE-T-0386..0388 each broke
this on one path (GQLITE-T-0397).

The comparison is strict on type: True is not 1, and 2.0 is not 2.
"""

import json

import pytest

from graphqlite import Graph

from test_graph import get_extension_path


# name -> (Cypher literal, Python value)
VALUES = {
    "int": ("42", 42),
    "neg_int": ("-7", -7),
    "zero": ("0", 0),
    "float": ("1.5", 1.5),
    "whole_float": ("2.0", 2.0),
    "true": ("true", True),
    "false": ("false", False),
    "text": ("'abc'", "abc"),
    "text_int": ("'123'", "123"),
    "text_float": ("'1.5'", "1.5"),
    "text_true": ("'true'", "true"),
    "text_false": ("'false'", "false"),
    "text_null": ("'null'", "null"),
    "text_zip": ("'02134'", "02134"),
    "text_empty": ("''", ""),
    "text_list": ("'[1]'", "[1]"),
    "text_map": ("'{\"a\":1}'", '{"a":1}'),
    "text_quote": ("'say \"hi\"'", 'say "hi"'),
    "text_apostrophe": ('"it\'s"', "it's"),
    "float_precise": ("3.14159265358979", 3.14159265358979),
    "list_int": ("[1, 2]", [1, 2]),
    "list_float": ("[1.5, 2.0]", [1.5, 2.0]),
    "list_bool": ("[true, false]", [True, False]),
    "list_text": ("['1', 'true', 'x']", ["1", "true", "x"]),
    "list_mixed": ("[1, 'a', 1.5, true]", [1, "a", 1.5, True]),
    "list_empty": ("[]", []),
}

# Values that are not legal property values but are legal expression values.
EXPR_ONLY_VALUES = {
    "null": ("null", None),
    "map": ("{a: 1, b: 'x', c: true, d: 1.5}", {"a": 1, "b": "x", "c": True, "d": 1.5}),
    "nested": ("[[1, true], {k: '2'}]", [[1, True], {"k": "2"}]),
}


def same(a, b):
    """Equal and of the same type, recursively."""
    if type(a) is not type(b):
        return False
    if isinstance(a, list):
        return len(a) == len(b) and all(same(x, y) for x, y in zip(a, b))
    if isinstance(a, dict):
        return a.keys() == b.keys() and all(same(a[k], b[k]) for k in a)
    return a == b


def describe(v):
    return f"{v!r} ({type(v).__name__})"


@pytest.fixture
def g():
    graph = Graph(":memory:", extension_path=get_extension_path())
    yield graph
    graph.close()


def one(rows):
    assert len(rows) == 1, rows
    return rows[0]["v"]


# --- ways in: each leaves exactly one :V node with property p ---------------

def _w_create_inline(g, lit, val):
    g.query(f"CREATE (:V {{p: {lit}}})")


def _w_create_param(g, lit, val):
    g.query("CREATE (:V {p: $v})", {"v": val})


def _w_set(g, lit, val):
    g.query("CREATE (:V)")
    g.query(f"MATCH (n:V) SET n.p = {lit}")


def _w_set_param(g, lit, val):
    g.query("CREATE (:V)")
    g.query("MATCH (n:V) SET n.p = $v", {"v": val})


def _w_set_map(g, lit, val):
    g.query("CREATE (:V)")
    g.query(f"MATCH (n:V) SET n += {{p: {lit}}}")


def _w_merge_inline(g, lit, val):
    g.query(f"MERGE (:V {{p: {lit}}})")


def _w_merge_on_create(g, lit, val):
    g.query(f"MERGE (n:V {{k: 1}}) ON CREATE SET n.p = {lit}")


def _w_with(g, lit, val):
    g.query(f"WITH {lit} AS x CREATE (:V {{p: x}})")


def _w_unwind_literal(g, lit, val):
    g.query(f"UNWIND [{lit}] AS x CREATE (:V {{p: x}})")


def _w_unwind_param(g, lit, val):
    g.query("UNWIND $xs AS x CREATE (:V {p: x})", {"xs": [val]})


def _w_unwind_param_set(g, lit, val):
    g.query("UNWIND $xs AS x CREATE (n:V) SET n.p = x", {"xs": [val]})


def _w_unwind_rows(g, lit, val):
    g.query("UNWIND $rows AS r CREATE (:V {p: r.p})", {"rows": [{"p": val}]})


def _w_unwind_rows_merge(g, lit, val):
    g.query("UNWIND $rows AS r MERGE (:V {p: r.p})", {"rows": [{"p": val}]})


def _w_set_after_with(g, lit, val):
    g.query("CREATE (:V)")
    g.query(f"MATCH (n:V) WITH n SET n.p = {lit}")


def _w_set_copy(g, lit, val):
    g.query(f"CREATE (:V {{q: {lit}}})")
    g.query("MATCH (n:V) SET n.p = n.q")


def _w_set_copy_after_with(g, lit, val):
    g.query(f"CREATE (:V {{q: {lit}}})")
    g.query("MATCH (n:V) WITH n SET n.p = n.q")


def _w_upsert_node(g, lit, val):
    g.upsert_node("x", {"p": val}, label="V")


NODE_WRITES = {
    "create_inline": _w_create_inline,
    "create_param": _w_create_param,
    "set": _w_set,
    "set_param": _w_set_param,
    "set_map": _w_set_map,
    "merge_inline": _w_merge_inline,
    "merge_on_create": _w_merge_on_create,
    "with": _w_with,
    "unwind_literal": _w_unwind_literal,
    "unwind_param": _w_unwind_param,
    "unwind_param_set": _w_unwind_param_set,
    "unwind_rows": _w_unwind_rows,
    "unwind_rows_merge": _w_unwind_rows_merge,
    "set_after_with": _w_set_after_with,
    "set_copy": _w_set_copy,
    "set_copy_after_with": _w_set_copy_after_with,
    "upsert_node": _w_upsert_node,
}


# --- ways out: each returns the value of p of the one :V node ---------------

NODE_READS = {
    "return_prop": lambda g: one(g.query("MATCH (n:V) RETURN n.p AS v")),
    "return_node": lambda g: one(g.query("MATCH (n:V) RETURN n AS v"))["properties"]["p"],
    "properties": lambda g: one(g.query("MATCH (n:V) RETURN properties(n) AS v"))["p"],
    "map_projection": lambda g: one(g.query("MATCH (n:V) RETURN n {.p} AS v"))["p"],
    "in_list": lambda g: one(g.query("MATCH (n:V) RETURN [n.p] AS v"))[0],
    "in_map": lambda g: one(g.query("MATCH (n:V) RETURN {x: n.p} AS v"))["x"],
    "with_prop": lambda g: one(g.query("MATCH (n:V) WITH n.p AS x RETURN x AS v")),
    "with_node": lambda g: one(g.query("MATCH (n:V) WITH n RETURN n.p AS v")),
    "collect": lambda g: one(g.query("MATCH (n:V) RETURN collect(n.p) AS v"))[0],
    "unwind": lambda g: one(g.query("MATCH (n:V) UNWIND [n.p] AS x RETURN x AS v")),
    "case": lambda g: one(g.query("MATCH (n:V) RETURN CASE WHEN true THEN n.p END AS v")),
    "coalesce": lambda g: one(g.query("MATCH (n:V) RETURN coalesce(n.p, 0) AS v")),
    "iter_query": lambda g: [r["v"] for r in g.iter_query("MATCH (n:V) RETURN n.p AS v")][0],
    "get_node": lambda g: g.get_node("x")["properties"]["p"],
}


@pytest.mark.parametrize("value", list(VALUES))
@pytest.mark.parametrize("write", list(NODE_WRITES))
def test_node_property_round_trip(g, write, value):
    lit, val = VALUES[value]
    NODE_WRITES[write](g, lit, val)
    bad = {}
    for name, read in NODE_READS.items():
        if name == "get_node" and write != "upsert_node":
            continue
        try:
            got = read(g)
        except Exception as exc:  # noqa: BLE001 - report every read path
            bad[name] = f"error: {exc}"
            continue
        if not same(got, val):
            bad[name] = describe(got)
    assert not bad, f"wrote {describe(val)} by {write}; reads: {bad}"


# --- relationships -----------------------------------------------------------

def _e_create_inline(g, lit, val):
    g.query(f"CREATE (:A)-[:R {{p: {lit}}}]->(:B)")


def _e_create_param(g, lit, val):
    g.query("CREATE (:A)-[:R {p: $v}]->(:B)", {"v": val})


def _e_set(g, lit, val):
    g.query("CREATE (:A)-[:R]->(:B)")
    g.query(f"MATCH ()-[r:R]->() SET r.p = {lit}")


def _e_set_param(g, lit, val):
    g.query("CREATE (:A)-[:R]->(:B)")
    g.query("MATCH ()-[r:R]->() SET r.p = $v", {"v": val})


def _e_set_after_with(g, lit, val):
    g.query("CREATE (:A)-[:R]->(:B)")
    g.query(f"MATCH ()-[r:R]->() WITH r SET r.p = {lit}")


def _e_set_copy_after_with(g, lit, val):
    g.query(f"CREATE (:A)-[:R {{q: {lit}}}]->(:B)")
    g.query("MATCH ()-[r:R]->() WITH r SET r.p = r.q")


def _e_merge(g, lit, val):
    g.query("CREATE (:A), (:B)")
    g.query(f"MATCH (a:A), (b:B) MERGE (a)-[:R {{p: {lit}}}]->(b)")


def _e_unwind_rows(g, lit, val):
    g.query("CREATE (:A), (:B)")
    g.query(
        "UNWIND $rows AS r MATCH (a:A), (b:B) CREATE (a)-[:R {p: r.p}]->(b)",
        {"rows": [{"p": val}]},
    )


def _e_upsert_edge(g, lit, val):
    g.upsert_node("a", {}, label="A")
    g.upsert_node("b", {}, label="B")
    g.upsert_edge("a", "b", {"p": val}, rel_type="R")


EDGE_WRITES = {
    "create_inline": _e_create_inline,
    "create_param": _e_create_param,
    "set": _e_set,
    "set_param": _e_set_param,
    "set_after_with": _e_set_after_with,
    "set_copy_after_with": _e_set_copy_after_with,
    "merge": _e_merge,
    "unwind_rows": _e_unwind_rows,
    "upsert_edge": _e_upsert_edge,
}

EDGE_READS = {
    "return_prop": lambda g: one(g.query("MATCH ()-[r:R]->() RETURN r.p AS v")),
    "return_rel": lambda g: one(g.query("MATCH ()-[r:R]->() RETURN r AS v"))["properties"]["p"],
    "properties": lambda g: one(g.query("MATCH ()-[r:R]->() RETURN properties(r) AS v"))["p"],
    "in_list": lambda g: one(g.query("MATCH ()-[r:R]->() RETURN [r.p] AS v"))[0],
    "with_prop": lambda g: one(g.query("MATCH ()-[r:R]->() WITH r.p AS x RETURN x AS v")),
    "with_rel": lambda g: one(g.query("MATCH ()-[r:R]->() WITH r RETURN r.p AS v")),
    "collect": lambda g: one(g.query("MATCH ()-[r:R]->() RETURN collect(r.p) AS v"))[0],
    "unwind": lambda g: one(g.query("MATCH ()-[r:R]->() UNWIND [r.p] AS x RETURN x AS v")),
}


@pytest.mark.parametrize("value", list(VALUES))
@pytest.mark.parametrize("write", list(EDGE_WRITES))
def test_edge_property_round_trip(g, write, value):
    lit, val = VALUES[value]
    EDGE_WRITES[write](g, lit, val)
    bad = {}
    for name, read in EDGE_READS.items():
        try:
            got = read(g)
        except Exception as exc:  # noqa: BLE001 - report every read path
            bad[name] = f"error: {exc}"
            continue
        if not same(got, val):
            bad[name] = describe(got)
    assert not bad, f"wrote {describe(val)} by {write}; reads: {bad}"


# --- expressions, no storage -------------------------------------------------

EXPRESSIONS = {
    "return_literal": lambda g, lit, val: one(g.query(f"RETURN {lit} AS v")),
    "return_param": lambda g, lit, val: one(g.query("RETURN $v AS v", {"v": val})),
    "with_literal": lambda g, lit, val: one(g.query(f"WITH {lit} AS x RETURN x AS v")),
    "with_param": lambda g, lit, val: one(g.query("WITH $v AS x RETURN x AS v", {"v": val})),
    "unwind_literal": lambda g, lit, val: one(g.query(f"UNWIND [{lit}] AS x RETURN x AS v")),
    "unwind_param": lambda g, lit, val: one(g.query("UNWIND $xs AS x RETURN x AS v", {"xs": [val]})),
    "unwind_param_with": lambda g, lit, val: one(
        g.query("UNWIND $xs AS x WITH x RETURN x AS v", {"xs": [val]})),
    "unwind_rows": lambda g, lit, val: one(
        g.query("UNWIND $rows AS r RETURN r.p AS v", {"rows": [{"p": val}]})),
    "list_index": lambda g, lit, val: one(g.query(f"RETURN [{lit}][0] AS v")),
    "map_key": lambda g, lit, val: one(g.query(f"RETURN {{k: {lit}}}.k AS v")),
    # collect() drops nulls, so collect of [null] is [].
    "collect_unwind": lambda g, lit, val: (one(
        g.query(f"UNWIND [{lit}] AS x RETURN collect(x) AS v")) or [None])[0],
}


@pytest.mark.parametrize("value", list(VALUES) + list(EXPR_ONLY_VALUES))
def test_expression_round_trip(g, value):
    lit, val = {**VALUES, **EXPR_ONLY_VALUES}[value]
    bad = {}
    for name, fn in EXPRESSIONS.items():
        try:
            got = fn(g, lit, val)
        except Exception as exc:  # noqa: BLE001 - report every path
            bad[name] = f"error: {exc}"
            continue
        if not same(got, val):
            bad[name] = describe(got)
    assert not bad, f"value {describe(val)}; paths: {bad}"


def test_values_are_json_round_trippable():
    """Guard for the table itself: every expected value is plain JSON."""
    for _, val in {**VALUES, **EXPR_ONLY_VALUES}.values():
        assert same(json.loads(json.dumps(val)), val)
