"""Write + RETURN queries return only the matched rows, and change only them.

Each graph has decoys: nodes and relationships that the query must neither
return nor change. Each case checks the exact returned rows and the stored
state after. #128 point 1 (SET/REMOVE + RETURN returned every node of the
label) went unnoticed because no test had a row that must not come back
(GQLITE-T-0398).
"""

import pytest

from graphqlite import Graph

from test_graph import get_extension_path

SETUP = """
CREATE (a:Item {id: 'a', v: 1}), (b:Item {id: 'b', v: 2}), (c:Item {id: 'c', v: 3}),
       (o:Other {id: 'a', v: 1}),
       (a)-[:R {w: 1}]->(b), (b)-[:R {w: 2}]->(c), (o)-[:R {w: 1}]->(a)
"""

STATE = (
    "MATCH (n) RETURN labels(n) AS l, n.id AS id, n.v AS v, n.x AS x "
    "ORDER BY id, l[0]"
)
EDGE_STATE = (
    "MATCH (s)-[r]->(t) RETURN s.id AS s, type(r) AS t, r.w AS w, t.id AS d "
    "ORDER BY s, d, t"
)


def node(id, v, label="Item", x=None, labels=None):
    return {"l": labels or [label], "id": id, "v": v, "x": x}


BASE = [
    node("a", 1),
    node("a", 1, label="Other"),
    node("b", 2),
    node("c", 3),
]

BASE_EDGES = [
    {"s": "a", "t": "R", "w": 1, "d": "b"},
    {"s": "a", "t": "R", "w": 1, "d": "a"},
    {"s": "b", "t": "R", "w": 2, "d": "c"},
]


def with_node(nodes, id, label="Item", **changes):
    out = []
    for n in nodes:
        if n["id"] == id and n["l"][0] == label:
            n = {**n, **changes}
        out.append(n)
    return out


def sort_state(rows):
    """Labels have no order in Cypher: compare them sorted."""
    rows = [{**r, "l": sorted(r["l"])} for r in rows]
    return sorted(rows, key=lambda r: (str(r.get("id")), str(r.get("l"))))


# (name, query, params, expected rows (order-free), expected nodes or None
#  for unchanged, expected edges or None for unchanged)
CASES = [
    ("set_inline_match",
     "MATCH (n:Item {id: 'a'}) SET n.v = 7 RETURN n.id AS id",
     None, [{"id": "a"}], with_node(BASE, "a", v=7), None),
    ("set_where",
     "MATCH (n:Item) WHERE n.id = 'a' SET n.v = 7 RETURN n.id AS id",
     None, [{"id": "a"}], with_node(BASE, "a", v=7), None),
    ("set_param_match",
     "MATCH (n:Item {id: $id}) SET n.v = 7 RETURN n.id AS id",
     {"id": "a"}, [{"id": "a"}], with_node(BASE, "a", v=7), None),
    ("set_returns_new_value",
     "MATCH (n:Item {id: 'a'}) SET n.v = 7 RETURN n.id AS id, n.v AS v",
     None, [{"id": "a", "v": 7}], with_node(BASE, "a", v=7), None),
    ("set_where_property_it_changes",
     "MATCH (n:Item) WHERE n.v = 1 SET n.v = 9 RETURN n.id AS id, n.v AS v",
     None, [{"id": "a", "v": 9}], with_node(BASE, "a", v=9), None),
    ("set_inline_property_it_changes",
     "MATCH (n:Item {v: 1}) SET n.v = 9 RETURN n.id AS id",
     None, [{"id": "a"}], with_node(BASE, "a", v=9), None),
    ("set_multi_row",
     "MATCH (n:Item) WHERE n.v < 3 SET n.x = 1 RETURN n.id AS id",
     None, [{"id": "a"}, {"id": "b"}],
     with_node(with_node(BASE, "a", x=1), "b", x=1), None),
    ("set_map_merge",
     "MATCH (n:Item {id: 'a'}) SET n += {x: 5} RETURN n.id AS id",
     None, [{"id": "a"}], with_node(BASE, "a", x=5), None),
    ("set_label",
     "MATCH (n:Item {id: 'a'}) SET n:Hot RETURN n.id AS id",
     None, [{"id": "a"}], with_node(BASE, "a", l=["Item", "Hot"]), None),
    ("set_and_remove",
     "MATCH (n:Item {id: 'a'}) SET n.x = 1 REMOVE n.v RETURN n.id AS id, n.x AS x, n.v AS v",
     None, [{"id": "a", "x": 1, "v": None}], with_node(BASE, "a", x=1, v=None), None),
    ("remove_and_set",
     "MATCH (n:Item) WHERE n.v = 1 REMOVE n.v SET n.x = 1 RETURN n.id AS id",
     None, [{"id": "a"}], with_node(BASE, "a", x=1, v=None), None),
    ("set_label_remove_label",
     "MATCH (n:Item {id: 'a'}) SET n:Hot REMOVE n:Item RETURN n.id AS id, labels(n) AS l",
     None, [{"id": "a", "l": ["Hot"]}], with_node(BASE, "a", l=["Hot"]), None),
    ("two_sets",
     "MATCH (n:Item) WHERE n.v = 1 SET n.v = 5 SET n.x = n.v RETURN n.id AS id, n.x AS x",
     None, [{"id": "a", "x": 5}], with_node(BASE, "a", v=5, x=5), None),
    ("set_return_node",
     "MATCH (n:Item {id: 'a'}) SET n.v = 7 RETURN n.id AS id, n.v AS v, labels(n) AS l",
     None, [{"id": "a", "v": 7, "l": ["Item"]}], with_node(BASE, "a", v=7), None),
    ("set_after_with",
     "MATCH (n:Item {id: 'a'}) WITH n SET n.v = 7 RETURN n.id AS id",
     None, [{"id": "a"}], with_node(BASE, "a", v=7), None),
    ("set_via_relationship",
     "MATCH (:Item {id: 'a'})-[:R]->(m:Item) SET m.x = 1 RETURN m.id AS id",
     None, [{"id": "b"}], with_node(BASE, "b", x=1), None),
    ("set_relationship_property",
     "MATCH (:Item {id: 'a'})-[r:R]->() SET r.w = 5 RETURN r.w AS w",
     None, [{"w": 5}], None,
     [{"s": "a", "t": "R", "w": 5, "d": "b"}] + BASE_EDGES[1:]),
    ("remove_property",
     "MATCH (n:Item {id: 'a'}) REMOVE n.v RETURN n.id AS id",
     None, [{"id": "a"}], with_node(BASE, "a", v=None), None),
    ("remove_where",
     "MATCH (n:Item) WHERE n.id = 'a' REMOVE n.v RETURN n.id AS id",
     None, [{"id": "a"}], with_node(BASE, "a", v=None), None),
    ("remove_label",
     "MATCH (n:Item {id: 'a'}) SET n:Hot REMOVE n:Hot RETURN n.id AS id",
     None, [{"id": "a"}], None, None),
    ("delete_count",
     "MATCH (n:Item {id: 'a'}) DETACH DELETE n RETURN count(*) AS c",
     None, [{"c": 1}],
     [n for n in BASE if not (n["id"] == "a" and n["l"] == ["Item"])],
     [BASE_EDGES[2]]),
    ("delete_where_count",
     "MATCH (n:Item) WHERE n.v >= 2 DETACH DELETE n RETURN count(*) AS c",
     None, [{"c": 2}],
     [n for n in BASE if n["id"] == "a"],
     [BASE_EDGES[1]]),
    ("delete_relationship",
     "MATCH (:Item {id: 'a'})-[r:R]->() DELETE r RETURN count(*) AS c",
     None, [{"c": 1}], None, BASE_EDGES[1:]),
    ("merge_on_match",
     "MERGE (n:Item {id: 'a'}) ON MATCH SET n.x = 5 RETURN n.id AS id, n.x AS x",
     None, [{"id": "a", "x": 5}], with_node(BASE, "a", x=5), None),
    ("merge_on_create",
     "MERGE (n:Item {id: 'd'}) ON CREATE SET n.v = 4 RETURN n.id AS id, n.v AS v",
     None, [{"id": "d", "v": 4}], BASE + [node("d", 4)], None),
    ("match_merge_relationship",
     "MATCH (a:Item {id: 'a'}), (c:Item {id: 'c'}) MERGE (a)-[r:S]->(c) RETURN a.id AS a, c.id AS c",
     None, [{"a": "a", "c": "c"}], None,
     BASE_EDGES[:2] + [{"s": "a", "t": "S", "w": None, "d": "c"}] + BASE_EDGES[2:]),
    ("match_create",
     "MATCH (n:Item {id: 'a'}) CREATE (n)-[:S]->(:New {id: 'n'}) RETURN n.id AS id",
     None, [{"id": "a"}], BASE + [node("n", None, label="New")],
     BASE_EDGES[:2] + [{"s": "a", "t": "S", "w": None, "d": "n"}] + BASE_EDGES[2:]),
    ("match_create_where",
     "MATCH (n:Item) WHERE n.id = 'a' CREATE (n)-[:S]->(:New {id: 'n'}) RETURN n.id AS id",
     None, [{"id": "a"}], BASE + [node("n", None, label="New")],
     BASE_EDGES[:2] + [{"s": "a", "t": "S", "w": None, "d": "n"}] + BASE_EDGES[2:]),
]


def canon(rows):
    return sorted(repr(sorted(r.items())) for r in rows)


@pytest.mark.parametrize("case", CASES, ids=[c[0] for c in CASES])
def test_write_return_scope(case):
    name, query, params, rows, nodes, edges = case
    g = Graph(":memory:", extension_path=get_extension_path())
    try:
        g.query(SETUP)
        got = g.query(query, params)
        assert canon(got) == canon(rows), f"returned {got}"
        want_nodes = sort_state(nodes if nodes is not None else BASE)
        assert sort_state(g.query(STATE)) == want_nodes, "node state"
        want_edges = edges if edges is not None else BASE_EDGES
        assert canon(g.query(EDGE_STATE)) == canon(want_edges), "edge state"
    finally:
        g.close()
