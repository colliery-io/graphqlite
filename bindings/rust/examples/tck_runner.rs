//! Long-lived REPL used by the openCypher TCK harness's `RustBindingBackend`.
//!
//! Protocol (one JSON object per line on stdin, one JSON object per line on stdout):
//!
//!   request: {"cmd": "reset"}              -> {"ok": true}
//!   request: {"cmd": "execute", "query": "MATCH (n) RETURN n"}
//!     ok:    {"ok": true, "rows": <serde_json::Value>}
//!     error: {"ok": false, "error": "..."}
//!   request: {"cmd": "shutdown"}           -> process exits 0
//!
//! Rows preserve whatever shape the binding's `CypherResult` exposes via
//! `to_json()`; the harness compares structurally and does not depend on
//! a particular schema beyond "JSON-serializable".
//!
//! Run via:  cargo run --example tck_runner --manifest-path bindings/rust/Cargo.toml

use std::io::{self, BufRead, Write};

use graphqlite::Connection;
use serde::Deserialize;
use serde_json::{json, Value};

#[derive(Deserialize)]
#[serde(tag = "cmd", rename_all = "lowercase")]
enum Request {
    Reset,
    Execute {
        query: String,
        #[serde(default)]
        parameters: Option<Value>,
    },
    Shutdown,
}

fn main() {
    let stdin = io::stdin();
    let stdout = io::stdout();
    let mut out = stdout.lock();

    let mut conn = match Connection::open_in_memory() {
        Ok(c) => c,
        Err(e) => {
            let _ = writeln!(
                out,
                "{}",
                json!({"ok": false, "error": format!("init: {e}")})
            );
            std::process::exit(1);
        }
    };

    for line in stdin.lock().lines() {
        let line = match line {
            Ok(l) => l,
            Err(e) => {
                let _ = writeln!(
                    out,
                    "{}",
                    json!({"ok": false, "error": format!("stdin: {e}")})
                );
                continue;
            }
        };
        let trimmed = line.trim();
        if trimmed.is_empty() {
            continue;
        }
        let req: Request = match serde_json::from_str(trimmed) {
            Ok(r) => r,
            Err(e) => {
                let _ = writeln!(
                    out,
                    "{}",
                    json!({"ok": false, "error": format!("parse: {e}")})
                );
                let _ = out.flush();
                continue;
            }
        };

        let resp: Value = match req {
            Request::Reset => match Connection::open_in_memory() {
                Ok(c) => {
                    conn = c;
                    json!({"ok": true})
                }
                Err(e) => json!({"ok": false, "error": format!("reset: {e}")}),
            },
            Request::Execute { query, parameters } => match match &parameters {
                Some(p) if !p.is_null() => conn.cypher_builder(&query).params(p).run(),
                _ => conn.cypher(&query),
            } {
                Ok(res) => {
                    let cols: Vec<String> = res.columns().to_vec();
                    let mut rows: Vec<Value> = Vec::with_capacity(res.len());
                    for i in 0..res.len() {
                        if let Some(row) = res.get(i) {
                            let mut obj = serde_json::Map::new();
                            for c in &cols {
                                if let Some(v) = row.get_value(c) {
                                    obj.insert(c.clone(), to_wire(v));
                                } else {
                                    obj.insert(c.clone(), Value::Null);
                                }
                            }
                            rows.push(Value::Object(obj));
                        }
                    }
                    json!({"ok": true, "columns": cols, "rows": rows})
                }
                Err(e) => json!({"ok": false, "error": format!("{e}")}),
            },
            Request::Shutdown => {
                let _ = writeln!(out, "{}", json!({"ok": true}));
                let _ = out.flush();
                return;
            }
        };
        let _ = writeln!(out, "{}", resp);
        let _ = out.flush();
    }
}

/// A result value as JSON for the harness. JSON has no NaN or Infinity, so
/// a non-finite float is sent as {"$float": "NaN" | "Infinity" | "-Infinity"}
/// (GQLITE-T-0532).
fn to_wire(v: &graphqlite::Value) -> Value {
    match v {
        graphqlite::Value::Float(f) if !f.is_finite() => {
            let s = if f.is_nan() {
                "NaN"
            } else if *f > 0.0 {
                "Infinity"
            } else {
                "-Infinity"
            };
            json!({ "$float": s })
        }
        graphqlite::Value::Array(a) => Value::Array(a.iter().map(to_wire).collect()),
        graphqlite::Value::Object(o) => {
            Value::Object(o.iter().map(|(k, x)| (k.clone(), to_wire(x))).collect())
        }
        other => serde_json::to_value(other).unwrap_or(Value::Null),
    }
}
