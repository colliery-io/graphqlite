"""Run cases in worker processes; check, group, shrink and report.

A case is identified by an id that rebuilds it exactly:
  r:<relation>:<seed>                      random mode
  c:<relation>:<row>:<rep>:<cover seed>    cover mode (row of the relation's
                                           covering array, repetition)
Workers are separate processes: an extension crash or a hang kills only the
worker; the case in flight is recorded as a crash or a timeout and a new
worker takes over.
"""

from __future__ import annotations

import json
import multiprocessing as mp
import os
import multiprocessing.connection
import random
import re
import time
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path

from .engine import Engine, Outcome, canon_row
from .gen import Graph, random_graph
from .relations import RELATIONS, Case, Variant
from .space import DOMAINS, Chooser, covering

CASE_TIMEOUT = 30.0


# ------------------------------------------------------------------ building

def case_seed(*parts) -> int:
    return random.Random(":".join(map(str, parts))).getrandbits(48)


_COVER_CACHE: dict = {}


def cover_rows(name: str, cover_seed: int, strength: int) -> list[dict]:
    key = (name, cover_seed, strength)
    if key not in _COVER_CACHE:
        _COVER_CACHE[key] = covering(RELATIONS[name][1], seed=case_seed(name, cover_seed),
                                     core_strength=strength)
    return _COVER_CACHE[key]


def build(case_id: str, strength: int = 3):
    """Rebuild (chooser, graph, case) from a case id."""
    parts = case_id.split(":")
    name = parts[1]
    if parts[0] == "r":
        ch = Chooser(random.Random(int(parts[2])))
    else:
        row, rep, cs = int(parts[2]), int(parts[3]), int(parts[4])
        ch = Chooser(random.Random(case_seed(name, row, rep, cs)), cover_rows(name, cs, strength)[row])
    graph = random_graph(ch)
    case = RELATIONS[name][0](ch)
    return ch, graph, case


# ------------------------------------------------------------------ checking

@dataclass
class Failure:
    kind: str                                        # error | mismatch
    outcomes: list[tuple[str, list[str], Outcome]]   # (label, queries, outcome)

    def detail(self, key) -> str:
        lines = []
        for label, qs, o in self.outcomes:
            for q in qs:
                lines.append(f"  [{label}] {q}")
            lines.append(f"      -> {key(o) if o.ok else 'ERROR: ' + o.error}")
        return "\n".join(lines)


def keyfn(compare: str):
    def key(o: Outcome) -> str:
        if not o.ok:
            return "ERROR: " + (o.error or "")
        rows = [canon_row(r) for r in o.rows]
        if compare == "bag":
            rows = sorted(rows)
        elif compare == "set":
            rows = sorted(set(rows))
        return json.dumps(rows)
    return key


def _run_variant(engine: Engine, conn, case: Case, var: Variant) -> Outcome:
    per_part = []
    for q in var.queries:
        o = None
        for _ in range(var.repeat):
            o = engine.run(conn, q)
            if not o.ok:
                return o
        per_part.append(o.rows)
    if var.mode == "write":
        o = engine.run(conn, case.readback)
        if not o.ok:
            return o
        rows = o.rows
    else:
        rows = var.combine(per_part) if var.combine else per_part[0]
    if var.post:
        rows = var.post(rows)
    if var.keymap:
        rows = [{var.keymap.get(k, k): x for k, x in r.items()} for r in rows]
    return Outcome(True, rows=rows)


def check(engine: Engine, case: Case, graph: Graph) -> Failure | None:
    setup = graph.create()
    results, conn = [], None
    for var in case.variants:
        if case.write or conn is None:
            conn = engine.fresh()
            if setup:
                s = engine.run(conn, setup)
                if not s.ok:
                    raise RuntimeError(f"graph setup failed: {s.error}\n{setup}")
        results.append((var.label, var.queries, _run_variant(engine, conn, case, var)))
    if any(not o.ok for _, _, o in results):
        return Failure("error", results)
    key = keyfn(case.compare)
    if len({key(o) for _, _, o in results}) > 1:
        return Failure("mismatch", results)
    return None


def signature(case: Case, f: Failure) -> str:
    if f.kind == "error":
        err = next(o.error for _, _, o in f.outcomes if not o.ok)
        err = re.sub(r"\d+", "N", err)
        err = re.sub(r", expecting .*", "", err)
        err = re.sub(r"_with_N\.\w+", "_with_N.VAR", err)
        err = re.sub(r"'[^']*'", "'…'", err)[:120]
        base_failed = not f.outcomes[0][2].ok
        return f"query error: {err}" if base_failed else f"{case.relation} error: {err}"
    return f"{case.relation} mismatch"


def known_task(known: list[dict], case: Case, f: Failure) -> str | None:
    queries = " ".join(q for _, qs, _ in f.outcomes for q in qs)
    errors = " ".join(o.error or "" for _, _, o in f.outcomes)
    outputs = json.dumps([o.rows for _, _, o in f.outcomes if o.ok])
    for k in known:
        if not re.search(k["relation"], case.relation):
            continue
        if k.get("kind") and k["kind"] != f.kind:
            continue
        if k.get("query") and not re.search(k["query"], queries):
            continue
        if k.get("error") and not re.search(k["error"], errors):
            continue
        if k.get("output") and not re.search(k["output"], outputs):
            continue
        return k["task"]
    return None


def shrink(engine: Engine, case: Case, graph: Graph, kind: str) -> Graph:
    def still(g: Graph) -> bool:
        f = check(engine, case, g)
        return f is not None and f.kind == kind
    changed = True
    while changed:
        changed = False
        for i in range(len(graph.edges)):
            g = graph.without_edge(i)
            if still(g):
                graph, changed = g, True
                break
        if changed:
            continue
        for n in list(graph.nodes):
            g = graph.without_node(n["id"])
            if still(g):
                graph, changed = g, True
                break
    return graph


# ------------------------------------------------------------------- workers

def run_one(engine: Engine, known: list, case_id: str, strength: int) -> dict:
    ch, graph, case = build(case_id, strength)
    f = check(engine, case, graph)
    res = {"id": case_id, "relation": case.relation, "dims": {k: str(x) for k, x in ch.used.items()}}
    if f is None:
        res["status"] = "ok"
    else:
        res.update(status="fail", kind=f.kind, signature=signature(case, f),
                   task=known_task(known, case, f))
    return res


def _worker(ext: str, known: list, strength: int, conn) -> None:
    """Run one case per message on `conn`; reply with its result. The
    parent knows which case a worker holds, so a crash loses nothing."""
    engine = Engine(Path(ext))
    while True:
        cid = conn.recv()
        if cid is None:
            return
        try:
            res = run_one(engine, known, cid, strength)
        except Exception as e:          # a harness fault, not an engine result
            res = {"id": cid, "status": "harness_error", "relation": cid.split(":")[1],
                   "error": repr(e)[:300]}
        conn.send(res)


def run_all(ext: Path, known: list, ids, jobs: int, strength: int,
            deadline: float | None = None, progress=None) -> list[dict]:
    """Run case ids (an iterator) on `jobs` workers until exhausted or the
    deadline passes."""
    ctx = mp.get_context("spawn")
    ids = iter(ids)
    out: list[dict] = []
    workers: dict[int, tuple] = {}          # wid -> (process, conn)
    holding: dict[int, tuple[str, float]] = {}

    def spawn(wid):
        parent, child = ctx.Pipe()
        p = ctx.Process(target=_worker, args=(str(ext), known, strength, child), daemon=True)
        p.start()
        child.close()
        workers[wid] = (p, parent)

    def give(wid) -> bool:
        if deadline and time.time() > deadline:
            return False
        cid = next(ids, None)
        if cid is None:
            return False
        workers[wid][1].send(cid)
        holding[wid] = (cid, time.time())
        return True

    def lost(wid, status):
        p, conn = workers[wid]
        cid, _ = holding.pop(wid)
        if p.is_alive():
            p.kill()
        p.join()
        conn.close()
        out.append({"id": cid, "status": status, "relation": cid.split(":")[1],
                    "exitcode": p.exitcode})
        spawn(wid)

    for w in range(jobs):
        spawn(w)
        give(w)
    while holding:
        conns = {workers[w][1]: w for w in holding}
        for conn in mp.connection.wait(list(conns), timeout=1.0):
            wid = conns[conn]
            try:
                res = conn.recv()
            except (EOFError, OSError):
                lost(wid, "crash")
                give(wid)
                continue
            holding.pop(wid)
            out.append(res)
            if progress and len(out) % 1000 == 0:
                progress(len(out))
            give(wid)
        now = time.time()
        for wid, (cid, t0) in list(holding.items()):
            if now - t0 > CASE_TIMEOUT:
                lost(wid, "timeout")
                give(wid)
            elif not workers[wid][0].is_alive() and not workers[wid][1].poll():
                lost(wid, "crash")
                give(wid)
    for p, conn in workers.values():
        try:
            conn.send(None)
        except OSError:
            pass
    for p, conn in workers.values():
        p.join(timeout=5)
    return out


# -------------------------------------------------------------------- report

def shared_dims(results: list[dict], failing: list[dict]) -> list[str]:
    """Dimension values that every failing case has, and that are rarer
    among all cases of the same relations."""
    if not failing:
        return []
    common = set(failing[0].get("dims", {}).items())
    for r in failing[1:]:
        common &= set(r.get("dims", {}).items())
    rels = {r["relation"] for r in failing}
    pool = [r for r in results if r.get("relation") in rels and "dims" in r]
    out = []
    for d, val in sorted(common):
        share = sum(1 for r in pool if r["dims"].get(d) == val) / max(1, len(pool))
        if share < 0.9:
            out.append(f"{d}={val}")
    return out


def report(results: list[dict], engine: Engine, strength: int, shrink_new: bool = True) -> dict:
    groups: dict[str, dict] = {}
    known = getattr(report, "known", [])
    for r in results:                    # a crash of an open bug is known
        if r["status"] in ("crash", "timeout"):
            try:
                _, _, case = build(r["id"], strength)
                f = Failure(r["status"], [(x.label, x.queries, Outcome(False, error=r["status"]))
                                          for x in case.variants])
                r["task"] = known_task(known, case, f)
            except Exception:
                r["task"] = None
    statuses = Counter(r["status"] for r in results)
    for r in results:
        if r["status"] == "fail":
            g = groups.setdefault(r["signature"], {"task": r["task"], "cases": []})
            g["cases"].append(r)
    for sig, g in groups.items():
        g["count"] = len(g["cases"])
        g["shared"] = shared_dims(results, g["cases"])
        first = g["cases"][0]["id"]
        g["first"] = first
        if g["task"] is None:
            ch, graph, case = build(first, strength)
            try:
                f = check(engine, case, graph)
                small = shrink(engine, case, graph, f.kind) if (shrink_new and f) else graph
                g["graph"] = small.create()
                g["detail"] = (check(engine, case, small) or f).detail(keyfn(case.compare))
            except Exception as e:
                g["detail"] = f"(could not rerun: {e!r})"
        g["ids"] = [c["id"] for c in g["cases"][:20]]
        del g["cases"]
    used: dict[str, set] = defaultdict(set)
    per_rel: dict[str, dict] = defaultdict(lambda: defaultdict(set))
    for r in results:
        for d, val in r.get("dims", {}).items():
            used[d].add(val)
            per_rel[r["id"].split(":")[1]][d].add(val)
    gaps = {}
    for name, seen in per_rel.items():
        missing = [f"{d}={x}" for d in RELATIONS[name][1] for x in DOMAINS[d]
                   if str(x) not in seen.get(d, set())]
        if missing:
            gaps[name] = missing
    return {"statuses": dict(statuses), "groups": groups,
            "crashes": [r for r in results if r["status"] in ("crash", "timeout", "harness_error")
                        and not r.get("task")],
            "known_crashes": [r for r in results if r.get("task") and r["status"] in ("crash", "timeout")],
            "per_relation": dict(Counter(r.get("relation", "?").split("/")[0] for r in results)),
            "dims_used": {d: sorted(vs) for d, vs in used.items()}, "gaps": gaps}


def print_report(rep: dict, header: str) -> int:
    groups = rep["groups"]
    new = {s: g for s, g in groups.items() if not g["task"]}
    old = {s: g for s, g in groups.items() if g["task"]}
    st = rep["statuses"]
    print(f"{header}: {sum(st.values())} cases, {st.get('fail', 0)} failed, "
          f"{len(new)} new signature(s), {len(old)} known, "
          f"{st.get('crash', 0)} crash(es), {st.get('timeout', 0)} timeout(s), "
          f"{st.get('harness_error', 0)} harness error(s)")
    by_task: dict[str, int] = Counter()
    for g in old.values():
        by_task[g["task"]] += g["count"]
    for c in rep.get("known_crashes", []):
        by_task[c["task"]] += 1
    for task, n in sorted(by_task.items(), key=lambda x: -x[1]):
        print(f"  known {task}: {n} case(s)")
    for c in rep["crashes"][:10]:
        print(f"\n{c['status'].upper()}: {c['id']} {c.get('error', '')} (exit {c.get('exitcode')})")
    for sig, g in sorted(new.items(), key=lambda x: -x[1]["count"]):
        print(f"\nNEW: {sig} ({g['count']}x, replay --replay {g['first']})")
        if g["shared"]:
            print(f"  every failing case has: {', '.join(g['shared'])}")
        print(f"  graph: {g.get('graph')}")
        print(g.get("detail", ""))
    if rep.get("gaps"):
        n = sum(len(x) for x in rep["gaps"].values())
        print(f"\ngaps: {n} dimension value(s) never exercised (see 'gaps' in the results file)")
    bad = len(rep["crashes"])
    return 1 if new or bad else 0


def default_jobs() -> int:
    return max(1, (os.cpu_count() or 2) - 1)
