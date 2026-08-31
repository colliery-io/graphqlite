// Centrality algorithms as pure functions.
//
// The algorithm family uses **no parameter binding** — numeric arguments are
// interpolated straight into the Cypher string. Because of that every numeric
// argument is validated to be finite first (a NaN/Infinity/non-number would
// otherwise land unescaped in the query). Mirrors
// bindings/python/src/graphqlite/algorithms/centrality.py.
import type { Connection } from '../connection.ts';
import { extractAlgoArray, safeFloat, safeInt, type AlgoRow } from './parsing.ts';
import { ValidationError } from '../errors.ts';

/** `{ nodeId, userId, score }` — pagerank / betweenness / closeness / eigenvector. */
export interface CentralityScore {
  nodeId: string;
  userId: unknown;
  score: number;
}

/** `{ nodeId, userId, inDegree, outDegree, degree }` — degreeCentrality. */
export interface DegreeCentralityResult {
  nodeId: string;
  userId: unknown;
  inDegree: number;
  outDegree: number;
  degree: number;
}

/** Guard an interpolated numeric argument — it goes into the query unescaped. */
function assertFinite(value: number, name: string): void {
  if (typeof value !== 'number' || !Number.isFinite(value)) {
    throw new ValidationError(`${name} must be a finite number, got ${JSON.stringify(value)}`, {
      code: 'VALIDATION_ERROR',
    });
  }
}

function algoRows(conn: Connection, cypher: string): AlgoRow[] {
  return extractAlgoArray(conn.cypher(cypher).toList());
}

/**
 * PageRank. **Uniquely** filters on `score !== null` in addition to
 * `nodeId !== null` (the other centrality methods only check `nodeId`) — this
 * asymmetry is reproduced verbatim from centrality.py:pagerank.
 */
export function pagerank(
  conn: Connection,
  damping = 0.85,
  iterations = 20,
): CentralityScore[] {
  assertFinite(damping, 'damping');
  assertFinite(iterations, 'iterations');
  const rows = algoRows(conn, `RETURN pageRank(${damping}, ${iterations})`);
  const ranks: CentralityScore[] = [];
  for (const row of rows) {
    const nodeId = row['node_id'];
    const score = row['score'];
    if (nodeId != null && score != null) {
      ranks.push({ nodeId: String(nodeId), userId: row['user_id'], score: safeFloat(score) });
    }
  }
  return ranks;
}

/** In/out/total degree per node. Only checks `nodeId !== null`. */
export function degreeCentrality(conn: Connection): DegreeCentralityResult[] {
  const rows = algoRows(conn, 'RETURN degreeCentrality()');
  const degrees: DegreeCentralityResult[] = [];
  for (const row of rows) {
    const nodeId = row['node_id'];
    if (nodeId != null) {
      degrees.push({
        nodeId: String(nodeId),
        userId: row['user_id'],
        inDegree: safeInt(row['in_degree']),
        outDegree: safeInt(row['out_degree']),
        degree: safeInt(row['degree']),
      });
    }
  }
  return degrees;
}

// Shared score-only parse for betweenness/closeness/eigenvector — `nodeId`-only
// filter (contrast pagerank's extra score check).
function scoreRows(rows: AlgoRow[]): CentralityScore[] {
  const scores: CentralityScore[] = [];
  for (const row of rows) {
    const nodeId = row['node_id'];
    if (nodeId != null) {
      scores.push({
        nodeId: String(nodeId),
        userId: row['user_id'],
        score: safeFloat(row['score']),
      });
    }
  }
  return scores;
}

/** Betweenness centrality (Brandes). */
export function betweennessCentrality(conn: Connection): CentralityScore[] {
  return scoreRows(algoRows(conn, 'RETURN betweennessCentrality()'));
}

/** Closeness centrality (harmonic variant). */
export function closenessCentrality(conn: Connection): CentralityScore[] {
  return scoreRows(algoRows(conn, 'RETURN closenessCentrality()'));
}

/** Eigenvector centrality (power iteration). No damping factor. */
export function eigenvectorCentrality(conn: Connection, iterations = 100): CentralityScore[] {
  assertFinite(iterations, 'iterations');
  return scoreRows(algoRows(conn, `RETURN eigenvectorCentrality(${iterations})`));
}

/** Alias for {@link betweennessCentrality} (matches Python's `betweenness`). */
export const betweenness = betweennessCentrality;

/** Alias for {@link closenessCentrality} (matches Python's `closeness`). */
export const closeness = closenessCentrality;
