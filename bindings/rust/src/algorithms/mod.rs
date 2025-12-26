//! Graph algorithm implementations and result types.

mod centrality;
mod community;
mod components;
pub(crate) mod parsing;
mod paths;
mod similarity;
mod traversal;

use serde::{Deserialize, Serialize};

/// PageRank result for a single node.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PageRankResult {
    /// Internal node identifier.
    pub node_id: String,
    /// User-defined node identifier (from the `id` property).
    pub user_id: Option<String>,
    /// PageRank score (higher = more important).
    pub score: f64,
}

/// Community detection result for a single node.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CommunityResult {
    /// Internal node identifier.
    pub node_id: String,
    /// User-defined node identifier (from the `id` property).
    pub user_id: Option<String>,
    /// Community label (nodes with the same value belong to the same community).
    pub community: i64,
}

/// Shortest path result from Dijkstra's algorithm.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ShortestPathResult {
    /// List of node IDs along the path (source to target).
    pub path: Vec<String>,
    /// Total distance/cost of the path (`None` if no path found).
    pub distance: Option<f64>,
    /// Whether a path was found.
    pub found: bool,
}

/// Degree centrality result for a single node.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DegreeCentralityResult {
    /// Internal node identifier.
    pub node_id: String,
    /// User-defined node identifier (from the `id` property).
    pub user_id: Option<String>,
    /// Number of incoming edges.
    pub in_degree: i64,
    /// Number of outgoing edges.
    pub out_degree: i64,
    /// Total degree (in + out).
    pub degree: i64,
}

/// Connected component result for a single node.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ComponentResult {
    /// Internal node identifier.
    pub node_id: String,
    /// User-defined node identifier (from the `id` property).
    pub user_id: Option<String>,
    /// Component identifier (nodes in the same component share this value).
    pub component: i64,
}

/// Betweenness centrality result for a single node.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BetweennessCentralityResult {
    /// Internal node identifier.
    pub node_id: String,
    /// User-defined node identifier (from the `id` property).
    pub user_id: Option<String>,
    /// Betweenness centrality score (higher = more central).
    pub score: f64,
}

/// Closeness centrality result for a single node.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ClosenessCentralityResult {
    /// Internal node identifier.
    pub node_id: String,
    /// User-defined node identifier (from the `id` property).
    pub user_id: Option<String>,
    /// Closeness centrality score (0 to 1, higher = more central).
    pub score: f64,
}

/// Triangle count result for a single node.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TriangleCountResult {
    /// Internal node identifier.
    pub node_id: String,
    /// User-defined node identifier (from the `id` property).
    pub user_id: Option<String>,
    /// Number of triangles this node participates in.
    pub triangles: i64,
    /// Local clustering coefficient (0 to 1).
    pub clustering_coefficient: f64,
}

/// A* shortest path result.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AStarResult {
    /// List of node IDs along the path (source to target).
    pub path: Vec<String>,
    /// Total distance/cost of the path (`None` if no path found).
    pub distance: Option<f64>,
    /// Whether a path was found.
    pub found: bool,
    /// Number of nodes explored during search.
    pub nodes_explored: i64,
}

/// Traversal result for BFS/DFS.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TraversalResult {
    /// User-defined node identifier (from the `id` property).
    pub user_id: String,
    /// Depth/distance from the starting node.
    pub depth: i64,
    /// Order in which the node was visited.
    pub order: i64,
}

/// Node similarity result using Jaccard coefficient.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NodeSimilarityResult {
    /// First node's user-defined identifier.
    pub node1: String,
    /// Second node's user-defined identifier.
    pub node2: String,
    /// Jaccard similarity score (0.0 to 1.0).
    pub similarity: f64,
}

/// K-nearest neighbor result.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KnnResult {
    /// Neighbor node's user-defined identifier.
    pub neighbor: String,
    /// Jaccard similarity score (0.0 to 1.0).
    pub similarity: f64,
    /// Rank (1 = most similar).
    pub rank: i64,
}

/// Eigenvector centrality result for a single node.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EigenvectorCentralityResult {
    /// Internal node identifier.
    pub node_id: String,
    /// User-defined node identifier (from the `id` property).
    pub user_id: Option<String>,
    /// Eigenvector centrality score (higher = more central).
    pub score: f64,
}

/// All pairs shortest path result for a single pair.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ApspResult {
    /// Source node's user-defined identifier.
    pub source: String,
    /// Target node's user-defined identifier.
    pub target: String,
    /// Shortest path distance between source and target.
    pub distance: f64,
}
