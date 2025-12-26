//! Edge operations for Graph.

use crate::utils::{escape_string, format_value, sanitize_rel_type};
use crate::{CypherResult, Result, Value};
use super::Graph;

impl Graph {
    /// Check if a directed edge exists from source to target.
    pub fn has_edge(&self, source_id: &str, target_id: &str) -> Result<bool> {
        let query = format!(
            "MATCH (a {{id: '{}'}})-[r]->(b {{id: '{}'}}) RETURN count(r) AS cnt",
            escape_string(source_id),
            escape_string(target_id)
        );
        let result = self.connection().cypher(&query)?;
        if result.is_empty() {
            return Ok(false);
        }
        let cnt: i64 = result[0].get("cnt").unwrap_or(0);
        Ok(cnt > 0)
    }

    /// Get edge properties between two nodes.
    pub fn get_edge(&self, source_id: &str, target_id: &str) -> Result<Option<Value>> {
        let query = format!(
            "MATCH (a {{id: '{}'}})-[r]->(b {{id: '{}'}}) RETURN r",
            escape_string(source_id),
            escape_string(target_id)
        );
        let result = self.connection().cypher(&query)?;
        if result.is_empty() {
            return Ok(None);
        }
        Ok(result[0].get_value("r").cloned())
    }

    /// Create an edge between two nodes.
    ///
    /// If an edge already exists, this is a no-op.
    pub fn upsert_edge<I, K, V>(
        &self,
        source_id: &str,
        target_id: &str,
        props: I,
        rel_type: &str,
    ) -> Result<()>
    where
        I: IntoIterator<Item = (K, V)>,
        K: AsRef<str>,
        V: AsRef<str>,
    {
        if self.has_edge(source_id, target_id)? {
            return Ok(());
        }

        let safe_rel_type = sanitize_rel_type(rel_type);
        let esc_source = escape_string(source_id);
        let esc_target = escape_string(target_id);

        let props: Vec<(String, String)> = props
            .into_iter()
            .map(|(k, v)| (k.as_ref().to_string(), v.as_ref().to_string()))
            .collect();

        let query = if props.is_empty() {
            format!(
                "MATCH (a {{id: '{}'}}), (b {{id: '{}'}}) CREATE (a)-[r:{}]->(b)",
                esc_source, esc_target, safe_rel_type
            )
        } else {
            let prop_parts: Vec<String> = props
                .iter()
                .map(|(k, v)| format!("{}: {}", k, format_value(v)))
                .collect();
            let prop_str = prop_parts.join(", ");
            format!(
                "MATCH (a {{id: '{}'}}), (b {{id: '{}'}}) CREATE (a)-[r:{} {{{}}}]->(b)",
                esc_source, esc_target, safe_rel_type, prop_str
            )
        };

        self.connection().cypher(&query)?;
        Ok(())
    }

    /// Delete the directed edge between two nodes.
    pub fn delete_edge(&self, source_id: &str, target_id: &str) -> Result<()> {
        let query = format!(
            "MATCH (a {{id: '{}'}})-[r]->(b {{id: '{}'}}) DELETE r",
            escape_string(source_id),
            escape_string(target_id)
        );
        self.connection().cypher(&query)?;
        Ok(())
    }

    /// Get all edges in the graph.
    pub fn get_all_edges(&self) -> Result<CypherResult> {
        self.connection().cypher("MATCH (a)-[r]->(b) RETURN a.id AS source, b.id AS target, r")
    }
}
