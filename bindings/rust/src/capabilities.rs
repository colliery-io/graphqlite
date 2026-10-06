//! Capability metadata reported by the extension (GQLITE-T-0100).
//!
//! `Connection::capabilities()` wraps the SQL function `cypher_capabilities()`
//! so a client can detect features at run time instead of comparing version
//! strings.

use std::collections::BTreeMap;

use serde::Deserialize;

/// What this build of the extension supports.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Capabilities {
    /// Changes only when a key of the capabilities document is added,
    /// renamed or removed. New feature flags do not change it.
    pub schema_version: u32,
    /// The extension version, e.g. `"0.9.1"`.
    pub graphqlite_version: String,
    /// The Cypher dialect the grammar follows, e.g. `"openCypher 9"`.
    pub cypher_dialect: String,
    /// The SQLite library version the extension runs in.
    pub sqlite_version: String,
    /// Whether SQLite's JSON functions are available.
    pub json1: bool,
    /// Reserved for a Neo4j compatibility mode; `false` today.
    pub neo4j_compat: bool,
    /// One boolean per feature flag, by name.
    pub features: BTreeMap<String, bool>,
}

impl Capabilities {
    /// `true` when `flag` is present and enabled; a missing flag reads as `false`.
    pub fn supports(&self, flag: &str) -> bool {
        self.features.get(flag).copied().unwrap_or(false)
    }

    pub(crate) fn from_json(json: &str) -> serde_json::Result<Self> {
        #[derive(Deserialize)]
        struct SqliteWire {
            version: String,
            json1: bool,
        }
        #[derive(Deserialize)]
        struct Wire {
            schema_version: u32,
            graphqlite_version: String,
            cypher_dialect: String,
            sqlite: SqliteWire,
            #[serde(default)]
            neo4j_compat: bool,
            #[serde(default)]
            features: BTreeMap<String, bool>,
        }
        let w: Wire = serde_json::from_str(json)?;
        Ok(Capabilities {
            schema_version: w.schema_version,
            graphqlite_version: w.graphqlite_version,
            cypher_dialect: w.cypher_dialect,
            sqlite_version: w.sqlite.version,
            json1: w.sqlite.json1,
            neo4j_compat: w.neo4j_compat,
            features: w.features,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_document_and_defaults_missing_flags_to_false() {
        let c = Capabilities::from_json(
            r#"{"schema_version":1,"graphqlite_version":"0.9.1","cypher_dialect":"openCypher 9",
                "sqlite":{"version":"3.47.2","json1":true},"neo4j_compat":false,
                "features":{"load_csv":false,"validate":true}}"#,
        )
        .unwrap();
        assert_eq!(c.schema_version, 1);
        assert_eq!(c.graphqlite_version, "0.9.1");
        assert!(c.json1);
        assert!(c.supports("validate"));
        assert!(!c.supports("load_csv"));
        assert!(!c.supports("not_a_flag"));
    }
}
