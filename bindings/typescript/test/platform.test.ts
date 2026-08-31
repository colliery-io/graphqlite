import { test } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import {
  getExtensionName,
  getPlatformKey,
  getExtensionSearchPaths,
  findExtension,
  ExtensionLoadError,
} from '../src/platform.ts';

test('getExtensionName maps each platform to its filename', () => {
  assert.equal(getExtensionName('darwin'), 'graphqlite.dylib');
  assert.equal(getExtensionName('linux'), 'graphqlite.so');
  assert.equal(getExtensionName('win32'), 'graphqlite.dll');
});

test('getExtensionName throws on an unsupported platform', () => {
  assert.throws(() => getExtensionName('sunos'), /Unsupported platform/);
});

test('win32 search paths use .dll (Python helper omission is not followed)', () => {
  const paths = getExtensionSearchPaths({ platform: 'win32', arch: 'x64', env: {} });
  assert.ok(paths.length > 0);
  assert.ok(paths.every((p) => p.endsWith('graphqlite.dll')));
});

test('getPlatformKey combines platform and arch', () => {
  assert.equal(getPlatformKey('darwin', 'arm64'), 'darwin-arm64');
  assert.equal(getPlatformKey('linux', 'x64'), 'linux-x64');
});

test('GRAPHQLITE_EXTENSION_PATH takes highest priority', () => {
  const paths = getExtensionSearchPaths({
    platform: 'linux',
    arch: 'x64',
    env: { GRAPHQLITE_EXTENSION_PATH: '/custom/graphqlite.so' },
  });
  assert.equal(paths[0], '/custom/graphqlite.so');
});

test('system paths come last, in /usr/local/lib then /usr/lib order', () => {
  const paths = getExtensionSearchPaths({ platform: 'linux', arch: 'x64', env: {} });
  assert.deepEqual(paths.slice(-2), ['/usr/local/lib/graphqlite.so', '/usr/lib/graphqlite.so']);
});

test('findExtension short-circuits on an existing explicit path', () => {
  const dir = mkdtempSync(join(tmpdir(), 'gqlite-plat-'));
  const file = join(dir, 'graphqlite.dylib');
  writeFileSync(file, '');
  try {
    assert.equal(findExtension(file), file);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test('findExtension throws ExtensionLoadError listing searched paths', () => {
  // Use win32 so the dev-build candidate is `build/graphqlite.dll`, which never
  // exists on the linux CI runner (make builds .so) or a macOS dev box (.dylib),
  // and the win32 bundled package is not installed off-Windows. That guarantees
  // *no* search path resolves, so the error path is exercised everywhere.
  try {
    findExtension(undefined, {
      platform: 'win32',
      arch: 'x64',
      env: { GRAPHQLITE_EXTENSION_PATH: 'C:\\definitely\\missing\\graphqlite.dll' },
    });
    assert.fail('expected findExtension to throw');
  } catch (err) {
    assert.ok(err instanceof ExtensionLoadError);
    assert.ok(err.searchedPaths.includes('C:\\definitely\\missing\\graphqlite.dll'));
    assert.match(err.message, /Searched/);
    // The dlopen doubled-extension artifact must never surface here.
    assert.doesNotMatch(err.message, /\.dll\.dll/);
  }
});

test('findExtension with a missing explicit path throws with that path', () => {
  try {
    findExtension('/no/such/graphqlite.dylib');
    assert.fail('expected findExtension to throw');
  } catch (err) {
    assert.ok(err instanceof ExtensionLoadError);
    assert.deepEqual(err.searchedPaths, ['/no/such/graphqlite.dylib']);
  }
});
