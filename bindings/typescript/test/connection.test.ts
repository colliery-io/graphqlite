import { test } from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtempSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { Connection, connect } from '../src/connection.ts';
import { ParseError, GraphQLiteError } from '../src/errors.ts';

// --- Test double -----------------------------------------------------------
// A minimal DatabaseSync stand-in so the branch logic (empty-object routing,
// error mapping, verification) can be tested without a built extension. It
// records every prepared SQL string and every get() argument list.
interface MockCalls {
  prepared: string[];
  getArgs: unknown[][];
  extLoadEnabled: boolean[];
}

function makeMockDb(cypherCell: string | null, testResult = 'GraphQLite extension loaded successfully!') {
  const calls: MockCalls = { prepared: [], getArgs: [], extLoadEnabled: [] };
  const db = {
    enableLoadExtension(on: boolean) {
      calls.extLoadEnabled.push(on);
    },
    loadExtension() {},
    exec() {},
    close() {},
    prepare(sql: string) {
      calls.prepared.push(sql);
      return {
        get(...args: unknown[]) {
          calls.getArgs.push(args);
          if (sql.includes('graphqlite_test')) {
            return { result: testResult };
          }
          return cypherCell === null ? undefined : { result: cypherCell };
        },
        all() {
          return [];
        },
      };
    },
  };
  return { db, calls };
}

// A real, existing file so findExtension() short-circuits without a build.
function withFakeExtension<T>(fn: (extensionPath: string) => T): T {
  const dir = mkdtempSync(join(tmpdir(), 'gqlite-conn-'));
  const extensionPath = join(dir, 'graphqlite.dylib');
  writeFileSync(extensionPath, '');
  try {
    return fn(extensionPath);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}

function cypherPrepares(calls: MockCalls): string[] {
  return calls.prepared.filter((sql) => sql.includes('cypher('));
}

// --- Empty-object routing (acceptance criterion #1) ------------------------
test('empty object {} takes the no-params SELECT cypher(?) path', () => {
  withFakeExtension((extensionPath) => {
    const { db, calls } = makeMockDb('[]');
    const conn = new Connection(db as never, { extensionPath });
    conn.cypher('MATCH (n) RETURN n', {});
    assert.deepEqual(cypherPrepares(calls), ['SELECT cypher(?) AS result']);
    // Only the query is bound — no JSON params argument.
    assert.deepEqual(calls.getArgs.at(-1), ['MATCH (n) RETURN n']);
  });
});

test('null / undefined params also take the no-params path', () => {
  withFakeExtension((extensionPath) => {
    for (const params of [undefined, null]) {
      const { db, calls } = makeMockDb('[]');
      const conn = new Connection(db as never, { extensionPath });
      conn.cypher('MATCH (n) RETURN n', params);
      assert.deepEqual(cypherPrepares(calls), ['SELECT cypher(?) AS result']);
    }
  });
});

test('non-empty params take SELECT cypher(?, ?) with JSON-encoded params', () => {
  withFakeExtension((extensionPath) => {
    const { db, calls } = makeMockDb('[{"n.name":"Alice"}]');
    const conn = new Connection(db as never, { extensionPath });
    conn.cypher('MATCH (n) WHERE n.name = $name RETURN n.name', { name: 'Alice' });
    assert.deepEqual(cypherPrepares(calls), ['SELECT cypher(?, ?) AS result']);
    assert.deepEqual(calls.getArgs.at(-1), [
      'MATCH (n) WHERE n.name = $name RETURN n.name',
      '{"name":"Alice"}',
    ]);
  });
});

// --- Extension verification (acceptance criterion #2) ----------------------
test('a graphqlite_test() result without "successfully" fails construction', () => {
  withFakeExtension((extensionPath) => {
    const { db } = makeMockDb('[]', 'extension is broken');
    assert.throws(
      () => new Connection(db as never, { extensionPath }),
      (err: unknown) =>
        err instanceof GraphQLiteError && /Failed to initialize/.test(err.message),
    );
  });
});

test('the extension suffix is stripped before loadExtension (no .dylib.dylib)', () => {
  withFakeExtension((extensionPath) => {
    let loaded: string | undefined;
    const base = makeMockDb('[]');
    const db = { ...base.db, loadExtension: (p: string) => void (loaded = p) };
    new Connection(db as never, { extensionPath });
    assert.equal(loaded, extensionPath.replace(/\.dylib$/, ''));
    assert.doesNotMatch(String(loaded), /\.dylib$/);
  });
});

// --- Error mapping to the F-03 hierarchy (acceptance criterion #5) ---------
test('an in-band {"error":...} cell is promoted to a typed error', () => {
  withFakeExtension((extensionPath) => {
    const { db } = makeMockDb('{"error":"Line 1, Col 1: syntax error","code":"PARSE_ERROR"}');
    const conn = new Connection(db as never, { extensionPath });
    assert.throws(
      () => conn.cypher('THIS IS NOT CYPHER'),
      (err: unknown) =>
        err instanceof ParseError &&
        err.code === 'PARSE_ERROR' &&
        err.line === 1 &&
        err.column === 1 &&
        err.query === 'THIS IS NOT CYPHER',
    );
  });
});

test('a thrown driver error carrying core JSON is mapped, not leaked raw', () => {
  withFakeExtension((extensionPath) => {
    const base = makeMockDb('[]');
    const db = {
      ...base.db,
      prepare(sql: string) {
        if (sql.includes('graphqlite_test')) {
          return { get: () => ({ result: 'loaded successfully' }), all: () => [] };
        }
        return {
          get() {
            throw new Error('{"error":"boom","code":"EXECUTION_ERROR"}');
          },
          all: () => [],
        };
      },
    };
    const conn = new Connection(db as never, { extensionPath });
    assert.throws(
      () => conn.cypher('MATCH (n) RETURN n'),
      (err: unknown) =>
        err instanceof GraphQLiteError && err.code === 'EXECUTION_ERROR' && err.message === 'boom',
    );
  });
});

// --- Transaction helpers ---------------------------------------------------
test('commit()/rollback() swallow "no transaction is active" (autocommit no-op)', () => {
  withFakeExtension((extensionPath) => {
    const base = makeMockDb('[]');
    const db = {
      ...base.db,
      exec(sql: string) {
        throw new Error(`cannot ${sql.toLowerCase()} - no transaction is active`);
      },
    };
    const conn = new Connection(db as never, { extensionPath });
    assert.doesNotThrow(() => conn.commit());
    assert.doesNotThrow(() => conn.rollback());
  });
});

// --- ExperimentalWarning suppression (acceptance criterion #3) -------------
// Run in a child process: the warning is emitted during node:sqlite load, so it
// can only be observed from a fresh process that imports the module.
function importConnectionModule(env: Record<string, string | undefined>): string {
  const moduleUrl = new URL('../src/connection.ts', import.meta.url).href;
  const result = spawnSync(
    process.execPath,
    ['--input-type=module', '--eval', `await import(${JSON.stringify(moduleUrl)});`],
    { encoding: 'utf8', env: { ...process.env, ...env } },
  );
  assert.equal(result.status, 0, `child exited nonzero:\n${result.stderr}`);
  return result.stderr;
}

// node:sqlite is stabilizing across Node releases — newer runtimes no longer
// emit the ExperimentalWarning at all. Probe a raw import so the "restore" test
// only asserts on Nodes that actually have a warning to restore; where none is
// emitted the suppressor is a correct no-op and the assertion is skipped.
function nodeEmitsSqliteWarning(): boolean {
  const result = spawnSync(
    process.execPath,
    ['--input-type=module', '--eval', `await import('node:sqlite');`],
    { encoding: 'utf8', env: { ...process.env } },
  );
  return /SQLite is an experimental feature/.test(result.stderr);
}

test('the node:sqlite ExperimentalWarning is swallowed by default', () => {
  const stderr = importConnectionModule({ GRAPHQLITE_SHOW_EXPERIMENTAL_WARNING: undefined });
  assert.doesNotMatch(stderr, /SQLite is an experimental feature/);
});

test(
  'GRAPHQLITE_SHOW_EXPERIMENTAL_WARNING=1 restores the warning',
  { skip: !nodeEmitsSqliteWarning() },
  () => {
    const stderr = importConnectionModule({ GRAPHQLITE_SHOW_EXPERIMENTAL_WARNING: '1' });
    assert.match(stderr, /SQLite is an experimental feature/);
  },
);

// --- Integration round-trip (gated on a built extension) -------------------
// Runs only when the real extension resolves; otherwise skipped so the suite
// passes on machines without a build.
function extensionAvailable(): boolean {
  try {
    connect(':memory:').close();
    return true;
  } catch {
    return false;
  }
}

test('round-trips CREATE / MATCH / $param against the real extension', { skip: !extensionAvailable() }, () => {
  const db = connect(':memory:');
  try {
    // A DDL summary is non-JSON text → wrapped as a single { result } row.
    const created = db.cypher("CREATE (n:Person {name: 'Alice'})");
    assert.equal(created.length, 1);
    assert.match(String(created[0]?.result), /successfully/);

    // A RETURN is a JSON row set → column-keyed rows.
    const all = db.cypher('MATCH (n:Person) RETURN n.name');
    assert.equal(all[0]?.['n.name'], 'Alice');

    const filtered = db.cypher('MATCH (n:Person) WHERE n.name = $name RETURN n.name', {
      name: 'Alice',
    });
    assert.equal(filtered[0]?.['n.name'], 'Alice');

    // Empty object must behave exactly like no params.
    const empty = db.cypher('MATCH (n:Person) RETURN n.name', {});
    assert.deepEqual(empty.toList(), all.toList());
  } finally {
    db.close();
  }
});

test('a malformed query throws ParseError against the real extension', { skip: !extensionAvailable() }, () => {
  const db = connect(':memory:');
  try {
    assert.throws(
      () => db.cypher('THIS IS NOT CYPHER @@'),
      (err: unknown) => err instanceof ParseError && err.code === 'PARSE_ERROR',
    );
  } finally {
    db.close();
  }
});
