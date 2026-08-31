import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import * as mod from '../src/index.ts';

// Public-surface verification — the TS counterpart of Python's
// test_connection.py::test_exports / test_version_exists.

test('factory functions are exported', () => {
  for (const name of ['connect', 'wrap', 'graph', 'graphs'] as const) {
    assert.equal(typeof mod[name], 'function', `${name} should be a function`);
  }
});

test('core classes are exported as constructors', () => {
  for (const name of ['Connection', 'Graph', 'GraphManager'] as const) {
    assert.equal(typeof mod[name], 'function', `${name} should be a constructor`);
  }
});

test('the full error hierarchy is exported', () => {
  const errorClasses = [
    'GraphQLiteError',
    'ParseError',
    'ValidationError',
    'ExecutionError',
    'UnsupportedOperationError',
    'ExtensionLoadError',
  ] as const;
  for (const name of errorClasses) {
    assert.equal(typeof mod[name], 'function', `${name} should be exported`);
    // Every error class extends GraphQLiteError.
    assert.ok(
      mod[name].prototype instanceof mod.GraphQLiteError || name === 'GraphQLiteError',
      `${name} should extend GraphQLiteError`,
    );
  }
});

test('a version constant is exported as a string', () => {
  assert.equal(typeof mod.VERSION, 'string');
  assert.match(mod.VERSION, /^\d+\.\d+\.\d+/);
});

test('VERSION matches package.json "version"', () => {
  const pkg = JSON.parse(
    readFileSync(fileURLToPath(new URL('../package.json', import.meta.url)), 'utf8'),
  );
  assert.equal(mod.VERSION, pkg.version);
});

test('package.json defines the "." and "./async" subpath exports', () => {
  const pkg = JSON.parse(
    readFileSync(fileURLToPath(new URL('../package.json', import.meta.url)), 'utf8'),
  );
  assert.equal(pkg.exports['.'], './src/index.ts');
  assert.equal(pkg.exports['./async'], './src/async.ts');
});

test('the graphqlite/async subpath re-exports the full sync surface plus the async variants', async () => {
  const asyncMod = await import('../src/async.ts');
  const asyncKeys = new Set(Object.keys(asyncMod));

  // 1. Every sync export is still reachable via the async subpath.
  for (const name of Object.keys(mod)) {
    assert.ok(asyncKeys.has(name), `async surface should include sync export "${name}"`);
  }

  // 2. The six worker_threads-backed async variants are exported as functions.
  const asyncFns = [
    'pagerankAsync',
    'louvainAsync',
    'betweennessCentralityAsync',
    'apspAsync',
    'nodeSimilarityAsync',
    'knnAsync',
  ] as const;
  for (const name of asyncFns) {
    assert.equal(
      typeof (asyncMod as Record<string, unknown>)[name],
      'function',
      `${name} should be exported as a function`,
    );
  }

  // 3. The async surface adds *exactly* those six runtime exports on top of the
  //    sync surface — no accidental extras leak from the worker plumbing.
  const extras = Object.keys(asyncMod).filter((name) => !(name in mod));
  assert.deepEqual(extras.sort(), [...asyncFns].sort());
});
