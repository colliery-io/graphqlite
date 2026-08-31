import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  nodeSimilarity,
  knn,
  triangleCount,
  triangles,
} from '../src/algorithms/similarity.ts';
import { graph } from '../src/graph/index.ts';
import { CypherResult, type CypherRow } from '../src/result.ts';

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

// --- nodeSimilarity: 4-way branch (priority order) --------------------------
test('nodeSimilarity: branch 1 — node1 && node2 → pair query, escaped', () => {
  const { conn, calls } = makeConn(
    rows({ node1: 'a', node2: 'b', similarity: 0.5 }),
  );
  const out = nodeSimilarity(conn, { node1: 'a', node2: "b'x", threshold: 5, topK: 3 });
  assert.equal(calls[0]?.query, "RETURN nodeSimilarity('a', 'b\\'x')"); // wins over threshold/topK
  assert.deepEqual(out, [{ node1: 'a', node2: 'b', similarity: 0.5 }]);
});

test('nodeSimilarity: branch 2 — threshold>0 && topK>0', () => {
  const { conn, calls } = makeConn(new CypherResult([], []));
  nodeSimilarity(conn, { threshold: 0.3, topK: 5 });
  assert.equal(calls[0]?.query, 'RETURN nodeSimilarity(0.3, 5)');
});

test('nodeSimilarity: branch 3 — threshold>0 only', () => {
  const { conn, calls } = makeConn(new CypherResult([], []));
  nodeSimilarity(conn, { threshold: 0.3 });
  assert.equal(calls[0]?.query, 'RETURN nodeSimilarity(0.3)');
});

test('nodeSimilarity: branch 2 — topK ONLY (threshold 0) is honored → nodeSimilarity(0, topK)', () => {
  const { conn, calls } = makeConn(new CypherResult([], []));
  nodeSimilarity(conn, { topK: 5 }); // threshold defaults to 0 → still emits topK (#71)
  assert.equal(calls[0]?.query, 'RETURN nodeSimilarity(0, 5)');
});

test('nodeSimilarity: no options → nodeSimilarity()', () => {
  const { conn, calls } = makeConn(new CypherResult([], []));
  nodeSimilarity(conn);
  assert.equal(calls[0]?.query, 'RETURN nodeSimilarity()');
});

// --- The key asymmetry: NO extractAlgoArray (direct iteration) --------------
test('nodeSimilarity: does NOT unwrap column_0 → empty against wrapped core shape', () => {
  const { conn } = makeConn(
    // The real core wraps like this; extractAlgoArray would unwrap it, but this
    // module iterates directly, so no node1/node2 keys are seen → empty.
    new CypherResult(
      [{ column_0: [{ node1: 'a', node2: 'b', similarity: 0.9 }] }],
      ['column_0'],
    ),
  );
  assert.deepEqual(nodeSimilarity(conn), []);
});

test('nodeSimilarity: drops rows missing either node; similarity via safeFloat', () => {
  const { conn } = makeConn(
    rows(
      { node1: 'a', node2: 'b', similarity: '0.5' }, // string coerced
      { node1: 'a', node2: null, similarity: 0.9 }, // dropped
      { node1: null, node2: 'c', similarity: 0.9 }, // dropped
    ),
  );
  assert.deepEqual(nodeSimilarity(conn), [{ node1: 'a', node2: 'b', similarity: 0.5 }]);
});

// --- knn --------------------------------------------------------------------
test('knn: default k=10, escaped id, keys neighbor/similarity/rank', () => {
  const { conn, calls } = makeConn(
    rows(
      { neighbor: 'b', similarity: 0.7, rank: 1 },
      { neighbor: null, similarity: 0.1, rank: 2 }, // dropped
    ),
  );
  const out = knn(conn, "a'x");
  assert.equal(calls[0]?.query, "RETURN knn('a\\'x', 10)");
  assert.deepEqual(out, [{ neighbor: 'b', similarity: 0.7, rank: 1 }]);
});

test('knn: custom k', () => {
  const { conn, calls } = makeConn(new CypherResult([], []));
  knn(conn, 'a', { k: 3 });
  assert.equal(calls[0]?.query, "RETURN knn('a', 3)");
});

// --- triangleCount: camelCase keys, alias -----------------------------------
test('triangleCount: RETURN triangleCount(), camelCase keys, drops null node_id', () => {
  const { conn, calls } = makeConn(
    rows(
      { node_id: 1, user_id: 'a', triangles: 2, clustering_coefficient: '0.33' },
      { node_id: null, user_id: 'z', triangles: 9, clustering_coefficient: 1 }, // dropped
    ),
  );
  const out = triangleCount(conn);
  assert.equal(calls[0]?.query, 'RETURN triangleCount()');
  assert.deepEqual(out, [
    { nodeId: '1', userId: 'a', triangles: 2, clusteringCoefficient: 0.33 },
  ]);
});

test('triangles is an alias of triangleCount', () => {
  assert.equal(triangles, triangleCount);
});

// --- Integration (gated on a built extension) -------------------------------
function extensionAvailable(): boolean {
  try {
    graph(':memory:').close();
    return true;
  } catch {
    return false;
  }
}
const gate = { skip: !extensionAvailable() };

test('similarity algorithms end-to-end against the real extension', gate, () => {
  using g = graph(':memory:');
  for (const id of ['a', 'b', 'c']) g.upsertNode(id, {}, 'P');
  g.upsertEdge('a', 'b', {}, 'E');
  g.upsertEdge('b', 'c', {}, 'E');
  g.upsertEdge('a', 'c', {}, 'E');

  // Direct iteration (no extractAlgoArray): against the column_0-wrapped core
  // these return [] — the documented Python inconsistency, reproduced.
  assert.deepEqual(g.nodeSimilarity(), []);
  assert.deepEqual(g.knn('a'), []);
  assert.deepEqual(g.triangleCount(), []);
  assert.equal(typeof g.triangles, 'function');
});
