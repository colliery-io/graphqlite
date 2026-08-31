import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  pagerank,
  degreeCentrality,
  betweennessCentrality,
  closenessCentrality,
  eigenvectorCentrality,
  betweenness,
  closeness,
} from '../src/algorithms/centrality.ts';
import { graph } from '../src/graph/index.ts';
import { CypherResult, type CypherRow } from '../src/result.ts';
import { ValidationError } from '../src/errors.ts';

// --- Connection test double -------------------------------------------------
// The algorithm result comes back as a single row with an array column that
// extractAlgoArray() unwraps — `pageRank()` is one of ALGO_COLUMN_NAMES.
interface Call {
  query: string;
  params?: Record<string, unknown> | null;
}

function makeConn(columnName: string, rows: CypherRow[]) {
  const calls: Call[] = [];
  const conn = {
    cypher(query: string, params?: Record<string, unknown> | null): CypherResult {
      calls.push({ query, params });
      return new CypherResult([{ [columnName]: rows }], [columnName]);
    },
  };
  return { conn: conn as never, calls };
}

// --- No parameter binding — all string interpolation -----------------------
test('numeric args are interpolated into the query, never bound', () => {
  const { conn, calls } = makeConn('pageRank()', []);
  pagerank(conn, 0.9, 30);
  assert.equal(calls[0]?.query, 'RETURN pageRank(0.9, 30)');
  assert.equal(calls[0]?.params, undefined); // no binding
});

test('default args match Python (0.85 / 20 / 100)', () => {
  {
    const { conn, calls } = makeConn('pageRank()', []);
    pagerank(conn);
    assert.equal(calls[0]?.query, 'RETURN pageRank(0.85, 20)');
  }
  {
    const { conn, calls } = makeConn('eigenvectorCentrality()', []);
    eigenvectorCentrality(conn);
    assert.equal(calls[0]?.query, 'RETURN eigenvectorCentrality(100)');
  }
});

test('degreeCentrality / betweenness / closeness emit their bare RETURN queries', () => {
  {
    const { conn, calls } = makeConn('degreeCentrality()', []);
    degreeCentrality(conn);
    assert.equal(calls[0]?.query, 'RETURN degreeCentrality()');
  }
  {
    const { conn, calls } = makeConn('betweennessCentrality()', []);
    betweennessCentrality(conn);
    assert.equal(calls[0]?.query, 'RETURN betweennessCentrality()');
  }
  {
    const { conn, calls } = makeConn('closenessCentrality()', []);
    closenessCentrality(conn);
    assert.equal(calls[0]?.query, 'RETURN closenessCentrality()');
  }
});

// --- pagerank asymmetry (acceptance #2) ------------------------------------
test('pagerank drops rows where score is null; keeps the rest as CentralityScore', () => {
  const { conn } = makeConn('pageRank()', [
    { node_id: 'a', user_id: 'alice', score: 0.4 },
    { node_id: 'b', user_id: 'bob', score: null }, // dropped — pagerank checks score
    { node_id: null, user_id: 'x', score: 0.1 }, // dropped — nodeId null
  ]);
  assert.deepEqual(pagerank(conn), [{ nodeId: 'a', userId: 'alice', score: 0.4 }]);
});

test('score centralities keep score-null rows (nodeId-only filter); score → 0', () => {
  const { conn } = makeConn('betweennessCentrality()', [
    { node_id: 'a', user_id: 'alice', score: 1.5 },
    { node_id: 'b', user_id: 'bob', score: null }, // kept, score coerced to 0
    { node_id: null, user_id: 'x', score: 9 }, // dropped — nodeId null
  ]);
  assert.deepEqual(betweennessCentrality(conn), [
    { nodeId: 'a', userId: 'alice', score: 1.5 },
    { nodeId: 'b', userId: 'bob', score: 0 },
  ]);
});

// --- degreeCentrality keys (acceptance #3) ---------------------------------
test('degreeCentrality returns nodeId/userId/inDegree/outDegree/degree (safeInt)', () => {
  const { conn } = makeConn('degreeCentrality()', [
    { node_id: 'a', user_id: 'alice', in_degree: 2, out_degree: 3, degree: 5 },
  ]);
  assert.deepEqual(degreeCentrality(conn), [
    { nodeId: 'a', userId: 'alice', inDegree: 2, outDegree: 3, degree: 5 },
  ]);
});

// --- Finite-number validation (acceptance #6) ------------------------------
test('non-finite numeric args are rejected before interpolation', () => {
  const { conn } = makeConn('pageRank()', []);
  assert.throws(() => pagerank(conn, Number.NaN, 20), ValidationError);
  assert.throws(() => pagerank(conn, 0.85, Number.POSITIVE_INFINITY), ValidationError);
  assert.throws(() => pagerank(conn, 'x' as never, 20), ValidationError);
  assert.throws(() => eigenvectorCentrality(conn, Number.NaN), ValidationError);
});

// --- Aliases (acceptance #5) -----------------------------------------------
test('betweenness / closeness are aliases of their *Centrality functions', () => {
  assert.equal(betweenness, betweennessCentrality);
  assert.equal(closeness, closenessCentrality);
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

test('runs the 5 centrality algorithms end-to-end against the real extension', gate, () => {
  using g = graph(':memory:');
  g.upsertNode('a', {}, 'Person');
  g.upsertNode('b', {}, 'Person');
  g.upsertNode('c', {}, 'Person');
  g.upsertEdge('a', 'b', {}, 'KNOWS');
  g.upsertEdge('b', 'c', {}, 'KNOWS');
  g.upsertEdge('c', 'a', {}, 'KNOWS');

  const pr = g.pagerank();
  assert.ok(Array.isArray(pr) && pr.length > 0);
  assert.ok(typeof pr[0]?.score === 'number');

  const deg = g.degreeCentrality();
  assert.ok(deg.length > 0);
  assert.ok('inDegree' in deg[0]! && 'outDegree' in deg[0]! && 'degree' in deg[0]!);

  assert.ok(Array.isArray(g.betweennessCentrality()));
  assert.ok(Array.isArray(g.closenessCentrality()));
  assert.ok(Array.isArray(g.eigenvectorCentrality()));
  assert.deepEqual(g.betweenness(), g.betweennessCentrality());
});
