import { test } from 'node:test';
import assert from 'node:assert/strict';
import { communityDetection, louvain } from '../src/algorithms/community.ts';
import { graph } from '../src/graph/index.ts';
import { CypherResult, type CypherRow } from '../src/result.ts';
import { ValidationError, UnsupportedOperationError } from '../src/errors.ts';

// --- Connection test double -------------------------------------------------
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

// --- String interpolation, default args ------------------------------------
test('numeric args are interpolated (no binding); defaults match Python', () => {
  {
    const { conn, calls } = makeConn('labelPropagation()', []);
    communityDetection(conn);
    assert.equal(calls[0]?.query, 'RETURN labelPropagation(10)');
    assert.equal(calls[0]?.params, undefined);
  }
  {
    const { conn, calls } = makeConn('louvain()', []);
    louvain(conn);
    assert.equal(calls[0]?.query, 'RETURN louvain(1)');
  }
  {
    const { conn, calls } = makeConn('labelPropagation()', []);
    communityDetection(conn, 25);
    assert.equal(calls[0]?.query, 'RETURN labelPropagation(25)');
  }
});

// --- Asymmetric filter (acceptance #1) -------------------------------------
test('communityDetection drops rows with null community; keeps nodeId+community', () => {
  const { conn } = makeConn('labelPropagation()', [
    { node_id: 'a', user_id: 'alice', community: 1 },
    { node_id: 'b', user_id: 'bob', community: null }, // dropped — community null
    { node_id: null, user_id: 'x', community: 2 }, // dropped — nodeId null
  ]);
  assert.deepEqual(communityDetection(conn), [{ nodeId: 'a', userId: 'alice', community: 1 }]);
});

test('louvain keeps null-community rows (nodeId-only filter); community → 0', () => {
  const { conn } = makeConn('louvain()', [
    { node_id: 'a', user_id: 'alice', community: 3 },
    { node_id: 'b', user_id: 'bob', community: null }, // kept, coerced to 0
    { node_id: null, user_id: 'x', community: 9 }, // dropped — nodeId null
  ]);
  assert.deepEqual(louvain(conn), [
    { nodeId: 'a', userId: 'alice', community: 3 },
    { nodeId: 'b', userId: 'bob', community: 0 },
  ]);
});

// --- Return keys (acceptance #2) -------------------------------------------
test('return keys are nodeId / userId / community (safeInt)', () => {
  const { conn } = makeConn('labelPropagation()', [
    { node_id: 42, user_id: 'alice', community: '7' },
  ]);
  assert.deepEqual(communityDetection(conn), [{ nodeId: '42', userId: 'alice', community: 7 }]);
});

// --- Finite validation -----------------------------------------------------
test('non-finite numeric args are rejected before interpolation', () => {
  const { conn } = makeConn('labelPropagation()', []);
  assert.throws(() => communityDetection(conn, Number.NaN), ValidationError);
  assert.throws(() => louvain(conn, Number.POSITIVE_INFINITY), ValidationError);
  assert.throws(() => louvain(conn, 'x' as never), ValidationError);
});

// --- leidenCommunities stub (acceptance #3) --------------------------------
test('leidenCommunities throws UnsupportedOperationError (Python-only dependency)', () => {
  // No extension needed — the stub throws before touching the connection.
  let g;
  try {
    g = graph(':memory:');
  } catch {
    return; // no build — the stub still can't be reached without a Graph instance
  }
  try {
    assert.throws(
      () => g.leidenCommunities(),
      (err: unknown) =>
        err instanceof UnsupportedOperationError &&
        err.code === 'UNSUPPORTED_OPERATION' &&
        /graspologic/.test(err.message),
    );
  } finally {
    g.close();
  }
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

test('runs community detection end-to-end against the real extension', gate, () => {
  using g = graph(':memory:');
  for (const id of ['a', 'b', 'c', 'd']) g.upsertNode(id, {}, 'Person');
  g.upsertEdge('a', 'b', {}, 'KNOWS');
  g.upsertEdge('b', 'a', {}, 'KNOWS');
  g.upsertEdge('c', 'd', {}, 'KNOWS');
  g.upsertEdge('d', 'c', {}, 'KNOWS');

  const cd = g.communityDetection();
  assert.ok(Array.isArray(cd) && cd.length > 0);
  assert.ok(typeof cd[0]?.community === 'number' && typeof cd[0]?.nodeId === 'string');

  const lv = g.louvain();
  assert.ok(Array.isArray(lv));
});
