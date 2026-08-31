# TypeScript API Reference

Version: **0.6.1**

Package: `graphqlite`

---

The TypeScript binding loads the GraphQLite SQLite extension through Node's
built-in `node:sqlite` driver and round-trips the `cypher()` UDF. It targets
modern Node (the driver is `node:sqlite`'s experimental `DatabaseSync`), and all
public symbols are re-exported from the package entry point — only the names
listed on `bindings/typescript/src/index.ts` are part of the public surface.

## Function-based surface

Where the Python binding hangs every operation off `Graph` methods, the
TypeScript binding exposes a **function-based surface**: each node/edge/query/
algorithm operation is a standalone function whose first argument is a
{@link Connection}, e.g. `upsertNode(conn, id, data, label)`. The {@link Graph}
class is a thin facade that delegates to those functions in three lines, so
`graph.upsertNode(id, data, label)` and `upsertNode(graph.connection, id, data,
label)` are equivalent. Both the functions and the `Graph` methods are public;
the entries below show the exported function signature and note the `Graph`
method form.

Behaviour was ported to be byte-compatible with the Python binding, quirks
included, with a few deliberate divergences — identifier validation, a
`leidenCommunities` stub, and `GraphManager.query` auto-detection — collected in
[Divergences from the Python binding](#divergences-from-the-python-binding).

---

## Connection

### Module Functions

#### `connect`

```ts
function connect(database?: string, options?: ConnectOptions): Connection
```

Open a GraphQLite database connection. `database` defaults to `':memory:'`.
`options.extensionPath` overrides extension auto-detection; `options.database`
forwards extra `node:sqlite` `DatabaseSync` options (`allowExtension` is forced
on).

```ts
import { connect } from 'graphqlite';

const conn = connect(':memory:');
conn.cypher("CREATE (:Person {name: 'Alice', age: 30})");
conn.close();
```

#### `wrap`

```ts
function wrap(db: DatabaseSync, options?: ConnectionOptions): Connection
```

Wrap an existing `node:sqlite` `DatabaseSync` with GraphQLite support. The
passed database must have been created with `allowExtension: true`.

```ts
import { DatabaseSync } from 'node:sqlite';
import { wrap } from 'graphqlite';

const db = new DatabaseSync(':memory:', { allowExtension: true });
const conn = wrap(db);
```

### `Connection`

A loaded extension plus the `cypher()` round-trip. Construct via `connect` or
`wrap`.

#### `Connection.cypher`

```ts
cypher(query: string, params?: Record<string, unknown> | null): CypherResult
```

Execute a Cypher query, optionally with `$name` parameters. Parameters are
serialized to JSON and handed to the core (they are not SQLite bindings). An
empty object `{}` takes the no-params path. Failures — whether thrown by the
driver or returned in-band as an `{"error":...}` cell — surface as the typed
error hierarchy (see [Errors](#errors)).

```ts
const result = conn.cypher(
  'MATCH (n:Person) WHERE n.age > $min RETURN n.name AS name',
  { min: 25 },
);
for (const row of result) {
  console.log(row['name']);
}
```

#### `Connection.execute`

```ts
execute(sql: string, params?: (null | number | bigint | string | Uint8Array)[]): unknown[]
```

Execute raw SQL and return the result rows (empty for non-returning
statements). This is the escape hatch used by the cache functions, which call
scalar SQL UDFs rather than `cypher()`.

```ts
const rows = conn.execute('SELECT gql_graph_loaded() AS status');
```

#### `Connection.commit`

```ts
commit(): void
```

Commit the active transaction. A no-op under autocommit (matches Python).

#### `Connection.rollback`

```ts
rollback(): void
```

Roll back the active transaction. A no-op under autocommit.

#### `Connection.close`

```ts
close(): void
```

Close the underlying database connection.

#### `Connection.database`

```ts
get database(): DatabaseSync
```

The underlying `node:sqlite` connection (escape hatch for raw driver use).

### `CypherResult`

The value returned by `Connection.cypher`. It is usable both as an
array-like and via `toList()`:

| Member | Signature | Description |
|--------|-----------|-------------|
| `length` | `get length(): number` | Number of rows |
| `columns` | `get columns(): string[]` | Column names, in first-row key order |
| index access | `result[i]` | Row at index `i` (a `Record<string, CypherValue>`) |
| iterate | `for (const row of result)` | Iterate rows |
| `toList` | `toList(): CypherRow[]` | The rows as a plain array |

```ts
const result = conn.cypher('MATCH (n:Person) RETURN n.name AS name, n.age AS age');
console.log(result.columns); // ['name', 'age']
console.log(result.length);  // row count
const rows = result.toList();
```

---

## Graph

### Graph Creation

#### `graph`

```ts
function graph(dbPath?: string, options?: GraphOptions): Graph
```

Create a {@link Graph}. `dbPath` defaults to `':memory:'`. `GraphOptions`
extends the connection options with a `namespace` field (default `'default'`)
that is **stored but never used in any query** — a dead parameter reproduced
from the Python binding.

```ts
import { graph } from 'graphqlite';

const g = graph(':memory:');
g.upsertNode('alice', { name: 'Alice', age: 30 }, 'Person');
g.close();
```

#### `Graph`

```ts
class Graph {
  constructor(dbPath?: string, options?: GraphOptions);
  get connection(): Connection;
  close(): void;
  [Symbol.dispose](): void; // enables `using g = graph(...)`
  readonly namespace: string;
}
```

High-level facade over a `Connection`. Every node/edge/query/algorithm method
delegates to the corresponding standalone function. Disposable via `using`.

```ts
import { graph } from 'graphqlite';

{
  using g = graph(':memory:'); // auto-closed at block end
  g.upsertNode('alice', { name: 'Alice' }, 'Person');
}
```

### Node Operations

#### `upsertNode`

```ts
function upsertNode(
  conn: Connection,
  nodeId: string,
  nodeData: Record<string, unknown>,
  label?: string,
): void
// Graph method: graph.upsertNode(nodeId, nodeData, label?)
```

Create a node or update an existing one (dispatched on `hasNode`). `label`
defaults to `'Entity'` and is used only on creation. The two paths are
asymmetric: **create** merges `{ id: nodeId, ...nodeData }` (a `nodeData.id`
overwrites `nodeId`) in a single interpolated `CREATE`; **update** SETs only the
`nodeData` entries (leaving `id` untouched), one query per entry. The `label`
and every property key are validated with `assertIdentifier` — an invalid
identifier throws `ValidationError`.

```ts
g.upsertNode('alice', { name: 'Alice', age: 30 }, 'Person');
g.upsertNode('alice', { age: 31 }); // update: only age changes
```

#### `getNode`

```ts
function getNode(conn: Connection, nodeId: string): CypherValue | null
// Graph method: graph.getNode(nodeId)
```

Fetch a node by its `id` property, or `null` if none. The node cell is returned
unmodified (the `{ id, labels, properties }` shape).

```ts
const node = g.getNode('alice');
```

#### `hasNode`

```ts
function hasNode(conn: Connection, nodeId: string): boolean
// Graph method: graph.hasNode(nodeId)
```

Whether a node with the given `id` exists.

```ts
if (g.hasNode('alice')) { /* ... */ }
```

#### `deleteNode`

```ts
function deleteNode(conn: Connection, nodeId: string): void
// Graph method: graph.deleteNode(nodeId)
```

Delete a node and all its relationships (`DETACH DELETE`).

```ts
g.deleteNode('alice');
```

#### `getAllNodes`

```ts
function getAllNodes(conn: Connection, label?: string): CypherValue[]
// Graph method: graph.getAllNodes(label?)
```

Return all nodes, optionally filtered by `label`. The label is interpolated
into the Cypher, so it is validated with `assertIdentifier` first — a bad label
throws `ValidationError`.

```ts
const all = g.getAllNodes();
const people = g.getAllNodes('Person');
```

### Edge Operations

#### `upsertEdge`

```ts
function upsertEdge(
  conn: Connection,
  sourceId: string,
  targetId: string,
  edgeData: Record<string, unknown>,
  relType?: string,
  edgeId?: string,
): void
// Graph method: graph.upsertEdge(sourceId, targetId, edgeData, relType?, edgeId?)
```

Create or update an edge via `MERGE`. `relType` defaults to `'RELATED'` and is
run through `sanitizeRelType` (coerced to a safe token, never thrown). Property
keys are validated with `assertIdentifier`. Without `edgeId` the merge key is
`(source, target, relType)`; with `edgeId` it is the caller-assigned `id`
property, allowing parallel edges of the same type. If either node is missing
the operation is a silent no-op.

```ts
g.upsertEdge('alice', 'bob', { since: 2020 }, 'KNOWS');
```

#### `getEdge`

```ts
function getEdge(
  conn: Connection,
  sourceId: string,
  targetId: string,
  relType?: string,
): CypherValue | null
// Graph method: graph.getEdge(sourceId, targetId, relType?)
```

Fetch the edge between two nodes, or `null` if none. The `r` cell is returned
unmodified.

```ts
const edge = g.getEdge('alice', 'bob', 'KNOWS');
```

#### `hasEdge`

```ts
function hasEdge(
  conn: Connection,
  sourceId: string,
  targetId: string,
  relType?: string,
): boolean
// Graph method: graph.hasEdge(sourceId, targetId, relType?)
```

Whether an edge exists between two nodes (optionally of a given type).

```ts
if (g.hasEdge('alice', 'bob')) { /* ... */ }
```

#### `deleteEdge`

```ts
function deleteEdge(
  conn: Connection,
  sourceId: string,
  targetId: string,
  relType?: string,
): void
// Graph method: graph.deleteEdge(sourceId, targetId, relType?)
```

Delete the edge(s) between two nodes (optionally of a given type).

```ts
g.deleteEdge('alice', 'bob', 'KNOWS');
```

#### `getAllEdges`

```ts
function getAllEdges(conn: Connection): CypherRow[]
// Graph method: graph.getAllEdges()
```

Return all edges as `{ source, target, r }` rows.

```ts
for (const edge of g.getAllEdges()) {
  console.log(edge['source'], edge['target']);
}
```

### Query Methods

#### `query`

```ts
function query(
  conn: Connection,
  cypher: string,
  params?: Record<string, unknown> | null,
): CypherRow[]
// Graph method: graph.query(cypher, params?)
```

Run a raw Cypher query, passing the caller's string through unchanged, and
return all rows as a plain array.

```ts
const rows = g.query('MATCH (n:Person) WHERE n.age > $min RETURN n.name AS name', {
  min: 25,
});
```

#### `stats`

```ts
function stats(conn: Connection): { nodeCount: number; edgeCount: number }
// Graph method: graph.stats()
```

Node and edge counts (two count queries under the hood).

```ts
const { nodeCount, edgeCount } = g.stats();
```

#### `nodeDegree`

```ts
function nodeDegree(conn: Connection, nodeId: string): number
// Graph method: graph.nodeDegree(nodeId)
```

Total degree (in + out) of a node. Empty result → `0`.

```ts
const deg = g.nodeDegree('alice');
```

#### `getNeighbors`

```ts
function getNeighbors(conn: Connection, nodeId: string): CypherValue[]
// Graph method: graph.getNeighbors(nodeId)
```

Distinct neighbouring nodes in either direction.

```ts
const neighbors = g.getNeighbors('alice');
```

#### `getNodeEdges`

```ts
function getNodeEdges(
  conn: Connection,
  nodeId: string,
): [string, string, Record<string, unknown>][]
// Graph method: graph.getNodeEdges(nodeId)
```

Edges touching a node as `[source, target, props]` **tuples** — deliberately a
different shape from the other query methods. A missing `r` cell becomes `{}`.

```ts
for (const [source, target, props] of g.getNodeEdges('alice')) {
  console.log(source, target, props);
}
```

#### `getEdgesFrom`

```ts
function getEdgesFrom(conn: Connection, nodeId: string): CypherRow[]
// Graph method: graph.getEdgesFrom(nodeId)
```

Outgoing edges from a node as `{ source, target, r }` rows.

```ts
const out = g.getEdgesFrom('alice');
```

#### `getEdgesTo`

```ts
function getEdgesTo(conn: Connection, nodeId: string): CypherRow[]
// Graph method: graph.getEdgesTo(nodeId)
```

Incoming edges to a node as `{ source, target, r }` rows.

```ts
const incoming = g.getEdgesTo('bob');
```

#### `getEdgesByType`

```ts
function getEdgesByType(conn: Connection, nodeId: string, relType: string): CypherRow[]
// Graph method: graph.getEdgesByType(nodeId, relType)
```

Outgoing edges of a given type from a node. `relType` is sanitized, then
interpolated.

```ts
const knows = g.getEdgesByType('alice', 'KNOWS');
```

### Graph Cache

These control the in-memory CSR cache that speeds up algorithm runs. They call
scalar SQL UDFs via `execute()`, not `cypher()`. `CacheStatus` is
`Record<string, unknown>`.

#### `loadGraph`

```ts
function loadGraph(conn: Connection): CacheStatus
// Graph method: graph.loadGraph()
```

Load the graph into the in-memory cache. Remaps the core's `nodes`/`edges` keys
to `nodeCount`/`edgeCount`.

```ts
const status = g.loadGraph();
```

#### `unloadGraph`

```ts
function unloadGraph(conn: Connection): CacheStatus
// Graph method: graph.unloadGraph()
```

Free the cached graph. Returns the raw status **without** the key remap — an
asymmetry reproduced verbatim from the Python binding.

```ts
g.unloadGraph();
```

#### `reloadGraph`

```ts
function reloadGraph(conn: Connection): CacheStatus
// Graph method: graph.reloadGraph()
```

Reload the cache with the latest data. Remaps `nodes`/`edges`.

```ts
g.reloadGraph();
```

#### `graphLoaded`

```ts
function graphLoaded(conn: Connection): boolean
// Graph method: graph.graphLoaded()
```

Whether the cache is currently loaded.

```ts
if (!g.graphLoaded()) {
  g.loadGraph();
}
```

### Batch Operations

> **Non-atomicity warning:** batch methods call `upsertNode`/`upsertEdge` in a
> loop and do **not** wrap it in a transaction. If one item throws partway
> through, earlier items are already committed. The binding has no atomic bulk
> API (see [Non-ported methods](#non-ported-methods)).

#### `upsertNodesBatch`

```ts
type NodeBatchItem = [string, Record<string, unknown>, string]; // [nodeId, props, label]

function upsertNodesBatch(conn: Connection, nodes: NodeBatchItem[]): void
// Graph method: graph.upsertNodesBatch(nodes)
```

Upsert many nodes by calling `upsertNode` once per item.

```ts
g.upsertNodesBatch([
  ['alice', { name: 'Alice' }, 'Person'],
  ['bob', { name: 'Bob' }, 'Person'],
]);
```

#### `upsertEdgesBatch`

```ts
type EdgeBatchItem = [string, string, Record<string, unknown>, string]; // [source, target, props, relType]

function upsertEdgesBatch(conn: Connection, edges: EdgeBatchItem[]): void
// Graph method: graph.upsertEdgesBatch(edges)
```

Upsert many edges by calling `upsertEdge` once per item. No `edgeId` is passed,
so a batch cannot create parallel edges between the same pair — `MERGE` reuses
the existing edge.

```ts
g.upsertEdgesBatch([
  ['alice', 'bob', { since: 2020 }, 'KNOWS'],
]);
```

---

## Algorithms

All algorithm functions interpolate their numeric arguments straight into the
Cypher string (no parameter binding), so every numeric argument is validated to
be finite first — a `NaN`/`Infinity`/non-number throws `ValidationError`. Run
`loadGraph()` first for best performance.

### Centrality

Result types:

```ts
interface CentralityScore { nodeId: string; userId: unknown; score: number }
interface DegreeCentralityResult {
  nodeId: string; userId: unknown;
  inDegree: number; outDegree: number; degree: number;
}
```

#### `pagerank`

```ts
function pagerank(conn: Connection, damping?: number, iterations?: number): CentralityScore[]
// Graph method: graph.pagerank(damping?, iterations?)
```

PageRank. `damping` defaults to `0.85`, `iterations` to `20`. Uniquely filters
rows on both `nodeId` and `score` being non-null.

```ts
g.loadGraph();
for (const r of g.pagerank(0.85, 20)) {
  console.log(r.nodeId, r.score);
}
```

#### `degreeCentrality`

```ts
function degreeCentrality(conn: Connection): DegreeCentralityResult[]
// Graph method: graph.degreeCentrality()
```

In/out/total degree per node.

```ts
const degrees = g.degreeCentrality();
```

#### `betweennessCentrality`

```ts
function betweennessCentrality(conn: Connection): CentralityScore[]
// Graph method: graph.betweennessCentrality()  (alias: graph.betweenness())
```

Betweenness centrality (Brandes). Also exported as `betweenness`.

```ts
const bc = g.betweennessCentrality();
```

#### `closenessCentrality`

```ts
function closenessCentrality(conn: Connection): CentralityScore[]
// Graph method: graph.closenessCentrality()  (alias: graph.closeness())
```

Closeness centrality (harmonic variant). Also exported as `closeness`.

```ts
const cc = g.closenessCentrality();
```

#### `eigenvectorCentrality`

```ts
function eigenvectorCentrality(conn: Connection, iterations?: number): CentralityScore[]
// Graph method: graph.eigenvectorCentrality(iterations?)
```

Eigenvector centrality (power iteration). `iterations` defaults to `100`.

```ts
const ec = g.eigenvectorCentrality(100);
```

### Community

```ts
interface CommunityResult { nodeId: string; userId: unknown; community: number }
```

#### `communityDetection`

```ts
function communityDetection(conn: Connection, iterations?: number): CommunityResult[]
// Graph method: graph.communityDetection(iterations?)
```

Label-propagation community detection. `iterations` defaults to `10`. Filters
on both `nodeId` and `community` being non-null.

```ts
const communities = g.communityDetection(10);
```

#### `louvain`

```ts
function louvain(conn: Connection, resolution?: number): CommunityResult[]
// Graph method: graph.louvain(resolution?)
```

Louvain community detection. `resolution` defaults to `1.0`. Filters on
`nodeId` only.

```ts
const communities = g.louvain(1.0);
```

> **`leidenCommunities` is not implemented.** `graph.leidenCommunities()` exists
> as a stub that always throws `UnsupportedOperationError` — Python's
> `leiden_communities` relies on the Python-only `graspologic` package. Use
> `communityDetection()` or `louvain()` instead. There is no `leiden` function
> exported from the entry point.

### Components

```ts
interface ComponentResult { nodeId: string; userId: unknown; component: number }
```

#### `weaklyConnectedComponents`

```ts
function weaklyConnectedComponents(conn: Connection): ComponentResult[]
// Graph method: graph.weaklyConnectedComponents()
// Aliases: graph.wcc(), graph.connectedComponents()
```

Weakly connected components (graph treated as undirected). Also exported as
`wcc` and `connectedComponents`.

```ts
const wccResult = g.wcc();
```

#### `stronglyConnectedComponents`

```ts
function stronglyConnectedComponents(conn: Connection): ComponentResult[]
// Graph method: graph.stronglyConnectedComponents()  (alias: graph.scc())
```

Strongly connected components (direction-aware). Also exported as `scc`.

```ts
const sccResult = g.scc();
```

### Paths

```ts
interface ShortestPathResult { path: unknown[]; distance: number | null; found: boolean }
interface AStarResult extends ShortestPathResult { nodesExplored: number }
interface AllPairsPath { source: unknown; target: unknown; distance: number }
interface ShortestPathOptions { weightProp?: string }
interface AStarOptions { latProp?: string; lonProp?: string }
```

> **Reproduced Python inconsistencies:** `shortestPath` quotes identifiers with
> `"` while `astar` quotes with `'`. Both unwrap the core's `column_0` wrapper
> (with a defensive fallback to direct field access), and `allPairsShortestPath`
> unwraps via `extractAlgoArray`. (`astar` historically skipped the unwrap and
> always returned defaults; fixed in #64.) The quoting quirk is kept intact for
> byte-compatibility with the Python binding.

#### `shortestPath`

```ts
function shortestPath(
  conn: Connection,
  sourceId: string,
  targetId: string,
  options?: ShortestPathOptions,
): ShortestPathResult
// Graph method: graph.shortestPath(sourceId, targetId, options?)  (alias: graph.dijkstra(...))
```

Dijkstra shortest path. `options.weightProp` selects a weighted edge property;
omitted means hop count. Empty result → `{ path: [], distance: null, found:
false }`. Also exported as `dijkstra`.

```ts
const result = g.shortestPath('alice', 'carol', { weightProp: 'distance' });
if (result.found) {
  console.log(result.path, result.distance);
}
```

#### `astar`

```ts
function astar(
  conn: Connection,
  sourceId: string,
  targetId: string,
  options?: AStarOptions,
): AStarResult
// Graph method: graph.astar(sourceId, targetId, options?)  (alias: graph.aStar(...))
```

A* shortest path. `latProp`/`lonProp` name the coordinate properties and are
validated with `assertIdentifier`. Also exported as `aStar`.

```ts
const result = g.astar('alice', 'carol', { latProp: 'lat', lonProp: 'lon' });
```

#### `allPairsShortestPath`

```ts
function allPairsShortestPath(conn: Connection): AllPairsPath[]
// Graph method: graph.allPairsShortestPath()  (alias: graph.apsp())
```

All-pairs shortest path (Floyd–Warshall). Keeps only rows with non-null
`source` and `target`. Also exported as `apsp`.

```ts
const pairs = g.apsp();
```

### Traversal

```ts
interface TraversalResult { user_id: unknown; depth: number; order: number }
interface TraversalOptions { maxDepth?: number }
```

#### `bfs`

```ts
function bfs(conn: Connection, startId: string, options?: TraversalOptions): TraversalResult[]
// Graph method: graph.bfs(startId, options?)  (alias: graph.breadthFirstSearch(...))
```

Breadth-first search from a node. `maxDepth` defaults to `-1` (unlimited); only
`maxDepth < 0` means unlimited (`maxDepth === 0` is passed through literally).
Also exported as `breadthFirstSearch`.

```ts
for (const node of g.bfs('alice', { maxDepth: 2 })) {
  console.log(node.user_id, node.depth);
}
```

#### `dfs`

```ts
function dfs(conn: Connection, startId: string, options?: TraversalOptions): TraversalResult[]
// Graph method: graph.dfs(startId, options?)  (alias: graph.depthFirstSearch(...))
```

Depth-first search from a node. Same `maxDepth` semantics as `bfs`. Also
exported as `depthFirstSearch`.

```ts
const visited = g.dfs('alice');
```

### Similarity

```ts
interface NodeSimilarityPair { node1: unknown; node2: unknown; similarity: number }
interface KnnNeighbor { neighbor: unknown; similarity: number; rank: number }
interface TriangleCount {
  nodeId: string; userId: unknown; triangles: number; clusteringCoefficient: number;
}
interface NodeSimilarityOptions { node1?: string; node2?: string; threshold?: number; topK?: number }
interface KnnOptions { k?: number }
```

#### `nodeSimilarity`

```ts
function nodeSimilarity(conn: Connection, options?: NodeSimilarityOptions): NodeSimilarityPair[]
// Graph method: graph.nodeSimilarity(options?)
```

Node similarity (Jaccard). The query is chosen by a 4-way branch in priority
order: (1) `node1 && node2`; (2) `threshold > 0 && topK > 0`; (3) `threshold >
0`; (4) otherwise all-pairs. Note that supplying only `topK` (with `threshold
=== 0`) falls through to branch 4, i.e. `topK` is ignored — a Python
inconsistency kept intact.

```ts
const pairs = g.nodeSimilarity({ threshold: 0.5, topK: 10 });
```

#### `knn`

```ts
function knn(conn: Connection, nodeId: string, options?: KnnOptions): KnnNeighbor[]
// Graph method: graph.knn(nodeId, options?)
```

K-nearest neighbors by Jaccard similarity. `k` defaults to `10`.

```ts
const neighbors = g.knn('alice', { k: 5 });
```

#### `triangleCount`

```ts
function triangleCount(conn: Connection): TriangleCount[]
// Graph method: graph.triangleCount()  (alias: graph.triangles())
```

Triangle count and local clustering coefficient per node. Also exported as
`triangles`.

```ts
const stats = g.triangleCount();
```

---

## GraphManager

Manages many graphs in a directory, one `.db` file each, with cross-graph
queries via a shared in-memory coordinator that ATTACHes them.

### Factory and Class

#### `graphs`

```ts
function graphs(basePath: string, options?: GraphManagerOptions): GraphManager
```

Create a `GraphManager` for a directory of graphs (created if missing).

```ts
import { graphs } from 'graphqlite';

const gm = graphs('./data/graphs');
```

#### `GraphManager`

```ts
class GraphManager {
  constructor(basePath: string, options?: GraphManagerOptions);
  [Symbol.dispose](): void; // enables `using gm = graphs(...)`
}
```

Graph names are validated with `assertIdentifier` and every resolved path is
checked to stay inside `basePath` — see
[Identifier validation](#identifier-validation).

### Methods

#### `GraphManager.list`

```ts
list(): string[]
```

List graph names (without `.db`), sorted.

```ts
const names = gm.list();
```

#### `GraphManager.exists`

```ts
exists(name: string): boolean
```

Whether a graph file exists.

#### `GraphManager.create`

```ts
create(name: string): Graph
```

Create a new graph. Throws if it already exists.

```ts
const social = gm.create('social');
social.upsertNode('alice', { name: 'Alice' }, 'Person');
```

#### `GraphManager.open`

```ts
open(name: string): Graph
```

Open an existing graph (reusing the cached instance if already open). Throws if
not found.

#### `GraphManager.openOrCreate`

```ts
openOrCreate(name: string): Graph
```

Open a graph, creating it if it does not exist.

```ts
const g = gm.openOrCreate('social');
```

#### `GraphManager.drop`

```ts
drop(name: string): void
```

Delete a graph: close it if open, DETACH it, then delete the file. Throws if not
found.

#### `GraphManager.query`

```ts
query(
  cypher: string,
  graphs?: string[] | null,
  params?: Record<string, unknown> | null,
): CypherResult
```

Cross-graph Cypher query. Open graphs are committed first so their writes are
visible, then the **named** graphs are attached.

> **`graphs` auto-detection is not implemented.** Omitting `graphs` (or passing
> an empty list) attaches **nothing** — there is no detection of graph names
> from the query text. The Python docstring claims the query is auto-scanned for
> graph references, but no such code exists in either binding; the TypeScript
> binding reproduces the real behaviour and you must pass the graph list
> explicitly.

```ts
// Correct: name the graphs to attach explicitly.
const result = gm.query(
  'MATCH (n) RETURN count(n) AS total',
  ['social', 'org'],
);

// Attaches nothing — NOT auto-detected from the query.
const empty = gm.query('MATCH (social.n) RETURN social.n');
```

#### `GraphManager.querySql`

```ts
querySql(
  sql: string,
  graphs: string[],
  parameters?: (null | number | bigint | string | Uint8Array)[],
): unknown[]
```

Raw SQL across attached graphs — the power-user escape hatch. `graphs` is
required here.

```ts
const rows = gm.querySql('SELECT COUNT(*) AS n FROM social.gql_nodes', ['social']);
```

#### `GraphManager.close`

```ts
close(): void
```

Close all open graphs and the coordinator.

---

## Errors

The core reports failures as JSON of the shape `{"error": "...", "code":
"..."}`. Unlike the Python binding, which discards the `code`, the TypeScript
binding keeps it, promoting known codes to typed subclasses and preserving the
offending query on the error object.

#### `GraphQLiteError`

```ts
class GraphQLiteError extends Error {
  readonly code?: string;
  readonly query?: string;
}
```

Base class for every error surfaced by the binding.

```ts
import { GraphQLiteError } from 'graphqlite';

try {
  g.query('INVALID');
} catch (err) {
  if (err instanceof GraphQLiteError) {
    console.error(err.code, err.query);
  }
}
```

#### `ParseError`

```ts
class ParseError extends GraphQLiteError {
  readonly line?: number;
  readonly column?: number;
}
```

`PARSE_ERROR` — the `Line N, Col M` position is parsed out when present.

#### `ValidationError`

```ts
class ValidationError extends GraphQLiteError {}
```

`VALIDATION_ERROR` — identifier/validation failures, including the binding's own
`assertIdentifier` and finite-number checks.

#### `ExecutionError`

```ts
class ExecutionError extends GraphQLiteError {}
```

`EXECUTION_ERROR` — failure during the execution stage.

#### `UnsupportedOperationError`

```ts
class UnsupportedOperationError extends GraphQLiteError {}
```

A feature the Python binding has but this one does not implement (e.g.
`leidenCommunities`). Its `code` defaults to `'UNSUPPORTED_OPERATION'`.

#### `ExtensionLoadError`

```ts
class ExtensionLoadError extends GraphQLiteError {
  readonly searchedPaths: string[];
}
```

Client-side failure to locate/load the extension binary. Its `code` defaults to
`'EXTENSION_LOAD_ERROR'`.

---

## Utilities

The Python binding re-exports helper functions (`escape_string`,
`sanitize_rel_type`, `format_props`) and the `CYPHER_RESERVED` set at package
level. **The TypeScript binding does not.** These helpers exist internally
(`src/utils.ts`: `escapeString`, `sanitizeRelType`, `formatProps`,
`CYPHER_RESERVED`, `assertIdentifier`) and drive edge/label handling, but they
are **not re-exported from the package entry point**, so they are not part of
the public API surface. If you need Cypher literal escaping, do it through the
parameterized `cypher()`/`query()` path instead. The only utility export is the
version constant:

#### `VERSION`

```ts
const VERSION: string
```

The binding version (mirrors Python's `graphqlite.__version__`), kept in step
with `package.json`.

```ts
import { VERSION } from 'graphqlite';

console.log(VERSION); // '0.6.1'
```

---

## Divergences from the Python binding

### Identifier validation

The TypeScript binding adds an injection boundary the Python binding does not
have. Wherever an identifier is **interpolated** into Cypher (rather than bound
as a `$param`), it is checked with `assertIdentifier`, which requires a valid
unquoted identifier — Unicode-aware, so CJK labels pass — matching the regex
`^[\p{L}_][\p{L}\p{N}_]*$`, and throws `ValidationError` on anything else
(quotes, spaces, semicolons, empty string, …). This applies to:

- **node labels** (`upsertNode`, `getAllNodes`),
- **property keys** (`upsertNode`, `upsertEdge`, batch operations),
- **A\* coordinate properties** (`astar`'s `latProp`/`lonProp`),
- **graph names** in `GraphManager`, which additionally verifies the resolved
  file path stays inside `basePath`.

Python performs none of these checks and interpolates freely (it even allows
`GraphManager` names like `../x`). Two things are **not** validated-and-thrown,
to preserve Python parity: relationship types go through `sanitizeRelType`
(coerced to a safe token, never thrown), and finite-number guards on
interpolated numeric algorithm arguments throw `ValidationError` rather than
silently interpolating `NaN`/`Infinity`.

### Non-ported methods

The following Python `Graph`/module APIs are **not implemented** in the
TypeScript binding (confirmed absent from `src/index.ts`):

| Python API | Status in TypeScript |
|------------|----------------------|
| `Graph.insert_nodes_bulk` | Not ported — no atomic bulk insert; use `upsertNodesBatch` (non-atomic) |
| `Graph.insert_edges_bulk` | Not ported — use `upsertEdgesBatch` (non-atomic) |
| `Graph.insert_graph_bulk` | Not ported |
| `Graph.resolve_node_ids` | Not ported |
| `Graph.to_rustworkx` | Not ported (no JS equivalent of `rustworkx`) |
| `Graph.leiden_communities` | Present as `leidenCommunities()` stub that **throws** `UnsupportedOperationError` (Python-only `graspologic` dependency) |
| `graphqlite.load` | Not ported (no load-into-existing-connection helper) |
| `graphqlite.loadable_path` | Not ported |
| `graphqlite.escape_string` / `sanitize_rel_type` / `format_props` / `CYPHER_RESERVED` | Exist internally but **not re-exported** — not part of the public surface (see [Utilities](#utilities)) |

### `GraphManager.query` auto-detection

As described under [`GraphManager.query`](#graphmanagerquery): calling `query`
without an explicit `graphs` list attaches no databases. The Python docstring
advertises automatic detection of referenced graphs from the query text, but no
such implementation exists in either binding; the TypeScript binding reproduces
the real behaviour and requires the graph list to be passed explicitly.

---

## Example

```ts
import { graph, VERSION } from 'graphqlite';

console.log(`GraphQLite ${VERSION}`);

using g = graph(':memory:');

g.upsertNode('alice', { name: 'Alice', age: 30 }, 'Person');
g.upsertNode('bob', { name: 'Bob', age: 25 }, 'Person');
g.upsertEdge('alice', 'bob', { since: 2020 }, 'KNOWS');

const rows = g.query('MATCH (n:Person) RETURN n.name AS name, n.age AS age');
for (const row of rows) {
  console.log(row['name'], row['age']);
}

g.loadGraph();
for (const r of g.pagerank(0.85, 20)) {
  console.log(r.nodeId, r.score.toFixed(4));
}
```
