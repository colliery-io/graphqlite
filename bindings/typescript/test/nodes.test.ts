import { test } from 'node:test';
import assert from 'node:assert/strict';
import { hasNode, getNode, deleteNode, getAllNodes } from '../src/graph/nodes.ts';
import { graph } from '../src/graph/index.ts';
import { CypherResult, type CypherRow } from '../src/result.ts';
import { ValidationError } from '../src/errors.ts';

// --- Connection test double -------------------------------------------------
// Records every (query, params) and returns queued CypherResults, so the
// generated Cypher and the dual-path parsing can be checked without a build.
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

// --- hasNode (acceptance criterion #1) -------------------------------------
test('hasNode: empty result → false; falsy/zero cnt → false; positive → true', () => {
  {
    const { conn } = makeConn(new CypherResult([], []));
    assert.equal(hasNode(conn, 'x'), false);
  }
  {
    const { conn } = makeConn(rows({ cnt: 0 }));
    assert.equal(hasNode(conn, 'x'), false);
  }
  {
    const { conn } = makeConn(rows({ cnt: null }));
    assert.equal(hasNode(conn, 'x'), false);
  }
  {
    const { conn } = makeConn(rows({ cnt: 2 }));
    assert.equal(hasNode(conn, 'x'), true);
  }
});

test('hasNode: Cypher and params match Python', () => {
  const { conn, calls } = makeConn(rows({ cnt: 1 }));
  hasNode(conn, 'alice');
  assert.deepEqual(calls[0], {
    query: 'MATCH (n {id: $id}) RETURN count(n) AS cnt',
    params: { id: 'alice' },
  });
});

// --- getNode (acceptance criterion #2) -------------------------------------
test('getNode: returns row.n unmodified, or null when empty', () => {
  const node = { id: 'alice', labels: ['Person'], properties: { name: 'Alice' } };
  {
    const { conn, calls } = makeConn(rows({ n: node }));
    assert.deepEqual(getNode(conn, 'alice'), node);
    assert.deepEqual(calls[0], { query: 'MATCH (n {id: $id}) RETURN n', params: { id: 'alice' } });
  }
  {
    const { conn } = makeConn(new CypherResult([], []));
    assert.equal(getNode(conn, 'missing'), null);
  }
});

// --- deleteNode ------------------------------------------------------------
test('deleteNode: emits DETACH DELETE with the bound id', () => {
  const { conn, calls } = makeConn();
  deleteNode(conn, 'alice');
  assert.deepEqual(calls[0], {
    query: 'MATCH (n {id: $id}) DETACH DELETE n',
    params: { id: 'alice' },
  });
});

// --- getAllNodes label handling (acceptance criteria #3, #5) ---------------
test('getAllNodes: label is validated and interpolated; no label uses MATCH (n)', () => {
  {
    const { conn, calls } = makeConn(rows({ n: { id: 1 } }));
    getAllNodes(conn, 'Person');
    assert.equal(calls[0]?.query, 'MATCH (n:Person) RETURN n');
  }
  {
    const { conn, calls } = makeConn(rows({ n: { id: 1 } }));
    getAllNodes(conn);
    assert.equal(calls[0]?.query, 'MATCH (n) RETURN n');
  }
  {
    const { conn } = makeConn();
    assert.throws(() => getAllNodes(conn, 'bad label; DROP'), ValidationError);
  }
});

// --- getAllNodes dual-path parsing (acceptance criterion #4) ---------------
test('getAllNodes: string result key → JSON.parse, collect item.n', () => {
  const { conn } = makeConn(rows({ result: '[{"n":{"id":1}},{"x":2},{"n":{"id":3}}]' }));
  assert.deepEqual(getAllNodes(conn), [{ id: 1 }, { id: 3 }]);
});

test('getAllNodes: a non-JSON result string is silently ignored', () => {
  const { conn } = makeConn(rows({ result: 'not json at all' }));
  assert.deepEqual(getAllNodes(conn), []);
});

test('getAllNodes: row.n path collects only truthy n', () => {
  const { conn } = makeConn(
    new CypherResult([{ n: { id: 1 } }, { n: null }, { n: { id: 2 } }], ['n']),
  );
  assert.deepEqual(getAllNodes(conn), [{ id: 1 }, { id: 2 }]);
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

test('round-trips node read/delete against the real extension', gate, () => {
  using g = graph(':memory:');
  assert.deepEqual(g.getAllNodes(), []);

  g.connection.cypher("CREATE (n:Person {id: 'alice', name: 'Alice'})");
  assert.equal(g.hasNode('alice'), true);
  assert.equal(g.hasNode('nobody'), false);

  const node = g.getNode('alice') as { properties?: Record<string, unknown> } | null;
  assert.ok(node && typeof node === 'object');

  g.connection.cypher("CREATE (n:Company {id: 'acme'})");
  assert.equal(g.getAllNodes('Person').length, 1);
  assert.equal(g.getAllNodes().length, 2);

  g.deleteNode('alice');
  assert.equal(g.hasNode('alice'), false);
});

// --- Facade exposes the delegations ----------------------------------------
test('Graph delegates the node methods', () => {
  assert.equal(typeof hasNode, 'function');
  assert.equal(typeof getNode, 'function');
  assert.equal(typeof deleteNode, 'function');
  assert.equal(typeof getAllNodes, 'function');
});
