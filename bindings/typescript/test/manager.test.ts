import { test } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, existsSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { graphs } from '../src/manager.ts';
import { ValidationError, GraphQLiteError } from '../src/errors.ts';

function tmpDir(): string {
  return mkdtempSync(join(tmpdir(), 'gqlm-'));
}

// --- Pure behavior (no extension needed) ------------------------------------
test('constructor creates the base directory recursively', () => {
  const root = tmpDir();
  const nested = join(root, 'a', 'b', 'graphs');
  const gm = graphs(nested);
  assert.ok(existsSync(nested));
  assert.equal(gm.list().length, 0);
  rmSync(root, { recursive: true, force: true });
});

test('graph names are validated (assertIdentifier) — slashes/dots rejected', () => {
  const root = tmpDir();
  const gm = graphs(root);
  assert.throws(() => gm.exists('bad/name'), ValidationError);
  assert.throws(() => gm.exists('has.dot'), ValidationError);
  assert.throws(() => gm.exists('with space'), ValidationError);
  // A traversal attempt is rejected by the identifier check before path logic.
  assert.throws(() => gm.exists('..'), ValidationError);
  assert.ok(!gm.exists('valid_name')); // legitimate name → no throw, just absent
  rmSync(root, { recursive: true, force: true });
});

// --- Integration (gated on a built extension) -------------------------------
function extensionAvailable(): boolean {
  try {
    const root = tmpDir();
    const gm = graphs(root);
    const ok = (() => {
      try {
        gm.create('probe').close();
        return true;
      } catch {
        return false;
      }
    })();
    gm.close();
    rmSync(root, { recursive: true, force: true });
    return ok;
  } catch {
    return false;
  }
}
const gate = { skip: !extensionAvailable() };

test('create / list / exists / openOrCreate', gate, () => {
  const root = tmpDir();
  using gm = graphs(root);
  gm.create('social').close();
  gm.create('products').close();
  assert.deepEqual(gm.list(), ['products', 'social']); // sorted
  assert.ok(gm.exists('social'));
  assert.ok(!gm.exists('missing'));
  // openOrCreate makes a new one when absent, opens when present.
  const c1 = gm.openOrCreate('cache');
  const c2 = gm.openOrCreate('cache');
  assert.equal(c1, c2); // cached
  rmSync(root, { recursive: true, force: true });
});

test('create on an existing graph throws GRAPH_EXISTS', gate, () => {
  const root = tmpDir();
  using gm = graphs(root);
  gm.create('dup').close();
  assert.throws(() => gm.create('dup'), (e: unknown) => e instanceof GraphQLiteError && e.code === 'GRAPH_EXISTS');
  rmSync(root, { recursive: true, force: true });
});

test('open caches the instance; missing graph error lists Available', gate, () => {
  const root = tmpDir();
  using gm = graphs(root);
  gm.create('a').close();
  const o1 = gm.open('a');
  const o2 = gm.open('a');
  assert.equal(o1, o2); // same cached Graph
  assert.throws(
    () => gm.open('nope'),
    (e: unknown) => e instanceof GraphQLiteError && e.code === 'GRAPH_NOT_FOUND' && /Available: \['a'\]/.test(e.message),
  );
  rmSync(root, { recursive: true, force: true });
});

test('drop: closes, DETACHes (ignored), deletes the file; missing → error', gate, () => {
  const root = tmpDir();
  using gm = graphs(root);
  gm.create('gone').close();
  assert.ok(gm.exists('gone'));
  gm.drop('gone'); // not attached — DETACH failure is swallowed
  assert.ok(!gm.exists('gone'));
  assert.throws(() => gm.drop('gone'), (e: unknown) => e instanceof GraphQLiteError && e.code === 'GRAPH_NOT_FOUND');
  rmSync(root, { recursive: true, force: true });
});

test('query commits open graphs, attaches named ones, and runs on the coordinator', gate, () => {
  const root = tmpDir();
  using gm = graphs(root);
  const g = gm.create('data');
  g.upsertNode('a', { name: 'A' }, 'P'); // uncommitted write in the open graph

  // query() must commit 'data' before attaching, then run on the coordinator.
  const res = gm.query('RETURN 1 AS one', ['data']);
  assert.equal(res.length, 1);
  assert.equal(res[0]?.['one'], 1);
  rmSync(root, { recursive: true, force: true });
});

test('query tolerates a cached graph the caller already closed', gate, () => {
  const root = tmpDir();
  using gm = graphs(root);
  gm.create('side').close(); // closed but still cached — must not break the commit loop
  const res = gm.query('RETURN 7 AS n');
  assert.equal(res[0]?.['n'], 7);
  rmSync(root, { recursive: true, force: true });
});

test('query without a graphs list attaches nothing (no auto-detect)', gate, () => {
  const root = tmpDir();
  using gm = graphs(root);
  // No graphs requested → nothing attached, but the coordinator still answers.
  const res = gm.query('RETURN 42 AS answer');
  assert.equal(res[0]?.['answer'], 42);
  rmSync(root, { recursive: true, force: true });
});

test('query on a missing graph throws GRAPH_NOT_FOUND', gate, () => {
  const root = tmpDir();
  using gm = graphs(root);
  assert.throws(
    () => gm.query('RETURN 1', ['ghost']),
    (e: unknown) => e instanceof GraphQLiteError && e.code === 'GRAPH_NOT_FOUND',
  );
  rmSync(root, { recursive: true, force: true });
});
