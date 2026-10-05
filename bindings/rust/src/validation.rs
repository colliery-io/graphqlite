//! Non-executing Cypher validation (GitHub issue #16).
//!
//! [`Connection::validate`](crate::Connection::validate) runs the scanner, the
//! grammar and the extension's compile-time semantic checks over a query
//! without touching the graph, and returns the outcome as data instead of an
//! error, so a client can pre-check Cypher it is about to run.

use serde::Deserialize;
use std::fmt;

/// One diagnostic produced by query validation (or carried by a `cypher()`
/// error): a stable code, a human-readable message and, when the parser can
/// locate the problem, a 1-based line and column.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Diagnostic {
    /// Stable extension error code: `PARSE_ERROR` for scanner and grammar
    /// failures, `VALIDATION_ERROR` for static semantic violations such as
    /// `RETURN NOT 1` or UNION branches with different columns.
    pub code: String,
    /// Human-readable reason. Grammar failures include the unexpected token
    /// and, where Bison knows it, the expected one.
    pub message: String,
    /// 1-based source line of the offending token, when known.
    pub line: Option<u32>,
    /// 1-based source column of the offending token, when known.
    pub column: Option<u32>,
}

impl fmt::Display for Diagnostic {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match (self.line, self.column) {
            (Some(l), Some(c)) => write!(f, "{} at {}:{}: {}", self.code, l, c, self.message),
            (Some(l), None) => write!(f, "{} at line {}: {}", self.code, l, self.message),
            _ => write!(f, "{}: {}", self.code, self.message),
        }
    }
}

/// Outcome of [`Connection::validate`](crate::Connection::validate).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ValidationResult {
    /// `true` when the query parsed and passed static validation.
    pub valid: bool,
    /// The diagnostic that rejected the query; `None` when `valid`.
    pub diagnostic: Option<Diagnostic>,
}

impl ValidationResult {
    /// Shorthand for `self.valid`.
    pub fn is_valid(&self) -> bool {
        self.valid
    }

    pub(crate) fn from_json(json: &str) -> serde_json::Result<Self> {
        #[derive(Deserialize)]
        struct Wire {
            valid: bool,
            #[serde(default)]
            error: Option<String>,
            #[serde(default)]
            code: Option<String>,
            #[serde(default)]
            line: Option<u32>,
            #[serde(default)]
            column: Option<u32>,
        }
        let w: Wire = serde_json::from_str(json)?;
        let diagnostic = if w.valid {
            None
        } else {
            Some(Diagnostic {
                code: w.code.unwrap_or_else(|| "VALIDATION_ERROR".to_string()),
                message: w
                    .error
                    .unwrap_or_else(|| "Unknown validation error".to_string()),
                line: w.line,
                column: w.column,
            })
        };
        Ok(ValidationResult {
            valid: w.valid,
            diagnostic,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_valid() {
        let r = ValidationResult::from_json(r#"{"valid": true}"#).unwrap();
        assert!(r.is_valid());
        assert!(r.diagnostic.is_none());
    }

    #[test]
    fn parses_parse_error_with_location() {
        let r = ValidationResult::from_json(
            r#"{"valid":false,"error":"Line 1, Col 17: syntax error","code":"PARSE_ERROR","line":1,"column":17}"#,
        )
        .unwrap();
        assert!(!r.valid);
        let d = r.diagnostic.unwrap();
        assert_eq!(d.code, "PARSE_ERROR");
        assert_eq!((d.line, d.column), (Some(1), Some(17)));
        assert_eq!(
            d.to_string(),
            "PARSE_ERROR at 1:17: Line 1, Col 17: syntax error"
        );
    }

    #[test]
    fn parses_validation_error_without_location() {
        let r = ValidationResult::from_json(
            r#"{"valid":false,"error":"Type mismatch","code":"VALIDATION_ERROR"}"#,
        )
        .unwrap();
        let d = r.diagnostic.unwrap();
        assert_eq!(d.code, "VALIDATION_ERROR");
        assert_eq!((d.line, d.column), (None, None));
        assert_eq!(d.to_string(), "VALIDATION_ERROR: Type mismatch");
    }
}
