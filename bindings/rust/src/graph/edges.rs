//! Edge operations for Graph.

use super::Graph;
use crate::utils::{rel_type_pattern, sanitize_rel_type, PropertyValue};
use crate::{CypherResult, Result, Value};

impl Graph {
    /// Check if a directed edge exists from source to target.
    pub fn has_edge(
        &self,
        source_id: &str,
        target_id: &str,
        rel_type: Option<&str>,
    ) -> Result<bool> {
        let rel_pattern = rel_type_pattern(rel_type);
        let query = format!(
            "MATCH (a {{id: $src}})-[r{}]->(b {{id: $tgt}}) RETURN count(r) AS cnt",
            rel_pattern
        );
        let result = self
            .connection()
            .cypher_builder(&query)
            .param("src", source_id)
            .param("tgt", target_id)
            .run()?;
        if result.is_empty() {
            return Ok(false);
        }
        let cnt: i64 = result[0].get("cnt").unwrap_or(0);
        Ok(cnt > 0)
    }

    /// Get edge properties between two nodes.
    pub fn get_edge(
        &self,
        source_id: &str,
        target_id: &str,
        rel_type: Option<&str>,
    ) -> Result<Option<Value>> {
        let rel_pattern = rel_type_pattern(rel_type);
        let query = format!(
            "MATCH (a {{id: $src}})-[r{}]->(b {{id: $tgt}}) RETURN r",
            rel_pattern
        );
        let result = self
            .connection()
            .cypher_builder(&query)
            .param("src", source_id)
            .param("tgt", target_id)
            .run()?;
        if result.is_empty() {
            return Ok(None);
        }
        Ok(result[0].get_value("r").cloned())
    }

    /// Create or update an edge between two nodes.
    ///
    /// If an edge of the same type already exists, its properties are updated
    /// (merge semantics — existing properties not in `props` are preserved).
    /// If no edge of that type exists, a new one is created.
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
        V: Into<PropertyValue>,
    {
        let safe_rel_type = sanitize_rel_type(rel_type);

        let props: Vec<(String, PropertyValue)> = props
            .into_iter()
            .map(|(k, v)| (k.as_ref().to_string(), v.into()))
            .collect();

        let merge_query = format!(
            "MATCH (a {{id: $src}}), (b {{id: $tgt}}) MERGE (a)-[r:{}]->(b)",
            safe_rel_type
        );
        self.connection()
            .cypher_builder(&merge_query)
            .param("src", source_id)
            .param("tgt", target_id)
            .run()?;

        if !props.is_empty() {
            let set_parts: Vec<String> = props
                .iter()
                .map(|(k, v)| format!("r.{} = {}", k, v.to_cypher()))
                .collect();
            let set_str = set_parts.join(", ");
            let set_query = format!(
                "MATCH (a {{id: $src}})-[r:{}]->(b {{id: $tgt}}) SET {}",
                safe_rel_type, set_str
            );
            self.connection()
                .cypher_builder(&set_query)
                .param("src", source_id)
                .param("tgt", target_id)
                .run()?;
        }

        Ok(())
    }

    /// Delete the directed edge between two nodes.
    pub fn delete_edge(
        &self,
        source_id: &str,
        target_id: &str,
        rel_type: Option<&str>,
    ) -> Result<()> {
        let rel_pattern = rel_type_pattern(rel_type);
        let query = format!(
            "MATCH (a {{id: $src}})-[r{}]->(b {{id: $tgt}}) DELETE r",
            rel_pattern
        );
        self.connection()
            .cypher_builder(&query)
            .param("src", source_id)
            .param("tgt", target_id)
            .run()?;
        Ok(())
    }

    /// Get all edges in the graph.
    pub fn get_all_edges(&self) -> Result<CypherResult> {
        self.connection()
            .cypher("MATCH (a)-[r]->(b) RETURN a.id AS source, b.id AS target, r")
    }
}
