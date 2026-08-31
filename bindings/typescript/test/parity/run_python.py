#!/usr/bin/env python3
"""Python parity runner for #30 [V-01] Python<->TS 대조 하네스.

Loads scenarios.json, executes every scenario against a fresh in-memory graph
built from the shared `fixture`, and emits one JSON document on stdout describing
each step's outcome. Two modes:

  results  (default): record each step's return value (or the error it raised).
  cypher            : record the sequence of Cypher query strings (+ params) the
                      binding emitted to the core while running the step. This is
                      captured by wrapping Connection.cypher; it reflects exactly
                      what each method hands to the C extension. Steps that raise
                      before hitting the core (e.g. TS identifier validation, or a
                      Python-only ImportError) emit an empty/partial call list.

This runner NEVER touches the C core; it only drives the public Python binding.
The extension is auto-detected by the binding (GRAPHQLITE_EXTENSION_PATH is
honored — parity-check.sh sets it). Usage:

  python run_python.py [--mode results|cypher] [--scenarios PATH]
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

import graphqlite

_WS = re.compile(r"\s+")


def _norm_ws(s: str) -> str:
    return _WS.sub(" ", s).strip()


# ── Dispatch: camelCase scenario method -> Python binding call ─────────────────
# Each adapter takes (graph, args_list) and performs the positional/keyword
# translation the Python API needs. Missing trailing args fall back to defaults.
def _arg(args, i, default=None):
    return args[i] if len(args) > i else default


def _bulk_prop_table(g, external_id, key):
    """Report which typed table (int/real/text/bool) a node property landed in.

    This is what surfaces the intentional 1.0->int divergence: the bulk insert
    RETURN value is identical across bindings, but the storage table differs
    (Python float 1.0 -> node_props_real, JS 1.0 -> node_props_int). Raw SQL via
    the driver escape hatch; emits no cypher, so cypher-mode stays empty.
    """
    conn = g.connection.sqlite_connection
    row = conn.execute("SELECT id FROM property_keys WHERE key = 'id'").fetchone()
    if row is None:
        return None
    id_key_id = row[0]
    row = conn.execute(
        "SELECT node_id FROM node_props_text WHERE key_id = ? AND value = ?",
        (id_key_id, external_id),
    ).fetchone()
    if row is None:
        return None
    node_id = row[0]
    row = conn.execute("SELECT id FROM property_keys WHERE key = ?", (key,)).fetchone()
    if row is None:
        return None
    key_id = row[0]
    for suffix in ("int", "real", "text", "bool"):
        r = conn.execute(
            f"SELECT 1 FROM node_props_{suffix} WHERE node_id = ? AND key_id = ?",
            (node_id, key_id),
        ).fetchone()
        if r is not None:
            return suffix
    return None


def _graph_bulk(g, a):
    r = g.insert_graph_bulk(a[0], a[1])
    return {
        "nodesInserted": r.nodes_inserted,
        "edgesInserted": r.edges_inserted,
        "idMap": r.id_map,
    }


DISPATCH = {
    # nodes
    "upsertNode": lambda g, a: g.upsert_node(a[0], a[1], *( [a[2]] if len(a) > 2 else [] )),
    "getNode": lambda g, a: g.get_node(a[0]),
    "hasNode": lambda g, a: g.has_node(a[0]),
    "deleteNode": lambda g, a: g.delete_node(a[0]),
    "getAllNodes": lambda g, a: g.get_all_nodes(*( [a[0]] if len(a) > 0 else [] )),
    # edges
    "upsertEdge": lambda g, a: g.upsert_edge(a[0], a[1], a[2], *a[3:]),
    "getEdge": lambda g, a: g.get_edge(a[0], a[1], _arg(a, 2)),
    "hasEdge": lambda g, a: g.has_edge(a[0], a[1], _arg(a, 2)),
    "deleteEdge": lambda g, a: g.delete_edge(a[0], a[1], _arg(a, 2)),
    "getAllEdges": lambda g, a: g.get_all_edges(),
    # queries
    "nodeDegree": lambda g, a: g.node_degree(a[0]),
    "getNeighbors": lambda g, a: g.get_neighbors(a[0]),
    "getNodeEdges": lambda g, a: g.get_node_edges(a[0]),
    "getEdgesFrom": lambda g, a: g.get_edges_from(a[0]),
    "getEdgesTo": lambda g, a: g.get_edges_to(a[0]),
    "getEdgesByType": lambda g, a: g.get_edges_by_type(a[0], a[1]),
    "stats": lambda g, a: g.stats(),
    "query": lambda g, a: g.query(a[0], _arg(a, 1)),
    # centrality
    "pagerank": lambda g, a: g.pagerank(*a),
    "degreeCentrality": lambda g, a: g.degree_centrality(),
    "betweennessCentrality": lambda g, a: g.betweenness_centrality(),
    "closenessCentrality": lambda g, a: g.closeness_centrality(),
    "eigenvectorCentrality": lambda g, a: g.eigenvector_centrality(*a),
    # community
    "communityDetection": lambda g, a: g.community_detection(*a),
    "louvain": lambda g, a: g.louvain(*a),
    "leidenCommunities": lambda g, a: g.leiden_communities(*a),
    # components
    "weaklyConnectedComponents": lambda g, a: g.weakly_connected_components(),
    "stronglyConnectedComponents": lambda g, a: g.strongly_connected_components(),
    # paths
    "shortestPath": lambda g, a: g.shortest_path(a[0], a[1], _arg(a, 2)),
    "astar": lambda g, a: g.astar(a[0], a[1], _arg(a, 2), _arg(a, 3)),
    "allPairsShortestPath": lambda g, a: g.all_pairs_shortest_path(),
    # traversal
    "bfs": lambda g, a: g.bfs(a[0], *( [a[1]] if len(a) > 1 else [] )),
    "dfs": lambda g, a: g.dfs(a[0], *( [a[1]] if len(a) > 1 else [] )),
    # similarity
    "nodeSimilarity": lambda g, a: g.node_similarity(*a),
    "knn": lambda g, a: g.knn(a[0], *( [a[1]] if len(a) > 1 else [] )),
    "triangleCount": lambda g, a: g.triangle_count(),
    # bulk — raw SQL, bypasses Cypher (cypher-mode captures are empty both sides).
    "insertNodesBulk": lambda g, a: g.insert_nodes_bulk(a[0]),
    "insertEdgesBulk": lambda g, a: g.insert_edges_bulk(a[0], _arg(a, 1)),
    "insertGraphBulk": lambda g, a: _graph_bulk(g, a),
    "resolveNodeIds": lambda g, a: g.resolve_node_ids(a[0]),
    "bulkPropTable": lambda g, a: _bulk_prop_table(g, a[0], a[1]),
    # export (Python-only; allowlisted divergence)
    "toRustworkx": lambda g, a: g.to_rustworkx(),
}


def _json_safe(value):
    """Coerce return values into plain JSON-serializable structures.

    Tuples -> lists; everything else the binding returns is already dict / list /
    scalar / None. Non-serializable objects (e.g. a rustworkx graph) fall back to
    their repr so the harness never crashes on them.
    """
    if isinstance(value, tuple):
        return [_json_safe(v) for v in value]
    if isinstance(value, list):
        return [_json_safe(v) for v in value]
    if isinstance(value, dict):
        return {k: _json_safe(v) for k, v in value.items()}
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    return {"__repr__": repr(value)}


def _install_cypher_spy(graph, sink):
    """Wrap Connection.cypher so we capture what the binding sends to the core."""
    conn = graph.connection
    original = conn.cypher

    def spy(query, params=None):
        sink.append({"query": _norm_ws(query), "params": params})
        return original(query, params)

    conn.cypher = spy  # shadow the bound method on the instance


def build_graph(fixture, mode, call_sink):
    g = graphqlite.graph(":memory:")
    if mode == "cypher":
        _install_cypher_spy(g, call_sink)
    for step in fixture:
        DISPATCH[step["method"]](g, step.get("args", []))
    # Discard fixture-phase captures; only per-step captures are compared.
    call_sink.clear()
    return g


def run_step(graph, step, mode, call_sink):
    call_sink.clear()
    record = {"id": step["id"], "method": step["method"], "args": step.get("args", [])}
    try:
        value = DISPATCH[step["method"]](graph, step.get("args", []))
        record["status"] = "ok"
        if mode == "results":
            record["value"] = _json_safe(value)
    except Exception as exc:  # noqa: BLE001 — divergence detection needs all errors
        record["status"] = "error"
        record["error"] = {"type": type(exc).__name__, "message": str(exc)}
    if mode == "cypher":
        record["cypher"] = list(call_sink)
    return record


def main() -> int:
    parser = argparse.ArgumentParser(description="Python parity runner")
    parser.add_argument("--mode", choices=["results", "cypher"], default="results")
    parser.add_argument(
        "--scenarios",
        default=str(Path(__file__).with_name("scenarios.json")),
    )
    args = parser.parse_args()

    spec = json.loads(Path(args.scenarios).read_text())
    fixture = spec.get("fixture", [])

    out = {"binding": "python", "mode": args.mode, "scenarios": []}
    for scenario in spec["scenarios"]:
        call_sink: list = []
        g = build_graph(fixture, args.mode, call_sink)
        try:
            steps = [run_step(g, s, args.mode, call_sink) for s in scenario["steps"]]
        finally:
            g.close()
        out["scenarios"].append({"id": scenario["id"], "group": scenario.get("group"), "steps": steps})

    json.dump(out, sys.stdout)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
