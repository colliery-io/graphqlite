import { test } from 'node:test';
import assert from 'node:assert/strict';
import { hasEdge, getEdge, deleteEdge, getAllEdges } from '../src/graph/edges.ts';
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

// --- Cypher parity + $src/$tgt binding (acceptance criterion #1) ------------
test('hasEdge: bound src/tgt, no relType → bare [r]', () => {
  const { conn, calls } = makeConn(rows({ cnt: 1 }));
  hasEdge(conn, 'alice', 'bob');
  assert.deepEqual(calls[0], {
    query: 'MATCH (a {id: $src})-[r]->(b {id: $tgt}) RETURN count(r) AS cnt',
    params: { src: 'alice', tgt: 'bob' },
  });
});

test('hasEdge: relType is sanitized and interpolated as :TYPE', () => {
  const { conn, calls } = makeConn(rows({ cnt: 1 }));
  hasEdge(conn, 'alice', 'bob', 'KNOWS');
  assert.equal(
    calls[0]?.query,
    'MATCH (a {id: $src})-[r:KNOWS]->(b {id: $tgt}) RETURN count(r) AS cnt',
  );
});

test('relType with unsafe chars / leading digit is sanitized before interpolation', () => {
  {
    const { conn, calls } = makeConn(rows({ cnt: 0 }));
    hasEdge(conn, 'a', 'b', 'bad-type');
    assert.match(String(calls[0]?.query), /\[r:bad_type\]/); // '-' → '_'
  }
  {
    const { conn, calls } = makeConn(rows({ cnt: 0 }));
    hasEdge(conn, 'a', 'b', '1rel');
    assert.match(String(calls[0]?.query), /\[r:REL_1rel\]/); // leading digit → REL_ prefix
  }
});

test('getEdge / deleteEdge / getAllEdges: exact Cypher (Python parity)', () => {
  {
    const { conn, calls } = makeConn(rows({ r: { type: 'KNOWS' } }));
    getEdge(conn, 'alice', 'bob');
    assert.equal(calls[0]?.query, 'MATCH (a {id: $src})-[r]->(b {id: $tgt}) RETURN r');
  }
  {
    const { conn, calls } = makeConn();
    deleteEdge(conn, 'alice', 'bob', 'KNOWS');
    assert.deepEqual(calls[0], {
      query: 'MATCH (a {id: $src})-[r:KNOWS]->(b {id: $tgt}) DELETE r',
      params: { src: 'alice', tgt: 'bob' },
    });
  }
  {
    const { conn, calls } = makeConn(new CypherResult([], []));
    getAllEdges(conn);
    assert.equal(calls[0]?.query, 'MATCH (a)-[r]->(b) RETURN a.id AS source, b.id AS target, r');
    assert.equal(calls[0]?.params, undefined);
  }
});

// --- hasEdge parsing == hasNode (acceptance criterion #2) ------------------
test('hasEdge: empty → false; falsy/zero cnt → false; positive → true', () => {
  assert.equal(hasEdge(makeConn(new CypherResult([], [])).conn, 'a', 'b'), false);
  assert.equal(hasEdge(makeConn(rows({ cnt: 0 })).conn, 'a', 'b'), false);
  assert.equal(hasEdge(makeConn(rows({ cnt: null })).conn, 'a', 'b'), false);
  assert.equal(hasEdge(makeConn(rows({ cnt: 3 })).conn, 'a', 'b'), true);
});

// --- getEdge (acceptance criterion #3) -------------------------------------
test('getEdge: null on empty, row.r unmodified otherwise', () => {
  const edge = { id: 5, type: 'KNOWS', properties: { since: 2020 } };
  assert.equal(getEdge(makeConn(new CypherResult([], [])).conn, 'a', 'b'), null);
  assert.deepEqual(getEdge(makeConn(rows({ r: edge })).conn, 'a', 'b'), edge);
});

// --- getAllEdges (acceptance criterion #4) ---------------------------------
test('getAllEdges: returns toList() unmodified with source/target/r keys', () => {
  const edgeRows = [
    { source: 'alice', target: 'bob', r: { type: 'KNOWS' } },
    { source: 'bob', target: 'carol', r: { type: 'KNOWS' } },
  ];
  const { conn } = makeConn(new CypherResult(edgeRows, ['source', 'target', 'r']));
  assert.deepEqual(getAllEdges(conn), edgeRows);
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

test('round-trips edge read/delete against the real extension', gate, () => {
  using g = graph(':memory:');
  g.connection.cypher("CREATE (a:Person {id: 'alice'})");
  g.connection.cypher("CREATE (b:Person {id: 'bob'})");
  g.connection.cypher(
    "MATCH (a {id: 'alice'}), (b {id: 'bob'}) CREATE (a)-[r:KNOWS {since: 2020}]->(b)",
  );

  assert.equal(g.hasEdge('alice', 'bob'), true);
  assert.equal(g.hasEdge('alice', 'bob', 'KNOWS'), true);
  assert.equal(g.hasEdge('bob', 'alice'), false); // directed

  assert.ok(g.getEdge('alice', 'bob'));

  const all = g.getAllEdges();
  assert.equal(all.length, 1);
  assert.equal(all[0]?.['source'], 'alice');
  assert.equal(all[0]?.['target'], 'bob');

  g.deleteEdge('alice', 'bob');
  assert.equal(g.hasEdge('alice', 'bob'), false);
});
