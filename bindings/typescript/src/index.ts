// GraphQLite TypeScript bindings — public entry point.
//
// The driver-facing round-trip (`connect`, `wrap`, `Connection`) lands here with
// F-07 (#7). Higher-level surfaces (`graph()`, `graphs()`) arrive in later
// elements; see docs/internal/typescript-bindings-design.md §6.
export {
  connect,
  wrap,
  Connection,
  type ConnectOptions,
  type ConnectionOptions,
} from './connection.ts';
export { graph, Graph, type GraphOptions } from './graph/index.ts';
export { hasNode, getNode, deleteNode, getAllNodes, upsertNode } from './graph/nodes.ts';
export { hasEdge, getEdge, deleteEdge, getAllEdges, upsertEdge } from './graph/edges.ts';
export {
  nodeDegree,
  getNeighbors,
  getNodeEdges,
  getEdgesFrom,
  getEdgesTo,
  getEdgesByType,
  stats,
  query,
} from './graph/queries.ts';
export {
  loadGraph,
  unloadGraph,
  reloadGraph,
  graphLoaded,
  type CacheStatus,
} from './graph/cache.ts';
export {
  pagerank,
  degreeCentrality,
  betweennessCentrality,
  closenessCentrality,
  eigenvectorCentrality,
  betweenness,
  closeness,
  type CentralityScore,
  type DegreeCentralityResult,
} from './algorithms/centrality.ts';
export { communityDetection, louvain, type CommunityResult } from './algorithms/community.ts';
export {
  weaklyConnectedComponents,
  stronglyConnectedComponents,
  wcc,
  connectedComponents,
  scc,
  type ComponentResult,
} from './algorithms/components.ts';
export {
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
} from './algorithms/paths.ts';
export {
  bfs,
  dfs,
  breadthFirstSearch,
  depthFirstSearch,
  type TraversalResult,
  type TraversalOptions,
} from './algorithms/traversal.ts';
export {
  nodeSimilarity,
  knn,
  triangleCount,
  triangles,
  type NodeSimilarityPair,
  type KnnNeighbor,
  type TriangleCount,
  type NodeSimilarityOptions,
  type KnnOptions,
} from './algorithms/similarity.ts';
export {
  upsertNodesBatch,
  upsertEdgesBatch,
  type NodeBatchItem,
  type EdgeBatchItem,
} from './graph/batch.ts';
export {
  insertNodesBulk,
  insertEdgesBulk,
  insertGraphBulk,
  resolveNodeIds,
  type BulkNodeItem,
  type BulkEdgeItem,
  type BulkInsertResult,
} from './graph/bulk.ts';
export { GraphManager, graphs, type GraphManagerOptions } from './manager.ts';
export {
  GraphQLiteError,
  ParseError,
  ValidationError,
  ExecutionError,
  UnsupportedOperationError,
  ExtensionLoadError,
} from './errors.ts';
export { VERSION } from './version.ts';
