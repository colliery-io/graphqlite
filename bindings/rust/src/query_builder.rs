//! Fluent builder for parameterized Cypher queries.

use crate::{Connection, CypherResult, Result};

/// A builder for constructing and executing parameterized Cypher queries.
///
/// Created via [`Connection::cypher_builder`] or [`Graph::query_builder`](crate::Graph::query_builder).
///
/// # Examples
///
/// ```no_run
/// use graphqlite::Connection;
/// use serde_json::json;
///
/// let conn = Connection::open_in_memory()?;
/// conn.cypher("CREATE (n:Person {name: 'Alice', age: 30})")?;
///
/// // Individual params
/// let results = conn.cypher_builder("MATCH (n:Person) WHERE n.name = $name RETURN n")
///     .param("name", "Alice")
///     .run()?;
///
/// // Bulk params
/// let results = conn.cypher_builder("MATCH (n:Person) WHERE n.age > $min RETURN n")
///     .params(&json!({"min": 25}))
///     .run()?;
/// # Ok::<(), graphqlite::Error>(())
/// ```
pub struct CypherQuery<'a> {
    conn: &'a Connection,
    query: &'a str,
    params: serde_json::Map<String, serde_json::Value>,
}

impl<'a> CypherQuery<'a> {
    /// Create a new builder (called internally by `Connection::cypher_builder`).
    pub(crate) fn new(conn: &'a Connection, query: &'a str) -> Self {
        CypherQuery {
            conn,
            query,
            params: serde_json::Map::new(),
        }
    }

    /// Set an individual named parameter.
    ///
    /// Can be chained for multiple parameters.
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # use graphqlite::Connection;
    /// # let conn = Connection::open_in_memory()?;
    /// conn.cypher_builder("MATCH (n) WHERE n.name = $name AND n.age > $min RETURN n")
    ///     .param("name", "Alice")
    ///     .param("min", 25)
    ///     .run()?;
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn param(mut self, key: &str, value: impl Into<serde_json::Value>) -> Self {
        self.params.insert(key.to_string(), value.into());
        self
    }

    /// Merge parameters from a JSON object.
    ///
    /// Non-object values are silently ignored. Keys already set via
    /// [`param`](Self::param) are overwritten by matching keys in the object.
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # use graphqlite::Connection;
    /// # use serde_json::json;
    /// # let conn = Connection::open_in_memory()?;
    /// conn.cypher_builder("MATCH (n) WHERE n.age > $min AND n.age < $max RETURN n")
    ///     .params(&json!({"min": 20, "max": 40}))
    ///     .run()?;
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn params(mut self, params: &serde_json::Value) -> Self {
        if let serde_json::Value::Object(map) = params {
            for (k, v) in map {
                self.params.insert(k.clone(), v.clone());
            }
        }
        self
    }

    /// Execute the query and return results.
    ///
    /// If no parameters have been set, delegates to the non-parameterized
    /// path for efficiency.
    pub fn run(self) -> Result<CypherResult> {
        if self.params.is_empty() {
            self.conn.cypher(self.query)
        } else {
            let params_value = serde_json::Value::Object(self.params);
            self.conn.execute_cypher_with_params(self.query, &params_value)
        }
    }
}
