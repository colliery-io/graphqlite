import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  nodeDegree,
  getNeighbors,
  getNodeEdges,
  getEdgesFrom,
  getEdgesTo,
  getEdgesByType,
  stats,
  query,
} from '../src/graph/queries.ts';
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

// --- nodeDegree (acceptance #5 binding) ------------------------------------
test('nodeDegree: Cypher/params parity; empty → 0; value → number', () => {
  {
    const { conn, calls } = makeConn(rows({ degree: 3 }));
    assert.equal(nodeDegree(conn, 'alice'), 3);
    assert.deepEqual(calls[0], {
      query: 'MATCH (n {id: $id})-[r]-() RETURN count(r) AS degree',
      params: { id: 'alice' },
    });
  }
  assert.equal(nodeDegree(makeConn(new CypherResult([], [])).conn, 'x'), 0);
  assert.equal(nodeDegree(makeConn(rows({ degree: 0 })).conn, 'x'), 0);
});

// --- getNeighbors ----------------------------------------------------------
test('getNeighbors: DISTINCT m query; keeps only truthy m', () => {
  const { conn, calls } = makeConn(
    new CypherResult([{ m: { id: 1 } }, { m: null }, { m: { id: 2 } }], ['m']),
  );
  assert.deepEqual(getNeighbors(conn, 'alice'), [{ id: 1 }, { id: 2 }]);
  assert.equal(calls[0]?.query, 'MATCH (n {id: $id})-[]-(m) RETURN DISTINCT m');
});

// --- getNodeEdges tuple shape (acceptance #1) ------------------------------
test('getNodeEdges: returns [source, target, props] tuples; missing r → {}', () => {
  const { conn, calls } = makeConn(
    new CypherResult(
      [
        { source: 'alice', target: 'bob', r: { since: 2020 } },
        { source: 'alice', target: 'carol' }, // no r
      ],
      ['source', 'target', 'r'],
    ),
  );
  assert.deepEqual(getNodeEdges(conn, 'alice'), [
    ['alice', 'bob', { since: 2020 }],
    ['alice', 'carol', {}],
  ]);
  assert.equal(
    calls[0]?.query,
    'MATCH (n {id: $id})-[r]-(m) RETURN n.id AS source, m.id AS target, r',
  );
});

// --- getEdgesFrom / getEdgesTo (toList) ------------------------------------
test('getEdgesFrom / getEdgesTo: exact directed Cypher, toList() rows', () => {
  {
    const edgeRows = [{ source: 'alice', target: 'bob', r: {} }];
    const { conn, calls } = makeConn(new CypherResult(edgeRows, ['source', 'target', 'r']));
    assert.deepEqual(getEdgesFrom(conn, 'alice'), edgeRows);
    assert.equal(
      calls[0]?.query,
      'MATCH (a {id: $id})-[r]->(b) RETURN a.id AS source, b.id AS target, r',
    );
  }
  {
    const { conn, calls } = makeConn();
    getEdgesTo(conn, 'bob');
    assert.equal(
      calls[0]?.query,
      'MATCH (a)-[r]->(b {id: $id}) RETURN a.id AS source, b.id AS target, r',
    );
  }
});

// --- getEdgesByType sanitize (acceptance #3) -------------------------------
test('getEdgesByType: relType is sanitized then interpolated', () => {
  {
    const { conn, calls } = makeConn();
    getEdgesByType(conn, 'alice', 'KNOWS');
    assert.equal(
      calls[0]?.query,
      'MATCH (a {id: $id})-[r:KNOWS]->(b) RETURN a.id AS source, b.id AS target, r',
    );
  }
  {
    const { conn, calls } = makeConn();
    getEdgesByType(conn, 'alice', 'bad-type');
    assert.match(String(calls[0]?.query), /\[r:bad_type\]/);
  }
});

// --- stats: two queries + rename (acceptance #2) ---------------------------
test('stats: two count queries; cnt renamed to nodeCount/edgeCount', () => {
  const { conn, calls } = makeConn(rows({ cnt: 5 }), rows({ cnt: 7 }));
  assert.deepEqual(stats(conn), { nodeCount: 5, edgeCount: 7 });
  assert.equal(calls.length, 2);
  assert.equal(calls[0]?.query, 'MATCH (n) RETURN count(n) AS cnt');
  assert.equal(calls[1]?.query, 'MATCH ()-[r]->() RETURN count(r) AS cnt');
});

test('stats: empty results yield zero counts', () => {
  const { conn } = makeConn(new CypherResult([], []), new CypherResult([], []));
  assert.deepEqual(stats(conn), { nodeCount: 0, edgeCount: 0 });
});

// --- query passthrough (acceptance #4) -------------------------------------
test('query: passes the caller string and params through unchanged', () => {
  const resultRows = [{ x: 1 }];
  const { conn, calls } = makeConn(new CypherResult(resultRows, ['x']));
  const out = query(conn, 'MATCH (n) RETURN n.x AS x', { limit: 10 });
  assert.deepEqual(out, resultRows);
  assert.deepEqual(calls[0], { query: 'MATCH (n) RETURN n.x AS x', params: { limit: 10 } });
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

test('runs the 8 queries end-to-end against the real extension', gate, () => {
  using g = graph(':memory:');
  g.upsertNode('alice', {}, 'Person');
  g.upsertNode('bob', {}, 'Person');
  g.upsertNode('carol', {}, 'Person');
  g.upsertEdge('alice', 'bob', { since: 2020 }, 'KNOWS');
  g.upsertEdge('alice', 'carol', {}, 'FOLLOWS');

  assert.equal(g.nodeDegree('alice'), 2);
  assert.equal(g.getNeighbors('alice').length, 2);

  const tuples = g.getNodeEdges('alice');
  assert.equal(tuples.length, 2);
  assert.ok(Array.isArray(tuples[0]) && tuples[0].length === 3);

  assert.equal(g.getEdgesFrom('alice').length, 2);
  assert.equal(g.getEdgesTo('bob').length, 1);
  assert.equal(g.getEdgesByType('alice', 'KNOWS').length, 1);

  assert.deepEqual(g.stats(), { nodeCount: 3, edgeCount: 2 });

  const q = g.query('MATCH (n:Person) RETURN n.id AS id');
  assert.ok(Array.isArray(q));
});
