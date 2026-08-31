// Similarity / clustering algorithms as pure functions.
//
// Each takes a Connection first so Graph can delegate in three lines (see
// graph/index.ts). Mirrors bindings/python/src/graphqlite/algorithms/similarity.py.
//
// **This module is the one that does NOT use extractAlgoArray.** It iterates the
// CypherResult directly (`for..of`), exactly like Python's `for row in result`.
// Because the core wraps algorithm output in `{ column_0: [...] }`, direct
// iteration finds no `node1`/`neighbor`/`node_id` keys and returns empty against
// the real core — a documented Python inconsistency reproduced faithfully.
import type { Connection } from '../connection.ts';
import { safeFloat, safeInt } from './parsing.ts';
import { escapeString } from '../utils.ts';

/** `{ node1, node2, similarity }` — a Jaccard-similar node pair. */
export interface NodeSimilarityPair {
  node1: unknown;
  node2: unknown;
  similarity: number;
}

/** `{ neighbor, similarity, rank }` — one K-nearest neighbor. */
export interface KnnNeighbor {
  neighbor: unknown;
  similarity: number;
  rank: number;
}

/** `{ nodeId, userId, triangles, clusteringCoefficient }` — a node's triangle stats. */
export interface TriangleCount {
  nodeId: string;
  userId: unknown;
  triangles: number;
  clusteringCoefficient: number;
}

/** Options for {@link nodeSimilarity} — see the 4-way branch below. */
export interface NodeSimilarityOptions {
  node1?: string;
  node2?: string;
  threshold?: number;
  topK?: number;
}

/** Options for {@link knn}. */
export interface KnnOptions {
  /** Number of neighbors to return. Defaults to `10`. */
  k?: number;
}

/**
 * Node similarity (Jaccard). The query is chosen by a **4-way branch** in
 * priority order. `topK` is honored on its own: giving only `topK` (with
 * `threshold === 0`) selects branch 2 and emits `nodeSimilarity(0, topK)`.
 *
 * 1. `node1 && node2`          → `nodeSimilarity('n1', 'n2')`
 * 2. `topK>0`                  → `nodeSimilarity(threshold, topK)`
 * 3. `threshold>0`             → `nodeSimilarity(threshold)`
 * 4. else                      → `nodeSimilarity()`
 *
 * @returns Rows of `{ node1, node2, similarity }` where both nodes are non-null.
 */
export function nodeSimilarity(
  conn: Connection,
  options: NodeSimilarityOptions = {},
): NodeSimilarityPair[] {
  const { node1, node2, threshold = 0, topK = 0 } = options;

  let query: string;
  if (node1 && node2) {
    query = `RETURN nodeSimilarity('${escapeString(node1)}', '${escapeString(node2)}')`;
  } else if (topK > 0) {
    query = `RETURN nodeSimilarity(${threshold}, ${topK})`;
  } else if (threshold > 0) {
    query = `RETURN nodeSimilarity(${threshold})`;
  } else {
    query = 'RETURN nodeSimilarity()';
  }

  // Direct iteration — deliberately NOT extractAlgoArray.
  const pairs: NodeSimilarityPair[] = [];
  for (const row of conn.cypher(query)) {
    const n1 = row['node1'];
    const n2 = row['node2'];
    if (n1 != null && n2 != null) {
      pairs.push({ node1: n1, node2: n2, similarity: safeFloat(row['similarity']) });
    }
  }
  return pairs;
}

/**
 * K-nearest neighbors by Jaccard similarity.
 *
 * @returns Rows of `{ neighbor, similarity, rank }` where `neighbor` is non-null.
 */
export function knn(conn: Connection, nodeId: string, options: KnnOptions = {}): KnnNeighbor[] {
  const k = options.k ?? 10;
  const query = `RETURN knn('${escapeString(nodeId)}', ${k})`;

  const neighbors: KnnNeighbor[] = [];
  for (const row of conn.cypher(query)) {
    const neighbor = row['neighbor'];
    if (neighbor != null) {
      neighbors.push({
        neighbor,
        similarity: safeFloat(row['similarity']),
        rank: safeInt(row['rank']),
      });
    }
  }
  return neighbors;
}

/**
 * Triangle count and local clustering coefficient per node.
 *
 * @returns Rows of `{ nodeId, userId, triangles, clusteringCoefficient }` where
 *          `nodeId` is non-null.
 */
export function triangleCount(conn: Connection): TriangleCount[] {
  const triangles: TriangleCount[] = [];
  for (const row of conn.cypher('RETURN triangleCount()')) {
    const nodeId = row['node_id'];
    if (nodeId != null) {
      triangles.push({
        nodeId: String(nodeId),
        userId: row['user_id'],
        triangles: safeInt(row['triangles']),
        clusteringCoefficient: safeFloat(row['clustering_coefficient']),
      });
    }
  }
  return triangles;
}

/** Alias for {@link triangleCount} (matches Python's `triangles`). */
export const triangles = triangleCount;
