// True async variants of the long-running graph algorithms (#29 [I-06]).
//
// `node:sqlite` is synchronous, so wrapping a call in `Promise.resolve()` keeps
// it on the main thread and still blocks the event loop for the duration of the
// algorithm. The functions here instead offload the work to a `worker_threads`
// Worker (see ./worker.ts) which opens *its own* connection — a `DatabaseSync`
// handle cannot be shared across threads — runs the synchronous algorithm, and
// posts the plain-object result back. The result of every wrapped algorithm is
// already a structured-clone-safe array of plain objects, so it survives the
// thread boundary untouched.
//
// Because the worker opens the file itself, an in-memory database can never be
// reached from the worker: `:memory:` is private per connection. That case is
// rejected up front (see the `:memory:` guard in `runInWorker`).
import { Worker } from 'node:worker_threads';
import { GraphQLiteError, UnsupportedOperationError } from '../errors.ts';
import type { CentralityScore } from '../algorithms/centrality.ts';
import type { CommunityResult } from '../algorithms/community.ts';
import type { AllPairsPath } from '../algorithms/paths.ts';
import type {
  NodeSimilarityPair,
  KnnNeighbor,
  NodeSimilarityOptions,
  KnnOptions,
} from '../algorithms/similarity.ts';

/**
 * A path to a **file-backed** database.
 *
 * The async algorithms spawn a `worker_threads` Worker that opens its own
 * connection to this path, so it must name a file both threads can open.
 * `':memory:'` is explicitly **not** allowed — an in-memory database is private
 * to the connection that created it and can never be reached from the worker.
 * Passing `':memory:'` throws {@link UnsupportedOperationError} before any
 * worker is spawned.
 */
export type DatabasePath = string;

/** The set of algorithms the async worker knows how to dispatch. */
export type AsyncAlgorithm =
  | 'pagerank'
  | 'louvain'
  | 'betweennessCentrality'
  | 'apsp'
  | 'nodeSimilarity'
  | 'knn';

/** Message the parent hands the worker via `workerData`. */
export interface WorkerRequest {
  algorithm: AsyncAlgorithm;
  database: DatabasePath;
  extensionPath?: string;
  /** Positional args applied after the connection, e.g. `[damping, iterations]`. */
  args: unknown[];
}

/** A thrown error flattened to a clone-safe shape for `postMessage`. */
export interface SerializedError {
  name: string;
  message: string;
  code?: string;
}

/** Reply the worker posts back to the parent. */
export type WorkerResponse =
  | { ok: true; result: unknown }
  | { ok: false; error: SerializedError };

/** Options common to every async variant. */
export interface AsyncWorkerOptions {
  /**
   * Explicit path to the extension binary, forwarded to the worker's
   * `connect()`. Auto-detected when omitted; primarily an escape hatch for
   * tests that run against a staged (not npm-installed) build.
   */
  extensionPath?: string;
}

/** Options for {@link pagerankAsync} — mirrors the sync `pagerank` numeric args. */
export interface PagerankAsyncOptions extends AsyncWorkerOptions {
  /** Damping factor. Defaults to `0.85` (the sync default). */
  damping?: number;
  /** Iteration count. Defaults to `20` (the sync default). */
  iterations?: number;
}

/** Options for {@link louvainAsync} — mirrors the sync `louvain` argument. */
export interface LouvainAsyncOptions extends AsyncWorkerOptions {
  /** Resolution parameter. Defaults to `1.0` (the sync default). */
  resolution?: number;
}

/** Options for {@link nodeSimilarityAsync} — sync options plus the worker options. */
export interface NodeSimilarityAsyncOptions extends NodeSimilarityOptions, AsyncWorkerOptions {}

/** Options for {@link knnAsync} — sync options plus the worker options. */
export interface KnnAsyncOptions extends KnnOptions, AsyncWorkerOptions {}

/** Rebuild a thrown error from the worker's flattened payload. */
function reviveError(error: SerializedError): GraphQLiteError {
  const revived = new GraphQLiteError(error.message, { code: error.code });
  revived.name = error.name;
  return revived;
}

/**
 * Spawn a Worker for `algorithm`, resolve with its result (or reject with its
 * error). The `:memory:` guard fires **before** the worker is spawned — an
 * in-memory database cannot be reached across the thread boundary.
 */
function runInWorker<T>(
  algorithm: AsyncAlgorithm,
  database: DatabasePath,
  args: unknown[],
  options: AsyncWorkerOptions,
): Promise<T> {
  if (database === ':memory:') {
    throw new UnsupportedOperationError(
      "Async algorithms cannot run against an in-memory database (':memory:'): the " +
        'worker thread opens its own connection and an in-memory database is private ' +
        'to the connection that created it. Use a file-backed database path instead.',
    );
  }

  const request: WorkerRequest = { algorithm, database, extensionPath: options.extensionPath, args };

  return new Promise<T>((resolve, reject) => {
    const worker = new Worker(new URL('./worker.ts', import.meta.url), { workerData: request });
    let settled = false;
    const settle = (action: () => void): void => {
      if (settled) {
        return;
      }
      settled = true;
      action();
      void worker.terminate();
    };

    worker.on('message', (response: WorkerResponse) => {
      if (response.ok) {
        settle(() => resolve(response.result as T));
      } else {
        settle(() => reject(reviveError(response.error)));
      }
    });
    worker.on('error', (error: Error) => settle(() => reject(error)));
    worker.on('exit', (code: number) => {
      if (!settled) {
        settle(() =>
          reject(new GraphQLiteError(`Async worker exited with code ${code} before returning a result`)),
        );
      }
    });
  });
}

/**
 * PageRank, offloaded to a worker thread. Same result as the sync
 * {@link pagerank}, computed off the event loop.
 *
 * @param database File-backed database path (`':memory:'` is rejected).
 */
export function pagerankAsync(
  database: DatabasePath,
  options: PagerankAsyncOptions = {},
): Promise<CentralityScore[]> {
  const { damping, iterations, extensionPath } = options;
  return runInWorker('pagerank', database, [damping, iterations], { extensionPath });
}

/**
 * Louvain community detection, offloaded to a worker thread.
 *
 * @param database File-backed database path (`':memory:'` is rejected).
 */
export function louvainAsync(
  database: DatabasePath,
  options: LouvainAsyncOptions = {},
): Promise<CommunityResult[]> {
  const { resolution, extensionPath } = options;
  return runInWorker('louvain', database, [resolution], { extensionPath });
}

/**
 * Betweenness centrality (Brandes), offloaded to a worker thread.
 *
 * @param database File-backed database path (`':memory:'` is rejected).
 */
export function betweennessCentralityAsync(
  database: DatabasePath,
  options: AsyncWorkerOptions = {},
): Promise<CentralityScore[]> {
  return runInWorker('betweennessCentrality', database, [], { extensionPath: options.extensionPath });
}

/**
 * All-pairs shortest path (Floyd–Warshall), offloaded to a worker thread.
 *
 * @param database File-backed database path (`':memory:'` is rejected).
 */
export function apspAsync(
  database: DatabasePath,
  options: AsyncWorkerOptions = {},
): Promise<AllPairsPath[]> {
  return runInWorker('apsp', database, [], { extensionPath: options.extensionPath });
}

/**
 * Node similarity (Jaccard), offloaded to a worker thread.
 *
 * @param database File-backed database path (`':memory:'` is rejected).
 */
export function nodeSimilarityAsync(
  database: DatabasePath,
  options: NodeSimilarityAsyncOptions = {},
): Promise<NodeSimilarityPair[]> {
  const { extensionPath, ...similarityOptions } = options;
  return runInWorker('nodeSimilarity', database, [similarityOptions], { extensionPath });
}

/**
 * K-nearest neighbors (Jaccard), offloaded to a worker thread.
 *
 * @param database File-backed database path (`':memory:'` is rejected).
 * @param nodeId   The node to find neighbors for.
 */
export function knnAsync(
  database: DatabasePath,
  nodeId: string,
  options: KnnAsyncOptions = {},
): Promise<KnnNeighbor[]> {
  const { extensionPath, ...knnOptions } = options;
  return runInWorker('knn', database, [nodeId, knnOptions], { extensionPath });
}
