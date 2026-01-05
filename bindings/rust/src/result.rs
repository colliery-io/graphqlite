//! Result types for Cypher query results.
//!
//! This module provides types for working with Cypher query results:
//!
//! - [`CypherResult`] - A collection of rows returned from a query
//! - [`Row`] - A single row with named columns
//! - [`Value`] - A typed value that can be extracted from rows
//!
//! # Example
//!
//! ```no_run
//! use graphqlite::Connection;
//!
//! let conn = Connection::open_in_memory()?;
//! conn.cypher("CREATE (n:Person {name: 'Alice', age: 30})")?;
//!
//! let results = conn.cypher("MATCH (n:Person) RETURN n.name, n.age")?;
//! for row in &results {
//!     let name: String = row.get("n.name")?;
//!     let age: i64 = row.get("n.age")?;
//!     println!("{} is {} years old", name, age);
//! }
//! # Ok::<(), graphqlite::Error>(())
//! ```

use crate::Error;
use serde_json::Value as JsonValue;
use std::collections::HashMap;

/// A dynamically-typed value returned from a Cypher query.
///
/// Cypher queries can return various types of values. This enum represents
/// all possible value types and provides methods to inspect and extract them.
///
/// # Type Extraction
///
/// Use [`Row::get`] with a type parameter for type-safe extraction:
///
/// ```ignore
/// let name: String = row.get("name")?;
/// let age: i64 = row.get("age")?;
/// let score: f64 = row.get("score")?;
/// let active: bool = row.get("active")?;
/// let maybe: Option<String> = row.get("nullable_field")?;
/// ```
#[derive(Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
#[serde(untagged)]
pub enum Value {
    /// SQL/Cypher NULL value.
    Null,
    /// Boolean value (`true` or `false`).
    Bool(bool),
    /// 64-bit signed integer.
    Integer(i64),
    /// 64-bit floating-point number.
    Float(f64),
    /// UTF-8 string.
    String(String),
    /// Array of values (from Cypher list expressions or `collect()`).
    Array(Vec<Value>),
    /// Object/map of values (from node/relationship properties).
    Object(HashMap<String, Value>),
}

impl Value {
    /// Convert from a `serde_json::Value`.
    ///
    /// This is used internally when parsing Cypher query results.
    pub fn from_json(json: JsonValue) -> Self {
        match json {
            JsonValue::Null => Value::Null,
            JsonValue::Bool(b) => Value::Bool(b),
            JsonValue::Number(n) => {
                if let Some(i) = n.as_i64() {
                    Value::Integer(i)
                } else if let Some(f) = n.as_f64() {
                    Value::Float(f)
                } else {
                    Value::Null
                }
            }
            JsonValue::String(s) => Value::String(s),
            JsonValue::Array(arr) => {
                Value::Array(arr.into_iter().map(Value::from_json).collect())
            }
            JsonValue::Object(obj) => {
                Value::Object(obj.into_iter().map(|(k, v)| (k, Value::from_json(v))).collect())
            }
        }
    }

    /// Returns `true` if this value is [`Value::Null`].
    pub fn is_null(&self) -> bool {
        matches!(self, Value::Null)
    }

    /// Returns the boolean value if this is a [`Value::Bool`].
    ///
    /// Returns `None` for other value types.
    pub fn as_bool(&self) -> Option<bool> {
        match self {
            Value::Bool(b) => Some(*b),
            _ => None,
        }
    }

    /// Returns the integer value if this is a [`Value::Integer`].
    ///
    /// Returns `None` for other value types. For automatic conversion
    /// from floats, use [`as_f64`](Self::as_f64) and cast.
    pub fn as_i64(&self) -> Option<i64> {
        match self {
            Value::Integer(i) => Some(*i),
            _ => None,
        }
    }

    /// Returns the float value if this is a [`Value::Float`] or [`Value::Integer`].
    ///
    /// Integers are automatically converted to floats.
    pub fn as_f64(&self) -> Option<f64> {
        match self {
            Value::Float(f) => Some(*f),
            Value::Integer(i) => Some(*i as f64),
            _ => None,
        }
    }

    /// Returns a string slice if this is a [`Value::String`].
    ///
    /// Returns `None` for other value types.
    pub fn as_str(&self) -> Option<&str> {
        match self {
            Value::String(s) => Some(s),
            _ => None,
        }
    }
}

/// A single row from a Cypher query result.
///
/// Rows contain named columns that can be accessed by name using [`get`](Self::get)
/// or [`get_value`](Self::get_value).
///
/// # Example
///
/// ```ignore
/// for row in &results {
///     let name: String = row.get("n.name")?;
///     let age: i64 = row.get("n.age")?;
///     println!("{}: {}", name, age);
/// }
/// ```
#[derive(Debug, Clone)]
pub struct Row {
    columns: Vec<String>,
    values: HashMap<String, Value>,
}

impl Row {
    /// Create a new row from column names and a JSON object.
    pub(crate) fn from_json_object(obj: serde_json::Map<String, JsonValue>) -> Self {
        let columns: Vec<String> = obj.keys().cloned().collect();
        let values: HashMap<String, Value> = obj
            .into_iter()
            .map(|(k, v)| (k, Value::from_json(v)))
            .collect();
        Row { columns, values }
    }

    /// Create a new row from a HashMap of Values.
    ///
    /// Used internally to convert array elements from graph algorithm results.
    pub(crate) fn from_map(map: HashMap<String, Value>) -> Self {
        let columns: Vec<String> = map.keys().cloned().collect();
        Row { columns, values: map }
    }

    /// Get a raw [`Value`] by column name.
    ///
    /// Returns `None` if the column doesn't exist. For type-safe extraction,
    /// prefer [`get`](Self::get).
    pub fn get_value(&self, column: &str) -> Option<&Value> {
        self.values.get(column)
    }

    /// Get a typed value by column name.
    ///
    /// This is the primary way to extract values from query results.
    /// The type parameter determines how the value is converted.
    ///
    /// # Supported Types
    ///
    /// - `String` - for text values (null becomes empty string)
    /// - `i64`, `i32` - for integers
    /// - `f64` - for floats (integers auto-convert)
    /// - `bool` - for booleans (SQLite's 1/0 auto-convert)
    /// - `Option<T>` - for nullable values
    ///
    /// # Errors
    ///
    /// Returns [`Error::ColumnNotFound`] if the column doesn't exist,
    /// or [`Error::TypeError`] if the value can't be converted.
    pub fn get<T: FromValue>(&self, column: &str) -> crate::Result<T> {
        let value = self.values.get(column).ok_or_else(|| {
            Error::ColumnNotFound(column.to_string())
        })?;
        T::from_value(value)
    }

    /// Get the column names in this row.
    pub fn columns(&self) -> &[String] {
        &self.columns
    }

    /// Check if the row contains a column with the given name.
    pub fn contains(&self, column: &str) -> bool {
        self.values.contains_key(column)
    }
}

/// Trait for converting from [`Value`] to typed Rust values.
///
/// This trait is implemented for common types and is used by [`Row::get`]
/// to provide type-safe value extraction.
///
/// # Implementations
///
/// - `String` - extracts string values (null → empty string)
/// - `i64` - extracts integers
/// - `i32` - extracts integers (with truncation)
/// - `f64` - extracts floats (integers auto-convert)
/// - `bool` - extracts booleans (1/0 auto-convert)
/// - `Option<T>` - wraps any type, returning `None` for null
pub trait FromValue: Sized {
    /// Convert from a [`Value`] reference.
    fn from_value(value: &Value) -> crate::Result<Self>;
}

impl FromValue for String {
    fn from_value(value: &Value) -> crate::Result<Self> {
        match value {
            Value::String(s) => Ok(s.clone()),
            Value::Null => Ok(String::new()),
            other => Err(Error::TypeError {
                expected: "String",
                actual: format!("{:?}", other),
            }),
        }
    }
}

impl FromValue for i64 {
    fn from_value(value: &Value) -> crate::Result<Self> {
        match value {
            Value::Integer(i) => Ok(*i),
            // Handle string numbers (aggregations sometimes return strings)
            Value::String(s) => s.parse::<i64>().map_err(|_| Error::TypeError {
                expected: "Integer",
                actual: format!("String({:?})", s),
            }),
            other => Err(Error::TypeError {
                expected: "Integer",
                actual: format!("{:?}", other),
            }),
        }
    }
}

impl FromValue for i32 {
    fn from_value(value: &Value) -> crate::Result<Self> {
        match value {
            Value::Integer(i) => Ok(*i as i32),
            other => Err(Error::TypeError {
                expected: "Integer",
                actual: format!("{:?}", other),
            }),
        }
    }
}

impl FromValue for f64 {
    fn from_value(value: &Value) -> crate::Result<Self> {
        match value {
            Value::Float(f) => Ok(*f),
            Value::Integer(i) => Ok(*i as f64),
            other => Err(Error::TypeError {
                expected: "Float",
                actual: format!("{:?}", other),
            }),
        }
    }
}

impl FromValue for bool {
    fn from_value(value: &Value) -> crate::Result<Self> {
        match value {
            Value::Bool(b) => Ok(*b),
            // SQLite returns 1/0 for booleans
            Value::Integer(1) => Ok(true),
            Value::Integer(0) => Ok(false),
            other => Err(Error::TypeError {
                expected: "Bool",
                actual: format!("{:?}", other),
            }),
        }
    }
}

impl<T: FromValue> FromValue for Option<T> {
    fn from_value(value: &Value) -> crate::Result<Self> {
        match value {
            Value::Null => Ok(None),
            _ => Ok(Some(T::from_value(value)?)),
        }
    }
}

/// Result of a Cypher query, containing zero or more rows.
///
/// `CypherResult` implements `IntoIterator` so you can iterate over rows directly,
/// and `Index<usize>` for direct row access.
///
/// # Example
///
/// ```no_run
/// use graphqlite::Connection;
///
/// let conn = Connection::open_in_memory()?;
/// let results = conn.cypher("MATCH (n) RETURN n.name")?;
///
/// // Check size
/// println!("Found {} rows", results.len());
///
/// // Iterate over rows
/// for row in &results {
///     let name: String = row.get("n.name")?;
///     println!("{}", name);
/// }
///
/// // Direct access
/// if !results.is_empty() {
///     let first_name: String = results[0].get("n.name")?;
/// }
/// # Ok::<(), graphqlite::Error>(())
/// ```
#[derive(Debug, Clone)]
pub struct CypherResult {
    rows: Vec<Row>,
    columns: Vec<String>,
}

impl CypherResult {
    /// Create an empty result with no rows or columns.
    pub fn empty() -> Self {
        CypherResult {
            rows: Vec::new(),
            columns: Vec::new(),
        }
    }

    /// Parse a JSON string into a `CypherResult`.
    ///
    /// This is used internally when processing Cypher query output.
    pub fn from_json(json_str: &str) -> crate::Result<Self> {
        let trimmed = json_str.trim();
        if trimmed.is_empty() {
            return Ok(Self::empty());
        }

        // Try to parse as JSON
        let json: JsonValue = match serde_json::from_str(trimmed) {
            Ok(v) => v,
            Err(_) => {
                // Non-JSON result (possibly a status message or error)
                if trimmed.starts_with("Error") {
                    return Err(crate::Error::Cypher(trimmed.to_string()));
                }
                // Return as a scalar result
                return Ok(CypherResult {
                    rows: vec![Row::from_json_object({
                        let mut obj = serde_json::Map::new();
                        obj.insert("result".to_string(), JsonValue::String(trimmed.to_string()));
                        obj
                    })],
                    columns: vec!["result".to_string()],
                });
            }
        };

        match json {
            JsonValue::Array(arr) => {
                if arr.is_empty() {
                    return Ok(Self::empty());
                }

                let mut rows = Vec::with_capacity(arr.len());
                let mut columns = Vec::new();

                for (i, item) in arr.into_iter().enumerate() {
                    match item {
                        JsonValue::Object(obj) => {
                            if i == 0 {
                                columns = obj.keys().cloned().collect();
                            }
                            rows.push(Row::from_json_object(obj));
                        }
                        _ => {
                            // Scalar value in array
                            let mut obj = serde_json::Map::new();
                            obj.insert("value".to_string(), item);
                            if i == 0 {
                                columns = vec!["value".to_string()];
                            }
                            rows.push(Row::from_json_object(obj));
                        }
                    }
                }

                Ok(CypherResult { rows, columns })
            }
            JsonValue::Object(obj) => {
                let columns: Vec<String> = obj.keys().cloned().collect();
                let row = Row::from_json_object(obj);
                Ok(CypherResult {
                    rows: vec![row],
                    columns,
                })
            }
            _ => {
                // Single scalar value
                let mut obj = serde_json::Map::new();
                obj.insert("result".to_string(), json);
                Ok(CypherResult {
                    rows: vec![Row::from_json_object(obj)],
                    columns: vec!["result".to_string()],
                })
            }
        }
    }

    /// Returns the number of rows in the result.
    pub fn len(&self) -> usize {
        self.rows.len()
    }

    /// Returns `true` if the result contains no rows.
    pub fn is_empty(&self) -> bool {
        self.rows.is_empty()
    }

    /// Returns the column names from the query.
    ///
    /// Column names correspond to the `RETURN` clause expressions.
    pub fn columns(&self) -> &[String] {
        &self.columns
    }

    /// Returns a reference to the row at the given index, or `None` if out of bounds.
    pub fn get(&self, index: usize) -> Option<&Row> {
        self.rows.get(index)
    }

    /// Returns an iterator over the rows.
    pub fn iter(&self) -> impl Iterator<Item = &Row> {
        self.rows.iter()
    }
}

impl<'a> IntoIterator for &'a CypherResult {
    type Item = &'a Row;
    type IntoIter = std::slice::Iter<'a, Row>;

    fn into_iter(self) -> Self::IntoIter {
        self.rows.iter()
    }
}

impl IntoIterator for CypherResult {
    type Item = Row;
    type IntoIter = std::vec::IntoIter<Row>;

    fn into_iter(self) -> Self::IntoIter {
        self.rows.into_iter()
    }
}

impl std::ops::Index<usize> for CypherResult {
    type Output = Row;

    fn index(&self, index: usize) -> &Self::Output {
        &self.rows[index]
    }
}
