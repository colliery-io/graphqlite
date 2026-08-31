// worker_threads entry point for the async algorithms (#29 [I-06]).
//
// This module runs *inside* a Worker. It receives `{ algorithm, database,
// extensionPath?, args }` via `workerData`, opens **its own** connection (a
// `node:sqlite` `DatabaseSync` handle cannot cross a thread boundary), runs the
// requested synchronous algorithm, and posts a clone-safe result back to the
// parent. Any thrown value is flattened to `{ name, message, code }` and posted
// as an error response instead. The connection is always closed afterwards.
//
// Node executes this `.ts` file directly (native type stripping on Node >=24);
// no build step is involved when spawned via `new Worker(new URL('./worker.ts',
// import.meta.url))`.
import { parentPort, workerData } from 'node:worker_threads';
import { connect } from '../connection.ts';
import { pagerank, betweennessCentrality } from '../algorithms/centrality.ts';
import { louvain } from '../algorithms/community.ts';
import { allPairsShortestPath } from '../algorithms/paths.ts';
import { nodeSimilarity, knn } from '../algorithms/similarity.ts';
import type { Connection } from '../connection.ts';
import type { NodeSimilarityOptions, KnnOptions } from '../algorithms/similarity.ts';
import type { WorkerRequest, WorkerResponse, SerializedError } from './index.ts';

/** Dispatch the request to the matching synchronous algorithm. */
function dispatch(conn: Connection, request: WorkerRequest): unknown {
  const { algorithm, args } = request;
  switch (algorithm) {
    case 'pagerank':
      // `undefined` positional args fall through to the sync defaults (0.85 / 20).
      return pagerank(conn, args[0] as number | undefined, args[1] as number | undefined);
    case 'louvain':
      return louvain(conn, args[0] as number | undefined);
    case 'betweennessCentrality':
      return betweennessCentrality(conn);
    case 'apsp':
      return allPairsShortestPath(conn);
    case 'nodeSimilarity':
      return nodeSimilarity(conn, args[0] as NodeSimilarityOptions | undefined);
    case 'knn':
      return knn(conn, args[0] as string, args[1] as KnnOptions | undefined);
    default: {
      const exhaustive: never = algorithm;
      throw new Error(`Unknown async algorithm: ${String(exhaustive)}`);
    }
  }
}

/** Flatten a thrown value to a clone-safe payload. */
function serializeError(error: unknown): SerializedError {
  if (error instanceof Error) {
    const code = (error as { code?: unknown }).code;
    return {
      name: error.name,
      message: error.message,
      code: typeof code === 'string' ? code : undefined,
    };
  }
  return { name: 'Error', message: String(error) };
}

/** Open a private connection, run the algorithm, and always close it. */
async function run(): Promise<WorkerResponse> {
  const request = workerData as WorkerRequest;
  const conn = connect(
    request.database,
    request.extensionPath ? { extensionPath: request.extensionPath } : {},
  );
  try {
    const result = dispatch(conn, request);
    return { ok: true, result };
  } finally {
    conn.close();
  }
}

run().then(
  (response) => parentPort?.postMessage(response),
  (error: unknown) =>
    parentPort?.postMessage({ ok: false, error: serializeError(error) } satisfies WorkerResponse),
);
