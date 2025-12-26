//! Node operations for Graph.

use crate::utils::{escape_string, format_value};
use crate::{Result, Value};
use super::Graph;

impl Graph {
    /// Check if a node with the given ID exists.
    pub fn has_node(&self, node_id: &str) -> Result<bool> {
        let query = format!(
            "MATCH (n {{id: '{}'}}) RETURN count(n) AS cnt",
            escape_string(node_id)
        );
        let result = self.connection().cypher(&query)?;
        if result.is_empty() {
            return Ok(false);
        }
        let cnt: i64 = result[0].get("cnt").unwrap_or(0);
        Ok(cnt > 0)
    }

    /// Get a node by ID.
    ///
    /// Returns the node as a [`Value`], or `None` if not found.
    pub fn get_node(&self, node_id: &str) -> Result<Option<Value>> {
        let query = format!(
            "MATCH (n {{id: '{}'}}) RETURN n",
            escape_string(node_id)
        );
        let result = self.connection().cypher(&query)?;
        if result.is_empty() {
            return Ok(None);
        }
        Ok(result[0].get_value("n").cloned())
    }

    /// Create or update a node.
    ///
    /// If a node with the given id exists, its properties are updated.
    /// Otherwise, a new node is created.
    pub fn upsert_node<I, K, V>(&self, node_id: &str, props: I, label: &str) -> Result<()>
    where
        I: IntoIterator<Item = (K, V)>,
        K: AsRef<str>,
        V: AsRef<str>,
    {
        let props: Vec<(String, String)> = props
            .into_iter()
            .map(|(k, v)| (k.as_ref().to_string(), v.as_ref().to_string()))
            .collect();

        if self.has_node(node_id)? {
            // Update existing node
            for (k, v) in props {
                let val = format_value(&v);
                let query = format!(
                    "MATCH (n {{id: '{}'}}) SET n.{} = {} RETURN n",
                    escape_string(node_id),
                    k,
                    val
                );
                self.connection().cypher(&query)?;
            }
        } else {
            // Create new node
            let mut prop_parts = vec![format!("id: '{}'", escape_string(node_id))];
            for (k, v) in props {
                prop_parts.push(format!("{}: {}", k, format_value(&v)));
            }
            let prop_str = prop_parts.join(", ");
            let query = format!("CREATE (n:{} {{{}}})", label, prop_str);
            self.connection().cypher(&query)?;
        }
        Ok(())
    }

    /// Delete a node and all its relationships.
    pub fn delete_node(&self, node_id: &str) -> Result<()> {
        let query = format!(
            "MATCH (n {{id: '{}'}}) DETACH DELETE n",
            escape_string(node_id)
        );
        self.connection().cypher(&query)?;
        Ok(())
    }

    /// Get all nodes, optionally filtered by label.
    pub fn get_all_nodes(&self, label: Option<&str>) -> Result<Vec<Value>> {
        let query = match label {
            Some(l) => format!("MATCH (n:{}) RETURN n", l),
            None => "MATCH (n) RETURN n".to_string(),
        };
        let result = self.connection().cypher(&query)?;
        let mut nodes = Vec::new();
        for row in result.iter() {
            if let Some(n) = row.get_value("n") {
                nodes.push(n.clone());
            }
        }
        Ok(nodes)
    }
}
