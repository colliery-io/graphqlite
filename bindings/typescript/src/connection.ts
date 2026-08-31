// The one place the binding touches the driver: load the extension via
// `node:sqlite` and round-trip the `cypher()` UDF.
//
// The whole point of this file is to *cage the driver surface here* (design
// intent from #7). `node:sqlite` is still experimental on Node 24, so if its
// API shifts we swap this single module for a better-sqlite3 adapter without
// touching the rest of the binding.
//
// Mirrors bindings/python/src/graphqlite/connection.py, with three portability
// facts baked in (all verified against build/graphqlite.dylib on Node 24):
//   1. `node:sqlite`'s loadExtension() *auto-appends* the platform suffix, so
//      the resolved `graphqlite.dylib` must have its suffix stripped first —
//      exactly like Python's `Path.stem` (connection.py:107). Otherwise dlopen
//      is handed `graphqlite.dylib.dylib` and fails.
//   2. `$param` is not an SQLite binding — it is a JSON string handed to the
//      core. Params round-trip as `SELECT cypher(?, ?)` with
//      `(query, JSON.stringify(params))`. This is the crux of the port.
//   3. The ExperimentalWarning is emitted during the *link phase* of a static
//      `import 'node:sqlite'`, before any module body runs. Suppressing it
//      therefore requires installing the filter first and importing the module
//      *dynamically* afterwards (see suppressExperimentalWarningOnce below).

import { findExtension, type ResolveOptions } from './platform.ts';
import { GraphQLiteError, graphQLiteErrorFrom, parseCoreError } from './errors.ts';
import { normalizeCypherResult, type CypherResult } from './result.ts';
import type { DatabaseSync as DatabaseSyncInstance, DatabaseSyncOptions } from 'node:sqlite';

/**
 * A value bindable to a SQL parameter. Mirrors `node:sqlite`'s input value
 * without importing it — the exported name has churned across `@types/node`
 * versions (SupportedValueType vs SQLInputValue), and this set is stable.
 */
export type BindValue = null | number | bigint | string | Uint8Array;

/**
 * Swallow exactly one `node:sqlite` ExperimentalWarning.
 *
 * Node emits it via the internal, bootstrap-captured warning path, so neither a
 * `process.emit` override nor removing the `warning` listener stops the print.
 * The one thing that works is overriding `process.emitWarning` *before*
 * `node:sqlite` loads — hence this runs at module top-level and the module is
 * imported dynamically below. Set `GRAPHQLITE_SHOW_EXPERIMENTAL_WARNING=1` to
 * keep the warning. Only the first matching warning is dropped; every other
 * warning (including later experimental ones) passes through untouched.
 */
function suppressExperimentalWarningOnce(): void {
  if (process.env.GRAPHQLITE_SHOW_EXPERIMENTAL_WARNING === '1') {
    return;
  }
  const original = process.emitWarning.bind(process);
  let handled = false;
  const patched: typeof process.emitWarning = function (
    warning: string | Error,
    ...args: unknown[]
  ): void {
    const options = args[0];
    const type =
      typeof options === 'object' && options !== null
        ? (options as { type?: string }).type
        : (options as string | undefined);
    const message = typeof warning === 'string' ? warning : warning?.message ?? '';
    if (!handled && type === 'ExperimentalWarning' && /\bSQLite\b/i.test(String(message))) {
      handled = true;
      process.emitWarning = original; // one-shot: restore immediately
      return;
    }
    (original as (...forwarded: unknown[]) => void)(warning, ...args);
  } as typeof process.emitWarning;
  process.emitWarning = patched;
}

suppressExperimentalWarningOnce();

// Dynamic import so the module loads *after* the suppressor is installed. A
// static `import { DatabaseSync }` would emit the warning at link time, before
// any code here runs. The top-level await makes importing this module async,
// which is transparent to ESM consumers.
const { DatabaseSync } = await import('node:sqlite');

const EXTENSION_SUFFIX = /\.(dylib|so|dll)$/i;

/** Strip the platform suffix so loadExtension() does not double it. */
function stripExtensionSuffix(path: string): string {
  return path.replace(EXTENSION_SUFFIX, '');
}

/** Map an arbitrary thrown value to the F-03 error hierarchy. */
function asGraphQLiteError(error: unknown, query?: string): GraphQLiteError {
  if (error instanceof GraphQLiteError) {
    return error;
  }
  const message = error instanceof Error ? error.message : String(error);
  return graphQLiteErrorFrom(message, query);
}

export interface ConnectionOptions extends ResolveOptions {
  /** Explicit path to the extension binary; auto-detected when omitted. */
  extensionPath?: string;
}

export interface ConnectOptions extends ConnectionOptions {
  /** Extra `node:sqlite` DatabaseSync options (allowExtension is forced on). */
  database?: Omit<DatabaseSyncOptions, 'allowExtension'>;
}

/**
 * A GraphQLite connection: a loaded extension plus the `cypher()` round-trip.
 *
 * Construct via {@link connect} (opens a database) or {@link wrap} (adopts an
 * existing `DatabaseSync` that was created with `allowExtension: true`).
 */
export class Connection {
  readonly #db: DatabaseSyncInstance;

  constructor(db: DatabaseSyncInstance, options: ConnectionOptions = {}) {
    this.#db = db;
    this.#loadExtension(options);
  }

  #loadExtension(options: ConnectionOptions): void {
    const resolved = findExtension(options.extensionPath, options);
    const loadPath = stripExtensionSuffix(resolved);
    try {
      this.#db.enableLoadExtension(true);
      this.#db.loadExtension(loadPath);
      this.#db.enableLoadExtension(false); // re-lock immediately after load
    } catch (error) {
      throw asGraphQLiteError(error);
    }

    // Verify the extension actually initialized. Matches connection.py:123-126.
    let text = '';
    try {
      const row = this.#db.prepare('SELECT graphqlite_test() AS result').get() as
        | { result?: unknown }
        | undefined;
      if (row && typeof row.result === 'string') {
        text = row.result;
      }
    } catch (error) {
      throw asGraphQLiteError(error);
    }
    if (!text.toLowerCase().includes('successfully')) {
      throw new GraphQLiteError('Failed to initialize GraphQLite extension', {
        code: 'EXTENSION_LOAD_ERROR',
      });
    }
  }

  /**
   * Run a Cypher query, optionally with `$param` values.
   *
   * An empty object `{}` deliberately takes the no-params `SELECT cypher(?)`
   * path — `Object.keys(params).length === 0` reproduces Python's falsy-`{}`
   * branch (connection.py:146). Errors surface as the F-03 hierarchy, whether
   * `node:sqlite` throws them or the core returns an `{"error":...}` cell.
   */
  cypher(query: string, params?: Record<string, unknown> | null): CypherResult {
    const hasParams = params != null && Object.keys(params).length > 0;
    let raw: string | null;
    try {
      const statement = this.#db.prepare(
        hasParams ? 'SELECT cypher(?, ?) AS result' : 'SELECT cypher(?) AS result',
      );
      const row = (
        hasParams ? statement.get(query, JSON.stringify(params)) : statement.get(query)
      ) as { result?: unknown } | undefined;
      raw = row && typeof row.result === 'string' ? row.result : null;
    } catch (error) {
      throw asGraphQLiteError(error, query);
    }

    // The core can also report failure in-band as a `{"error":...}` cell rather
    // than by throwing. Detect and promote it before normalization, which never
    // throws (result.ts is a pure shape normalizer).
    if (raw !== null && parseCoreError(raw) !== null) {
      throw graphQLiteErrorFrom(raw, query);
    }
    return normalizeCypherResult(raw);
  }

  /** Run raw SQL, returning the result rows (empty for non-returning statements). */
  execute(sql: string, params: BindValue[] = []): unknown[] {
    try {
      return this.#db.prepare(sql).all(...params);
    } catch (error) {
      throw asGraphQLiteError(error);
    }
  }

  /** Commit the active transaction; a no-op under autocommit (matches Python). */
  commit(): void {
    this.#endTransaction('COMMIT');
  }

  /** Roll back the active transaction; a no-op under autocommit (matches Python). */
  rollback(): void {
    this.#endTransaction('ROLLBACK');
  }

  #endTransaction(kind: 'COMMIT' | 'ROLLBACK'): void {
    try {
      this.#db.exec(kind);
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      // `node:sqlite` autocommits, so there may be no transaction to end. Python's
      // sqlite3 treats commit()/rollback() as harmless there; mirror that.
      if (/no transaction is active/i.test(message)) {
        return;
      }
      throw asGraphQLiteError(error);
    }
  }

  /** Close the underlying database connection. */
  close(): void {
    this.#db.close();
  }

  /** The underlying `node:sqlite` connection (escape hatch for raw driver use). */
  get database(): DatabaseSyncInstance {
    return this.#db;
  }
}

/**
 * Open a GraphQLite database connection.
 *
 * @param database Path to a database file, or `:memory:` (the default).
 */
export function connect(database: string = ':memory:', options: ConnectOptions = {}): Connection {
  const { database: databaseOptions, ...connectionOptions } = options;
  const db = new DatabaseSync(database, { ...databaseOptions, allowExtension: true });
  return new Connection(db, connectionOptions);
}

/**
 * Wrap an existing `node:sqlite` connection with GraphQLite support. The passed
 * `DatabaseSync` must have been created with `allowExtension: true`, otherwise
 * enabling extension loading throws.
 */
export function wrap(db: DatabaseSyncInstance, options: ConnectionOptions = {}): Connection {
  return new Connection(db, options);
}
