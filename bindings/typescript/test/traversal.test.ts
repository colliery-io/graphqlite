import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  bfs,
  dfs,
  breadthFirstSearch,
  depthFirstSearch,
} from '../src/algorithms/traversal.ts';
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

// The core wraps algorithm output in a single `column_0` array column, which
// extractAlgoArray unwraps. Build that shape.
function wrapped(...r: CypherRow[]): CypherResult {
  return new CypherResult([{ column_0: r }], ['column_0']);
}

// --- maxDepth boundary: only < 0 is unlimited (asymmetry) -------------------
test('bfs: default maxDepth (-1) is unlimited → 1-arg query, start escaped', () => {
  const { conn, calls } = makeConn(
    wrapped({ user_id: 'a', depth: 0, order: 0 }, { user_id: 'b', depth: 1, order: 1 }),
  );
  const out = bfs(conn, 'a');
  assert.equal(calls[0]?.query, "RETURN bfs('a')");
  assert.deepEqual(out, [
    { user_id: 'a', depth: 0, order: 0 },
    { user_id: 'b', depth: 1, order: 1 },
  ]);
});

test('bfs: maxDepth === 0 is NOT unlimited → 2-arg query bfs(x, 0)', () => {
  const { conn, calls } = makeConn(new CypherResult([], []));
  bfs(conn, 'a', { maxDepth: 0 });
  assert.equal(calls[0]?.query, "RETURN bfs('a', 0)");
});

test('bfs: positive maxDepth interpolated raw', () => {
  const { conn, calls } = makeConn(new CypherResult([], []));
  bfs(conn, 'a', { maxDepth: 2 });
  assert.equal(calls[0]?.query, "RETURN bfs('a', 2)");
});

test('bfs: a quote in startId is escaped', () => {
  const { conn, calls } = makeConn(new CypherResult([], []));
  bfs(conn, "a'x");
  assert.equal(calls[0]?.query, "RETURN bfs('a\\'x')");
});

// --- extractAlgoArray + parseTraversalResult null filtering -----------------
test('bfs: unwraps column_0, parses rows, drops null user_id, defaults depth/order', () => {
  const { conn } = makeConn(
    wrapped(
      { user_id: 'a', depth: 0, order: 0 },
      { user_id: null, depth: 5, order: 5 }, // dropped (parseTraversalResult → null)
      { user_id: 'c', depth: null, order: null }, // depth/order → 0
    ),
  );
  const out = bfs(conn, 'a');
  assert.deepEqual(out, [
    { user_id: 'a', depth: 0, order: 0 },
    { user_id: 'c', depth: 0, order: 0 },
  ]);
});

// --- dfs mirrors bfs --------------------------------------------------------
test('dfs: same query shape with the dfs function name', () => {
  const { conn, calls } = makeConn(
    wrapped({ user_id: 'a', depth: 0, order: 0 }),
  );
  const out = dfs(conn, 'a', { maxDepth: 3 });
  assert.equal(calls[0]?.query, "RETURN dfs('a', 3)");
  assert.deepEqual(out, [{ user_id: 'a', depth: 0, order: 0 }]);
});

test('dfs: default unlimited → 1-arg', () => {
  const { conn, calls } = makeConn(new CypherResult([], []));
  dfs(conn, 'a');
  assert.equal(calls[0]?.query, "RETURN dfs('a')");
});

// --- Aliases ----------------------------------------------------------------
test('breadthFirstSearch / depthFirstSearch are aliases', () => {
  assert.equal(breadthFirstSearch, bfs);
  assert.equal(depthFirstSearch, dfs);
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

test('bfs/dfs end-to-end against the real extension', gate, () => {
  using g = graph(':memory:');
  for (const id of ['a', 'b', 'c']) g.upsertNode(id, {}, 'P');
  g.upsertEdge('a', 'b', {}, 'E');
  g.upsertEdge('b', 'c', {}, 'E');

  const b = g.bfs('a');
  assert.ok(b.length >= 1);
  assert.equal(b[0]?.user_id, 'a');
  assert.ok(b.every((r) => r.user_id != null));

  // maxDepth 0 stays at the start node only (boundary, not unlimited).
  const b0 = g.bfs('a', { maxDepth: 0 });
  assert.ok(b0.length >= 1);

  const d = g.dfs('a');
  assert.ok(d.length >= 1);
  assert.equal(d[0]?.user_id, 'a');
});
