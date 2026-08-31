import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  shortestPath,
  astar,
  allPairsShortestPath,
  dijkstra,
  aStar,
  apsp,
} from '../src/algorithms/paths.ts';
import { graph } from '../src/graph/index.ts';
import { CypherResult, type CypherRow } from '../src/result.ts';
import { ValidationError } from '../src/errors.ts';

// --- Connection test double -------------------------------------------------
interface Call {
  query: string;
  params?: Record<string, unknown> | null;
}

function makeConn(...responses: CypherResult[]) {
  const calls: Call[] = [];
  const queue = [...responses];
  const conn = {
    cypher(query: string, params?: Record<string, unknown> | null): CypherResult {
      calls.push({ query, params });
      return queue.shift() ?? new CypherResult([], []);
    },
  };
  return { conn: conn as never, calls };
}

function rows(...r: CypherRow[]): CypherResult {
  return new CypherResult(r, r[0] ? Object.keys(r[0]) : []);
}

// --- shortestPath: double quotes, escape, column_0 unwrap (asymmetries 1,2) -
test('shortestPath: double quotes, escapes src/tgt/weight, unwraps column_0', () => {
  const { conn, calls } = makeConn(
    rows({ column_0: { path: ['a', 'b'], distance: 2, found: true } }),
  );
  const out = shortestPath(conn, 'a', 'b', { weightProp: 'w' });
  assert.equal(calls[0]?.query, 'RETURN dijkstra("a", "b", "w")');
  assert.deepEqual(out, { path: ['a', 'b'], distance: 2, found: true });
});

test('shortestPath: no weight → 2-arg; a quote in an id is escaped', () => {
  const { conn, calls } = makeConn(new CypherResult([], []));
  const out = shortestPath(conn, 'a"x', 'b');
  assert.equal(calls[0]?.query, 'RETURN dijkstra("a\\"x", "b")');
  assert.deepEqual(out, { path: [], distance: null, found: false }); // empty default
});

test('shortestPath: direct-access branch when there is no column_0', () => {
  const { conn } = makeConn(rows({ path: ['a'], distance: 0, found: true }));
  assert.deepEqual(shortestPath(conn, 'a', 'a'), { path: ['a'], distance: 0, found: true });
});

// --- astar: single quotes, unwraps column_0, assertIdentifier (asym 1,3; #64) -
test('astar: single quotes; unwraps column_0 (returns the real path)', () => {
  const { conn, calls } = makeConn(
    // Same column_0-wrapped shape the real core returns — astar now unwraps it.
    rows({ column_0: { path: ['a', 'b'], distance: 2, found: true, nodes_explored: 3 } }),
  );
  const out = astar(conn, 'a', 'b');
  assert.equal(calls[0]?.query, "RETURN astar('a', 'b')");
  assert.deepEqual(out, { path: ['a', 'b'], distance: 2, found: true, nodesExplored: 3 });
});

test('astar: reads fields directly from result[0] (unwrapped shape)', () => {
  const { conn } = makeConn(rows({ path: ['a', 'b'], distance: 5, found: true, nodes_explored: 7 }));
  assert.deepEqual(astar(conn, 'a', 'b'), {
    path: ['a', 'b'],
    distance: 5,
    found: true,
    nodesExplored: 7,
  });
});

test('astar: lat/lon props are interpolated and validated as identifiers', () => {
  {
    const { conn, calls } = makeConn(new CypherResult([], []));
    astar(conn, 'a', 'b', { latProp: 'lat', lonProp: 'lon' });
    assert.equal(calls[0]?.query, "RETURN astar('a', 'b', 'lat', 'lon')");
  }
  {
    const { conn } = makeConn(new CypherResult([], []));
    assert.throws(() => astar(conn, 'a', 'b', { latProp: 'bad prop', lonProp: 'lon' }), ValidationError);
  }
});

// --- apsp: toList → extractAlgoArray, not-null filter (asymmetry 4) ---------
test('apsp: RETURN apsp(), extractAlgoArray unwraps column_0, filters null pairs', () => {
  const { conn, calls } = makeConn(
    new CypherResult(
      [
        {
          'column_0': [
            { source: 'a', target: 'b', distance: 1 },
            { source: 'a', target: null, distance: 9 }, // dropped
            { source: 'b', target: 'c', distance: 2.5 },
          ],
        },
      ],
      ['column_0'],
    ),
  );
  const out = allPairsShortestPath(conn);
  assert.equal(calls[0]?.query, 'RETURN apsp()');
  assert.deepEqual(out, [
    { source: 'a', target: 'b', distance: 1 },
    { source: 'b', target: 'c', distance: 2.5 },
  ]);
});

// --- Aliases ---------------------------------------------------------------
test('dijkstra / aStar / apsp are aliases of their long-named functions', () => {
  assert.equal(dijkstra, shortestPath);
  assert.equal(aStar, astar);
  assert.equal(apsp, allPairsShortestPath);
});

// --- Integration (gated on a built extension) ------------------------------
function extensionAvailable(): boolean {
  try {
    graph(':memory:').close();
    return true;
  } catch {
    return false;
  }
}
const gate = { skip: !extensionAvailable() };

test('path algorithms end-to-end against the real extension', gate, () => {
  using g = graph(':memory:');
  for (const id of ['a', 'b', 'c']) g.upsertNode(id, {}, 'P');
  g.upsertEdge('a', 'b', { w: 1 }, 'E');
  g.upsertEdge('b', 'c', { w: 2 }, 'E');

  // shortestPath unwraps column_0 → real path returned.
  const sp = g.shortestPath('a', 'c', { weightProp: 'w' });
  assert.equal(sp.found, true);
  assert.deepEqual(sp.path, ['a', 'b', 'c']);

  // astar unwraps column_0 → real path returned (see #64 fix note above).
  const as = g.astar('a', 'c');
  assert.equal(as.found, true);
  assert.deepEqual(as.path, ['a', 'b', 'c']);
  assert.equal(as.distance, 2);
  assert.ok(as.nodesExplored >= 1);

  // apsp goes through extractAlgoArray → reachable pairs.
  const all = g.allPairsShortestPath();
  assert.ok(all.length >= 2);
  assert.ok(all.every((p) => p.source != null && p.target != null));
});
