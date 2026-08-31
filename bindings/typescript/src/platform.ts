// Platform detection and extension path resolution.
//
// Mirrors the search-order contract of the Python binding's `_platform.py`
// so all three bindings locate the extension the same way. See
// docs/internal/typescript-bindings-design.md §5.1.
import { existsSync } from 'node:fs';
import { resolve } from 'node:path';
import { createRequire } from 'node:module';

const EXTENSION_NAMES: Record<string, string> = {
  darwin: 'graphqlite.dylib',
  linux: 'graphqlite.so',
  win32: 'graphqlite.dll',
};

export interface ResolveOptions {
  /** Override the target platform (defaults to `process.platform`). */
  platform?: NodeJS.Platform;
  /** Override the target arch (defaults to `process.arch`). */
  arch?: string;
  /** Environment source (defaults to `process.env`). */
  env?: Record<string, string | undefined>;
}

/**
 * Raised when the extension binary cannot be located.
 *
 * Scope note (F-02): the canonical error hierarchy lives in `errors.ts` (#3).
 * This minimal standalone class satisfies the F-02 acceptance criteria; once
 * `errors.ts` lands it should re-export / extend `GraphQLiteError`.
 */
export class ExtensionLoadError extends Error {
  readonly code = 'EXTENSION_LOAD_ERROR';
  readonly searchedPaths: string[];

  constructor(searchedPaths: string[]) {
    super(
      `GraphQLite extension not found. Searched:\n` +
        searchedPaths.map((p) => `  - ${p}`).join('\n') +
        `\nSet GRAPHQLITE_EXTENSION_PATH or build the extension with 'make extension'.`,
    );
    this.name = 'ExtensionLoadError';
    this.searchedPaths = searchedPaths;
  }
}

/** Platform-specific extension filename (darwin→.dylib, linux→.so, win32→.dll). */
export function getExtensionName(platform: NodeJS.Platform = process.platform): string {
  const name = EXTENSION_NAMES[platform];
  if (!name) {
    throw new Error(`Unsupported platform: ${platform}`);
  }
  return name;
}

/** npm sub-package key, e.g. `darwin-arm64` — matches the optionalDependencies. */
export function getPlatformKey(
  platform: NodeJS.Platform = process.platform,
  arch: string = process.arch,
): string {
  return `${platform}-${arch}`;
}

function resolveBundled(key: string, name: string): string | null {
  // The @graphqlite/<key> sub-package is an optionalDependency that is only
  // present on a matching platform. Absence is normal — fall through silently.
  try {
    const require = createRequire(import.meta.url);
    return require.resolve(`@graphqlite/${key}/${name}`);
  } catch {
    return null;
  }
}

/**
 * Ordered candidate paths, highest priority first:
 * 1. `GRAPHQLITE_EXTENSION_PATH`
 * 2. bundled `@graphqlite/<platform>/graphqlite.<ext>`
 * 3. dev build `<repo>/build/graphqlite.<ext>`
 * 4. `/usr/local/lib`
 * 5. `/usr/lib`
 */
export function getExtensionSearchPaths(options: ResolveOptions = {}): string[] {
  const platform = options.platform ?? process.platform;
  const arch = options.arch ?? process.arch;
  const env = options.env ?? process.env;
  const name = getExtensionName(platform);
  const key = getPlatformKey(platform, arch);

  const paths: string[] = [];

  const envPath = env.GRAPHQLITE_EXTENSION_PATH;
  if (envPath) {
    paths.push(envPath);
  }

  const bundled = resolveBundled(key, name);
  if (bundled) {
    paths.push(bundled);
  }

  // src/platform.ts → src → typescript → bindings → <repo root>
  paths.push(resolve(import.meta.dirname, '..', '..', '..', 'build', name));
  paths.push(resolve('/usr/local/lib', name));
  paths.push(resolve('/usr/lib', name));

  return paths;
}

/**
 * Resolve the extension to a full, existing path.
 *
 * Every candidate is checked with `fs.existsSync` *before* returning, so a
 * bad path never reaches `node:sqlite`'s `loadExtension` (which would leak a
 * doubled-extension dlopen message like `/x.dylib.dylib`). If nothing exists,
 * throws {@link ExtensionLoadError} carrying the searched paths.
 */
export function findExtension(extensionPath?: string, options: ResolveOptions = {}): string {
  if (extensionPath) {
    if (existsSync(extensionPath)) {
      return resolve(extensionPath);
    }
    throw new ExtensionLoadError([extensionPath]);
  }

  const searchPaths = getExtensionSearchPaths(options);
  for (const candidate of searchPaths) {
    if (existsSync(candidate)) {
      return resolve(candidate);
    }
  }
  throw new ExtensionLoadError(searchPaths);
}
