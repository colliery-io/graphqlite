// The Graph facade — a thin front over the driver connection.
//
// Python composes Graph from 12 mixins via multiple inheritance
// (bindings/python/src/graphqlite/graph/__init__.py). Mirroring that in TS
// makes the types messy and, worse, funnels every feature into one Graph file —
// a parallel-development bottleneck. Instead each feature module exports **pure
// functions** taking a Connection first, and Graph delegates in three lines:
//
//   // src/graph/nodes.ts
//   export function hasNode(conn: Connection, nodeId: string): boolean { ... }
//   // here
//   hasNode(nodeId: string) { return hasNode(this.#conn, nodeId); }
//
// This element (G-01) is the **skeleton only**. Delegation methods and the cache
// helpers (load/unload/reload — those land with C-01 #14) are added by later
// elements at the marked insertion points below.
import { connect, type Connection, type ConnectionOptions } from '../connection.ts';
import type { CypherRow, CypherValue } from '../result.ts';
import { hasNode, getNode, deleteNode, getAllNodes, upsertNode } from './nodes.ts';
import { hasEdge, getEdge, deleteEdge, getAllEdges, upsertEdge } from './edges.ts';
import {
  nodeDegree,
  getNeighbors,
  getNodeEdges,
  getEdgesFrom,
  getEdgesTo,
  getEdgesByType,
  stats,
  query,
} from './queries.ts';
import { loadGraph, unloadGraph, reloadGraph, graphLoaded, type CacheStatus } from './cache.ts';
import {
  pagerank,
  degreeCentrality,
  betweennessCentrality,
  closenessCentrality,
  eigenvectorCentrality,
  betweenness,
  closeness,
  type CentralityScore,
  type DegreeCentralityResult,
} from '../algorithms/centrality.ts';
import {
  communityDetection,
  louvain,
  type CommunityResult,
} from '../algorithms/community.ts';
import {
  weaklyConnectedComponents,
  stronglyConnectedComponents,
  wcc,
  connectedComponents,
  scc,
  type ComponentResult,
} from '../algorithms/components.ts';
import {
  shortestPath,
  astar,
  allPairsShortestPath,
  dijkstra,
  aStar,
  apsp,
  type ShortestPathResult,
  type AStarResult,
  type AllPairsPath,
  type ShortestPathOptions,
  type AStarOptions,
} from '../algorithms/paths.ts';
import {
  bfs,
  dfs,
  breadthFirstSearch,
  depthFirstSearch,
  type TraversalResult,
  type TraversalOptions,
} from '../algorithms/traversal.ts';
import {
  nodeSimilarity,
  knn,
  triangleCount,
  triangles,
  type NodeSimilarityPair,
  type KnnNeighbor,
  type TriangleCount,
  type NodeSimilarityOptions,
  type KnnOptions,
} from '../algorithms/similarity.ts';
import {
  upsertNodesBatch,
  upsertEdgesBatch,
  type NodeBatchItem,
  type EdgeBatchItem,
} from './batch.ts';
import { UnsupportedOperationError } from '../errors.ts';

export interface GraphOptions extends ConnectionOptions {
  /**
   * Namespace label for the graph. **Stored but never used in queries** — this
   * is a dead parameter in the Python binding too, reproduced here verbatim so
   * the three bindings share one surface. Do not thread it into Cypher.
   */
  namespace?: string;
}

/**
 * High-level graph interface for GraphQLite. Wraps a {@link Connection} and, in
 * later elements, delegates node/edge/query/algorithm calls to pure feature
 * functions. Disposable via `using` (see {@link graph}).
 */
export class Graph {
  readonly #conn: Connection;

  /** The namespace passed at construction. Not used by any query (see {@link GraphOptions.namespace}). */
  readonly namespace: string;

  constructor(dbPath: string = ':memory:', options: GraphOptions = {}) {
    const { namespace = 'default', ...connectionOptions } = options;
    this.#conn = connect(dbPath, connectionOptions);
    this.namespace = namespace;
  }

  /** The underlying driver connection. */
  get connection(): Connection {
    return this.#conn;
  }

  /** Close the database connection. */
  close(): void {
    this.#conn.close();
  }

  /** Enables `using g = graph(...)` — disposes by closing the connection. */
  [Symbol.dispose](): void {
    this.close();
  }

  // ──────────────────────────────────────────────────────────────────────────
  // Delegation insertion points — Python Graph MRO order.
  // Each later element adds ONLY its own three-line delegations here, in place,
  // so the facade stays diff-friendly and merge conflicts stay local.
  //
  //   nodes       — #9  [N-01] 노드 읽기·삭제  ← 아래 구현됨
  //   edges       — #11 [E-01] 엣지 읽기·삭제  ← 아래 구현됨
  //   queries     — #13 [Q-01] 그래프 조회 8종 (queries.ts)  ← 아래 구현됨
  //   batch       — (batch ops)                     [+ cache 4종 → #14 C-01 ← 아래 구현됨]
  //   centrality  — #15 [A-01] 중심성 알고리즘 5종  ← 아래 구현됨
  //   community   — #16 [A-02] 커뮤니티 탐지  ← 아래 구현됨
  //   components  — #17 [A-03] 연결 요소 (WCC/SCC)  ← 아래 구현됨
  //   paths       — #18 [A-04] 경로 알고리즘 (dijkstra/astar/apsp)  ← 아래 구현됨
  //   traversal   — #19 [A-05] 순회 (BFS/DFS)
  //   similarity  — #20 [A-06] 유사도 (nodeSimilarity/knn/triangleCount)
  // ──────────────────────────────────────────────────────────────────────────

  // ── nodes — #9 [N-01] ──────────────────────────────────────────────────────
  /** Whether a node with the given `id` exists. */
  hasNode(nodeId: string): boolean {
    return hasNode(this.#conn, nodeId);
  }

  /** Fetch a node by `id`, or `null` if none (returned unmodified). */
  getNode(nodeId: string): CypherValue | null {
    return getNode(this.#conn, nodeId);
  }

  /** Delete a node and its relationships. */
  deleteNode(nodeId: string): void {
    deleteNode(this.#conn, nodeId);
  }

  /** All nodes, optionally filtered by `label` (validated as an identifier). */
  getAllNodes(label?: string): CypherValue[] {
    return getAllNodes(this.#conn, label);
  }

  /** Create a node, or update an existing one (dispatched on `hasNode`). */
  upsertNode(nodeId: string, nodeData: Record<string, unknown>, label?: string): void {
    upsertNode(this.#conn, nodeId, nodeData, label);
  }

  // ── edges — #11 [E-01] ─────────────────────────────────────────────────────
  /** Whether an edge exists between two nodes (optionally of `relType`). */
  hasEdge(sourceId: string, targetId: string, relType?: string): boolean {
    return hasEdge(this.#conn, sourceId, targetId, relType);
  }

  /** Fetch the edge between two nodes, or `null` if none (returned unmodified). */
  getEdge(sourceId: string, targetId: string, relType?: string): CypherValue | null {
    return getEdge(this.#conn, sourceId, targetId, relType);
  }

  /** Delete the edge(s) between two nodes (optionally of `relType`). */
  deleteEdge(sourceId: string, targetId: string, relType?: string): void {
    deleteEdge(this.#conn, sourceId, targetId, relType);
  }

  /** All edges as `{ source, target, r }` rows. */
  getAllEdges(): CypherRow[] {
    return getAllEdges(this.#conn);
  }

  /** Create or update an edge via MERGE (no-op if either node is missing). */
  upsertEdge(
    sourceId: string,
    targetId: string,
    edgeData: Record<string, unknown>,
    relType?: string,
    edgeId?: string,
  ): void {
    upsertEdge(this.#conn, sourceId, targetId, edgeData, relType, edgeId);
  }

  // ── queries — #13 [Q-01] ───────────────────────────────────────────────────
  /** Degree (in + out) of a node. */
  nodeDegree(nodeId: string): number {
    return nodeDegree(this.#conn, nodeId);
  }

  /** Distinct neighbouring nodes (either direction). */
  getNeighbors(nodeId: string): CypherValue[] {
    return getNeighbors(this.#conn, nodeId);
  }

  /** Edges touching a node as `[source, target, props]` tuples. */
  getNodeEdges(nodeId: string): [string, string, Record<string, unknown>][] {
    return getNodeEdges(this.#conn, nodeId);
  }

  /** Outgoing edges from a node as `{ source, target, r }` rows. */
  getEdgesFrom(nodeId: string): CypherRow[] {
    return getEdgesFrom(this.#conn, nodeId);
  }

  /** Incoming edges to a node as `{ source, target, r }` rows. */
  getEdgesTo(nodeId: string): CypherRow[] {
    return getEdgesTo(this.#conn, nodeId);
  }

  /** Outgoing edges of a given type from a node. */
  getEdgesByType(nodeId: string, relType: string): CypherRow[] {
    return getEdgesByType(this.#conn, nodeId, relType);
  }

  /** Node/edge counts as `{ nodeCount, edgeCount }`. */
  stats(): { nodeCount: number; edgeCount: number } {
    return stats(this.#conn);
  }

  /** Run a raw Cypher query, passing the caller's string through unchanged. */
  query(cypher: string, params?: Record<string, unknown> | null): CypherRow[] {
    return query(this.#conn, cypher, params);
  }

  // ── cache — #14 [C-01] ─────────────────────────────────────────────────────
  /** Load the graph into the in-memory CSR cache (renames nodes/edges). */
  loadGraph(): CacheStatus {
    return loadGraph(this.#conn);
  }

  /** Free the cached graph (raw status, no rename). */
  unloadGraph(): CacheStatus {
    return unloadGraph(this.#conn);
  }

  /** Reload the cache with the latest data (renames nodes/edges). */
  reloadGraph(): CacheStatus {
    return reloadGraph(this.#conn);
  }

  /** Whether the cache is currently loaded. */
  graphLoaded(): boolean {
    return graphLoaded(this.#conn);
  }

  // ── centrality — #15 [A-01] ────────────────────────────────────────────────
  /** PageRank (filters on score !== null, unlike the others). */
  pagerank(damping?: number, iterations?: number): CentralityScore[] {
    return pagerank(this.#conn, damping, iterations);
  }

  /** In/out/total degree per node. */
  degreeCentrality(): DegreeCentralityResult[] {
    return degreeCentrality(this.#conn);
  }

  /** Betweenness centrality (Brandes). */
  betweennessCentrality(): CentralityScore[] {
    return betweennessCentrality(this.#conn);
  }

  /** Closeness centrality (harmonic variant). */
  closenessCentrality(): CentralityScore[] {
    return closenessCentrality(this.#conn);
  }

  /** Eigenvector centrality (power iteration). */
  eigenvectorCentrality(iterations?: number): CentralityScore[] {
    return eigenvectorCentrality(this.#conn, iterations);
  }

  /** Alias for {@link Graph.betweennessCentrality}. */
  betweenness(): CentralityScore[] {
    return betweenness(this.#conn);
  }

  /** Alias for {@link Graph.closenessCentrality}. */
  closeness(): CentralityScore[] {
    return closeness(this.#conn);
  }

  // ── community — #16 [A-02] ─────────────────────────────────────────────────
  /** Label-propagation community detection (checks nodeId AND community). */
  communityDetection(iterations?: number): CommunityResult[] {
    return communityDetection(this.#conn, iterations);
  }

  /** Louvain community detection (checks nodeId only). */
  louvain(resolution?: number): CommunityResult[] {
    return louvain(this.#conn, resolution);
  }

  /**
   * **Not implemented in the TypeScript binding.** Python's `leiden_communities`
   * depends on graspologic, which has no JS equivalent. Always throws
   * {@link UnsupportedOperationError}; use {@link Graph.communityDetection} or
   * {@link Graph.louvain} instead.
   */
  leidenCommunities(): never {
    throw new UnsupportedOperationError(
      'leidenCommunities is not available in the TypeScript binding: it relies on ' +
        "Python-only 'graspologic'. Use communityDetection() or louvain() instead.",
    );
  }

  // ── components — #17 [A-03] ────────────────────────────────────────────────
  /** Weakly connected components (undirected). */
  weaklyConnectedComponents(): ComponentResult[] {
    return weaklyConnectedComponents(this.#conn);
  }

  /** Strongly connected components (direction-aware). */
  stronglyConnectedComponents(): ComponentResult[] {
    return stronglyConnectedComponents(this.#conn);
  }

  /** Alias for {@link Graph.weaklyConnectedComponents}. */
  wcc(): ComponentResult[] {
    return wcc(this.#conn);
  }

  /** Alias for {@link Graph.weaklyConnectedComponents}. */
  connectedComponents(): ComponentResult[] {
    return connectedComponents(this.#conn);
  }

  /** Alias for {@link Graph.stronglyConnectedComponents}. */
  scc(): ComponentResult[] {
    return scc(this.#conn);
  }

  // ── paths — #18 [A-04] ─────────────────────────────────────────────────────
  /** Dijkstra shortest path (double quotes, unwraps column_0). */
  shortestPath(
    sourceId: string,
    targetId: string,
    options?: ShortestPathOptions,
  ): ShortestPathResult {
    return shortestPath(this.#conn, sourceId, targetId, options);
  }

  /** A* shortest path (single quotes, no column_0 unwrap — Python inconsistency kept). */
  astar(sourceId: string, targetId: string, options?: AStarOptions): AStarResult {
    return astar(this.#conn, sourceId, targetId, options);
  }

  /** All-pairs shortest path (Floyd–Warshall). */
  allPairsShortestPath(): AllPairsPath[] {
    return allPairsShortestPath(this.#conn);
  }

  /** Alias for {@link Graph.shortestPath}. */
  dijkstra(sourceId: string, targetId: string, options?: ShortestPathOptions): ShortestPathResult {
    return dijkstra(this.#conn, sourceId, targetId, options);
  }

  /** Alias for {@link Graph.astar}. */
  aStar(sourceId: string, targetId: string, options?: AStarOptions): AStarResult {
    return aStar(this.#conn, sourceId, targetId, options);
  }

  /** Alias for {@link Graph.allPairsShortestPath}. */
  apsp(): AllPairsPath[] {
    return apsp(this.#conn);
  }

  // ── traversal — #19 [A-05] ─────────────────────────────────────────────────
  /** Breadth-first search from a node (`maxDepth < 0` = unlimited). */
  bfs(startId: string, options?: TraversalOptions): TraversalResult[] {
    return bfs(this.#conn, startId, options);
  }

  /** Depth-first search from a node (`maxDepth < 0` = unlimited). */
  dfs(startId: string, options?: TraversalOptions): TraversalResult[] {
    return dfs(this.#conn, startId, options);
  }

  /** Alias for {@link Graph.bfs}. */
  breadthFirstSearch(startId: string, options?: TraversalOptions): TraversalResult[] {
    return breadthFirstSearch(this.#conn, startId, options);
  }

  /** Alias for {@link Graph.dfs}. */
  depthFirstSearch(startId: string, options?: TraversalOptions): TraversalResult[] {
    return depthFirstSearch(this.#conn, startId, options);
  }

  // ── similarity — #20 [A-06] ────────────────────────────────────────────────
  /** Node similarity (Jaccard, 4-way branch; does NOT use extractAlgoArray). */
  nodeSimilarity(options?: NodeSimilarityOptions): NodeSimilarityPair[] {
    return nodeSimilarity(this.#conn, options);
  }

  /** K-nearest neighbors by Jaccard similarity. */
  knn(nodeId: string, options?: KnnOptions): KnnNeighbor[] {
    return knn(this.#conn, nodeId, options);
  }

  /** Triangle count + local clustering coefficient per node. */
  triangleCount(): TriangleCount[] {
    return triangleCount(this.#conn);
  }

  /** Alias for {@link Graph.triangleCount}. */
  triangles(): TriangleCount[] {
    return triangles(this.#conn);
  }

  // ── batch — #21 [B-01] ─────────────────────────────────────────────────────
  /** Upsert many nodes (non-atomic loop over upsertNode). */
  upsertNodesBatch(nodes: NodeBatchItem[]): void {
    upsertNodesBatch(this.#conn, nodes);
  }

  /** Upsert many edges (non-atomic; no edgeId, so no parallel edges). */
  upsertEdgesBatch(edges: EdgeBatchItem[]): void {
    upsertEdgesBatch(this.#conn, edges);
  }
}

/**
 * Create a new {@link Graph}. Factory matching the style of `connect()`.
 *
 * @param dbPath  Path to a database file, or `:memory:` (the default).
 * @param options `namespace` (stored, unused) and connection/extension options.
 */
export function graph(dbPath: string = ':memory:', options: GraphOptions = {}): Graph {
  return new Graph(dbPath, options);
}
