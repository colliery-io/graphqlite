//! Result types for Cypher query results.

use crate::Error;
use serde_json::Value as JsonValue;
use std::collections::HashMap;

/// A value returned from a Cypher query.
#[derive(Debug, Clone, PartialEq)]
pub enum Value {
    /// Null value.
    Null,
    /// Boolean value.
    Bool(bool),
    /// Integer value.
    Integer(i64),
    /// Floating-point value.
    Float(f64),
    /// String value.
    String(String),
    /// Array of values.
    Array(Vec<Value>),
    /// Object/map of values.
    Object(HashMap<String, Value>),
}

impl Value {
    /// Convert from serde_json Value.
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

    /// Check if the value is null.
    pub fn is_null(&self) -> bool {
        matches!(self, Value::Null)
    }

    /// Try to get as a boolean.
    pub fn as_bool(&self) -> Option<bool> {
        match self {
            Value::Bool(b) => Some(*b),
            _ => None,
        }
    }

    /// Try to get as an integer.
    pub fn as_i64(&self) -> Option<i64> {
        match self {
            Value::Integer(i) => Some(*i),
            _ => None,
        }
    }

    /// Try to get as a float.
    pub fn as_f64(&self) -> Option<f64> {
        match self {
            Value::Float(f) => Some(*f),
            Value::Integer(i) => Some(*i as f64),
            _ => None,
        }
    }

    /// Try to get as a string.
    pub fn as_str(&self) -> Option<&str> {
        match self {
            Value::String(s) => Some(s),
            _ => None,
        }
    }
}

/// A single row from a Cypher query result.
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

    /// Get a value by column name.
    pub fn get_value(&self, column: &str) -> Option<&Value> {
        self.values.get(column)
    }

    /// Get a typed value by column name.
    pub fn get<T: FromValue>(&self, column: &str) -> crate::Result<T> {
        let value = self.values.get(column).ok_or_else(|| {
            Error::ColumnNotFound(column.to_string())
        })?;
        T::from_value(value)
    }

    /// Get column names.
    pub fn columns(&self) -> &[String] {
        &self.columns
    }

    /// Check if the row contains a column.
    pub fn contains(&self, column: &str) -> bool {
        self.values.contains_key(column)
    }
}

/// Trait for converting from Value to typed values.
pub trait FromValue: Sized {
    /// Convert from a Value reference.
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

/// Result of a Cypher query.
#[derive(Debug, Clone)]
pub struct CypherResult {
    rows: Vec<Row>,
    columns: Vec<String>,
}

impl CypherResult {
    /// Create an empty result.
    pub fn empty() -> Self {
        CypherResult {
            rows: Vec::new(),
            columns: Vec::new(),
        }
    }

    /// Parse a JSON string into a CypherResult.
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

    /// Get the number of rows.
    pub fn len(&self) -> usize {
        self.rows.len()
    }

    /// Check if the result is empty.
    pub fn is_empty(&self) -> bool {
        self.rows.is_empty()
    }

    /// Get column names.
    pub fn columns(&self) -> &[String] {
        &self.columns
    }

    /// Get a row by index.
    pub fn get(&self, index: usize) -> Option<&Row> {
        self.rows.get(index)
    }

    /// Iterate over rows.
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
