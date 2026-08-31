// GraphManager — manage many graphs in a directory, one `.db` file each, with
// cross-graph queries via a shared in-memory coordinator that ATTACHes them.
//
// Mirrors bindings/python/src/graphqlite/manager.py, with two deliberate
// divergences the TS binding hardens (both called out in the element spec):
//   1. Graph names go through assertIdentifier and every resolved path is
//      checked to stay inside basePath — Python interpolates freely and even
//      allows `../x`. Here that throws.
//   2. `query()` without an explicit `graphs` list attaches NOTHING. Python's
//      docstring claims auto-detection from the query, but no such code exists;
//      this reproduces the real behavior and documents it truthfully.
import { mkdirSync, existsSync, readdirSync, rmSync } from 'node:fs';
import { join, resolve, sep } from 'node:path';
import { connect, type Connection, type ConnectionOptions } from './connection.ts';
import { Graph } from './graph/index.ts';
import { assertIdentifier } from './utils.ts';
import { GraphQLiteError, ValidationError } from './errors.ts';
import type { CypherResult } from './result.ts';

/** Options for {@link GraphManager} / {@link graphs}. */
export type GraphManagerOptions = ConnectionOptions;

export class GraphManager {
  readonly #basePath: string;
  readonly #options: ConnectionOptions;
  readonly #openGraphs = new Map<string, Graph>();
  #coordinator: Connection | null = null;

  constructor(basePath: string, options: GraphManagerOptions = {}) {
    this.#basePath = basePath;
    this.#options = options;
    // Ensure base directory exists (recursive, like Python's mkdir(parents)).
    mkdirSync(basePath, { recursive: true });
  }

  // Resolve `{basePath}/{name}.db`, validating the name as an identifier and
  // asserting the result stays inside basePath (the injection boundary).
  #graphPath(name: string): string {
    assertIdentifier(name, 'graph');
    const path = join(this.#basePath, `${name}.db`);
    const base = resolve(this.#basePath);
    const resolved = resolve(path);
    if (resolved !== base && !resolved.startsWith(base + sep)) {
      throw new ValidationError(`Graph path escapes base directory: ${JSON.stringify(name)}`, {
        code: 'VALIDATION_ERROR',
      });
    }
    return path;
  }

  #ensureCoordinator(): Connection {
    if (this.#coordinator === null) {
      this.#coordinator = connect(':memory:', this.#options);
    }
    return this.#coordinator;
  }

  // ATTACH each requested graph; a missing graph is a hard error, but an
  // "already in use" attach is ignored (idempotent), every other error propagates.
  #attach(coord: Connection, names: string[]): void {
    for (const name of names) {
      const path = this.#graphPath(name);
      if (!existsSync(path)) {
        throw new GraphQLiteError(`Graph '${name}' not found. Available: ${this.#available()}`, {
          code: 'GRAPH_NOT_FOUND',
        });
      }
      try {
        // SQLite string literal: escape single quotes by doubling.
        coord.execute(`ATTACH DATABASE '${path.replace(/'/g, "''")}' AS ${name}`);
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        if (!/already in use/i.test(message)) {
          throw error;
        }
      }
    }
  }

  #available(): string {
    return `[${this.list().map((n) => `'${n}'`).join(', ')}]`;
  }

  // Close a graph, tolerating an already-closed connection. Python's sqlite3
  // treats a double close() as harmless; node:sqlite throws, so we swallow it
  // to keep that behavior (a caller may have closed the graph it was handed).
  #closeGraph(graph: Graph): void {
    try {
      graph.close();
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      if (!/database is not open|not open/i.test(message)) {
        throw error;
      }
    }
  }

  /** List graph names (without `.db`), sorted. */
  list(): string[] {
    return readdirSync(this.#basePath)
      .filter((f) => f.endsWith('.db'))
      .map((f) => f.slice(0, -3))
      .sort();
  }

  /** Whether a graph file exists. */
  exists(name: string): boolean {
    return existsSync(this.#graphPath(name));
  }

  /** Create a new graph. Throws if it already exists. */
  create(name: string): Graph {
    const path = this.#graphPath(name);
    if (existsSync(path)) {
      throw new GraphQLiteError(`Graph '${name}' already exists at ${path}`, {
        code: 'GRAPH_EXISTS',
      });
    }
    const graph = new Graph(path, this.#options);
    this.#openGraphs.set(name, graph);
    return graph;
  }

  /** Open an existing graph, reusing the cached instance if already open. */
  open(name: string): Graph {
    const cached = this.#openGraphs.get(name);
    if (cached !== undefined) {
      return cached;
    }
    const path = this.#graphPath(name);
    if (!existsSync(path)) {
      throw new GraphQLiteError(`Graph '${name}' not found. Available: ${this.#available()}`, {
        code: 'GRAPH_NOT_FOUND',
      });
    }
    const graph = new Graph(path, this.#options);
    this.#openGraphs.set(name, graph);
    return graph;
  }

  /** Open a graph, creating it if it does not exist. */
  openOrCreate(name: string): Graph {
    return this.exists(name) ? this.open(name) : this.create(name);
  }

  /** Delete a graph: close if open → DETACH (ignore failure) → delete file. */
  drop(name: string): void {
    const path = this.#graphPath(name);
    if (!existsSync(path)) {
      throw new GraphQLiteError(`Graph '${name}' not found. Available: ${this.#available()}`, {
        code: 'GRAPH_NOT_FOUND',
      });
    }
    const open = this.#openGraphs.get(name);
    if (open !== undefined) {
      this.#closeGraph(open);
      this.#openGraphs.delete(name);
    }
    if (this.#coordinator !== null) {
      try {
        this.#coordinator.execute(`DETACH DATABASE ${name}`);
      } catch {
        // Not attached — ignore, like Python's OperationalError pass.
      }
    }
    rmSync(path);
  }

  /**
   * Cross-graph Cypher query. Open graphs are committed first so their writes
   * are visible to the coordinator, then the named graphs are attached.
   *
   * **Omitting `graphs` attaches nothing** — there is no auto-detection despite
   * the Python docstring (reproduced faithfully).
   */
  query(
    cypher: string,
    graphs?: string[] | null,
    params?: Record<string, unknown> | null,
  ): CypherResult {
    // Commit any *open* graph connections so their data is visible. A cached
    // graph the caller already closed has nothing to commit (its file is
    // flushed on close), so a "database is not open" here is skipped, not fatal.
    for (const graph of this.#openGraphs.values()) {
      try {
        graph.connection.commit();
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        if (!/database is not open|not open/i.test(message)) {
          throw error;
        }
      }
    }
    const coord = this.#ensureCoordinator();
    if (graphs && graphs.length > 0) {
      this.#attach(coord, graphs);
    }
    return coord.cypher(cypher, params ?? undefined);
  }

  /** Raw SQL across attached graphs — the power-user escape hatch. */
  querySql(sql: string, graphs: string[], parameters: (null | number | bigint | string | Uint8Array)[] = []): unknown[] {
    const coord = this.#ensureCoordinator();
    this.#attach(coord, graphs);
    return coord.execute(sql, parameters);
  }

  /** Close all open graphs and the coordinator. */
  close(): void {
    for (const graph of this.#openGraphs.values()) {
      this.#closeGraph(graph);
    }
    this.#openGraphs.clear();
    if (this.#coordinator !== null) {
      this.#coordinator.close();
      this.#coordinator = null;
    }
  }

  /** Enables `using gm = graphs(...)` — disposes by closing everything. */
  [Symbol.dispose](): void {
    this.close();
  }
}

/**
 * Create a {@link GraphManager} for a directory of graphs.
 *
 * @param basePath Directory where `{name}.db` files live (created if missing).
 */
export function graphs(basePath: string, options: GraphManagerOptions = {}): GraphManager {
  return new GraphManager(basePath, options);
}
