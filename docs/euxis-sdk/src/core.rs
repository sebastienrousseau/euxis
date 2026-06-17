// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2024-2026 Sebastien Rousseau · https://sebastienrousseau.com

//! Platform-agnostic types shared by the SDK and agent crates.

/// Errors returned by SDK operations.
///
/// # Variants
///
/// | Variant         | Meaning                                  |
/// |-----------------|------------------------------------------|
/// | `Serialization` | JSON / MessagePack codec failure.         |
/// | `Host`          | The Euxis host rejected the call.         |
/// | `Protocol`      | Unexpected wire format or missing fields. |
///
/// # Example
///
/// ```
/// use euxis_sdk::Error;
///
/// let err = Error::Host("metrics endpoint unavailable".into());
/// assert!(err.to_string().contains("unavailable"));
/// ```
#[derive(Debug, thiserror::Error)]
pub enum Error {
    /// A serialization or deserialization step failed.
    ///
    /// ```
    /// use euxis_sdk::Error;
    /// let e = Error::Serialization("bad json".into());
    /// assert_eq!(e.to_string(), "serialization error: bad json");
    /// ```
    #[error("serialization error: {0}")]
    Serialization(String),

    /// The Euxis host returned an error.
    ///
    /// ```
    /// use euxis_sdk::Error;
    /// let e = Error::Host("not found".into());
    /// assert_eq!(e.to_string(), "host error: not found");
    /// ```
    #[error("host error: {0}")]
    Host(String),

    /// A protocol-level violation (unexpected format, missing fields).
    ///
    /// ```
    /// use euxis_sdk::Error;
    /// let e = Error::Protocol("missing 'data' field".into());
    /// assert_eq!(e.to_string(), "protocol error: missing 'data' field");
    /// ```
    #[error("protocol error: {0}")]
    Protocol(String),
}

/// Convenience alias for SDK results.
///
/// # Example
///
/// ```
/// use euxis_sdk::{MCPResult, Error};
///
/// fn try_parse(s: &str) -> MCPResult<u32> {
///     s.parse::<u32>().map_err(|e| Error::Serialization(e.to_string()))
/// }
///
/// assert!(try_parse("42").is_ok());
/// assert!(try_parse("abc").is_err());
/// ```
pub type MCPResult<T> = Result<T, Error>;

/// An opaque content block returned by an MCP tool call.
///
/// # Example
///
/// ```
/// use euxis_sdk::MCPContent;
///
/// let content = MCPContent {
///     content_type: "application/json".into(),
///     data: br#"{"ok":true}"#.to_vec(),
/// };
/// assert_eq!(content.content_type, "application/json");
/// assert!(!content.data.is_empty());
/// ```
#[derive(Debug, Clone, serde::Serialize, serde::Deserialize, PartialEq)]
pub struct MCPContent {
    /// MIME type of the payload (e.g. `"application/json"`).
    pub content_type: String,
    /// Raw bytes of the response body.
    pub data: Vec<u8>,
}

#[cfg(test)]
mod tests {
    use super::*;

    // ── Error ────────────────────────────────────────────────────────

    #[test]
    fn error_serialization_display() {
        let e = Error::Serialization("bad json".into());
        assert_eq!(e.to_string(), "serialization error: bad json");
    }

    #[test]
    fn error_serialization_empty_message() {
        let e = Error::Serialization(String::new());
        assert_eq!(e.to_string(), "serialization error: ");
    }

    #[test]
    fn error_serialization_unicode() {
        let e = Error::Serialization("不正なJSON".into());
        assert!(e.to_string().contains("不正なJSON"));
    }

    #[test]
    fn error_serialization_debug() {
        let e = Error::Serialization("codec failure".into());
        let dbg = format!("{:?}", e);
        assert!(dbg.contains("Serialization"));
        assert!(dbg.contains("codec failure"));
    }

    #[test]
    fn error_host_display() {
        let e = Error::Host("not found".into());
        assert_eq!(e.to_string(), "host error: not found");
    }

    #[test]
    fn error_host_empty_message() {
        let e = Error::Host(String::new());
        assert_eq!(e.to_string(), "host error: ");
    }

    #[test]
    fn error_host_long_message() {
        let msg = "x".repeat(1000);
        let e = Error::Host(msg.clone());
        assert!(e.to_string().contains(&msg));
    }

    #[test]
    fn error_host_debug() {
        let e = Error::Host("timeout".into());
        let dbg = format!("{:?}", e);
        assert!(dbg.contains("Host"));
        assert!(dbg.contains("timeout"));
    }

    #[test]
    fn error_protocol_display() {
        let e = Error::Protocol("missing 'data' field".into());
        assert_eq!(e.to_string(), "protocol error: missing 'data' field");
    }

    #[test]
    fn error_protocol_empty_message() {
        let e = Error::Protocol(String::new());
        assert_eq!(e.to_string(), "protocol error: ");
    }

    #[test]
    fn error_protocol_special_chars() {
        let e = Error::Protocol("field <\"data\"> & 'type'".into());
        assert!(e.to_string().contains("<\"data\">"));
    }

    #[test]
    fn error_protocol_debug() {
        let e = Error::Protocol("bad wire format".into());
        let dbg = format!("{:?}", e);
        assert!(dbg.contains("Protocol"));
        assert!(dbg.contains("bad wire format"));
    }

    #[test]
    fn error_implements_std_error() {
        let e: Box<dyn std::error::Error> =
            Box::new(Error::Host("test".into()));
        assert!(e.to_string().contains("test"));
    }

    #[test]
    fn error_variants_are_distinct() {
        let s = Error::Serialization("x".into()).to_string();
        let h = Error::Host("x".into()).to_string();
        let p = Error::Protocol("x".into()).to_string();
        assert_ne!(s, h);
        assert_ne!(h, p);
        assert_ne!(s, p);
    }

    #[test]
    fn error_newlines_in_message() {
        let e = Error::Serialization("line1\nline2".into());
        assert_eq!(e.to_string(), "serialization error: line1\nline2");
    }

    // ── MCPResult ────────────────────────────────────────────────────

    #[test]
    fn mcp_result_ok() {
        let r: MCPResult<u32> = Ok(42);
        assert_eq!(r.unwrap(), 42);
    }

    #[test]
    fn mcp_result_err() {
        let r: MCPResult<u32> = Err(Error::Host("fail".into()));
        assert!(r.is_err());
    }

    #[test]
    fn mcp_result_map() {
        let r: MCPResult<u32> = Ok(10);
        let doubled = r.map(|v| v * 2);
        assert_eq!(doubled.unwrap(), 20);
    }

    #[test]
    fn mcp_result_and_then() {
        let r: MCPResult<&str> = Ok("42");
        let parsed = r.and_then(|s| {
            s.parse::<u32>()
                .map_err(|e| Error::Serialization(e.to_string()))
        });
        assert_eq!(parsed.unwrap(), 42);
    }

    #[test]
    fn mcp_result_and_then_err() {
        let r: MCPResult<&str> = Ok("abc");
        let parsed = r.and_then(|s| {
            s.parse::<u32>()
                .map_err(|e| Error::Serialization(e.to_string()))
        });
        assert!(parsed.is_err());
    }

    #[test]
    fn mcp_result_unwrap_or() {
        let r: MCPResult<u32> = Err(Error::Host("gone".into()));
        assert_eq!(r.unwrap_or(0), 0);
    }

    // ── MCPContent ───────────────────────────────────────────────────

    #[test]
    fn mcp_content_construction() {
        let c = MCPContent {
            content_type: "text/plain".into(),
            data: b"hello".to_vec(),
        };
        assert_eq!(c.content_type, "text/plain");
        assert_eq!(c.data, b"hello");
    }

    #[test]
    fn mcp_content_empty_data() {
        let c = MCPContent {
            content_type: "application/octet-stream".into(),
            data: vec![],
        };
        assert!(c.data.is_empty());
    }

    #[test]
    fn mcp_content_binary_data() {
        let c = MCPContent {
            content_type: "application/octet-stream".into(),
            data: vec![0x00, 0xFF, 0x7F, 0x80],
        };
        assert_eq!(c.data.len(), 4);
        assert_eq!(c.data[0], 0x00);
        assert_eq!(c.data[3], 0x80);
    }

    #[test]
    fn mcp_content_clone() {
        let c1 = MCPContent {
            content_type: "application/json".into(),
            data: br#"{"ok":true}"#.to_vec(),
        };
        let c2 = c1.clone();
        assert_eq!(c1, c2);
        assert_eq!(c1.content_type, c2.content_type);
        assert_eq!(c1.data, c2.data);
    }

    #[test]
    fn mcp_content_debug() {
        let c = MCPContent {
            content_type: "text/plain".into(),
            data: b"hi".to_vec(),
        };
        let dbg = format!("{:?}", c);
        assert!(dbg.contains("MCPContent"));
        assert!(dbg.contains("text/plain"));
    }

    #[test]
    fn mcp_content_serialize() {
        let c = MCPContent {
            content_type: "application/json".into(),
            data: vec![1, 2, 3],
        };
        let json = serde_json::to_string(&c).unwrap();
        assert!(json.contains("application/json"));
        assert!(json.contains("content_type"));
        assert!(json.contains("data"));
    }

    #[test]
    fn mcp_content_deserialize() {
        let json = r#"{"content_type":"text/html","data":[60,104,49,62]}"#;
        let c: MCPContent = serde_json::from_str(json).unwrap();
        assert_eq!(c.content_type, "text/html");
        assert_eq!(c.data, vec![60, 104, 49, 62]); // <h1>
    }

    #[test]
    fn mcp_content_roundtrip() {
        let original = MCPContent {
            content_type: "application/msgpack".into(),
            data: vec![0xDE, 0xAD, 0xBE, 0xEF],
        };
        let json = serde_json::to_string(&original).unwrap();
        let restored: MCPContent = serde_json::from_str(&json).unwrap();
        assert_eq!(original, restored);
    }

    #[test]
    fn mcp_content_deserialize_extra_fields_ignored() {
        let json = r#"{"content_type":"text/plain","data":[65],"extra":"ignored"}"#;
        let c: MCPContent = serde_json::from_str(json).unwrap();
        assert_eq!(c.content_type, "text/plain");
    }

    #[test]
    fn mcp_content_deserialize_missing_field() {
        let json = r#"{"content_type":"text/plain"}"#;
        let result = serde_json::from_str::<MCPContent>(json);
        assert!(result.is_err());
    }

    #[test]
    fn mcp_content_deserialize_wrong_type() {
        let json = r#"{"content_type":42,"data":[]}"#;
        let result = serde_json::from_str::<MCPContent>(json);
        assert!(result.is_err());
    }

    #[test]
    fn mcp_content_large_data() {
        let c = MCPContent {
            content_type: "application/octet-stream".into(),
            data: vec![0xAB; 10_000],
        };
        assert_eq!(c.data.len(), 10_000);
        let cloned = c.clone();
        assert_eq!(c, cloned);
    }

    #[test]
    fn mcp_content_empty_content_type() {
        let c = MCPContent {
            content_type: String::new(),
            data: vec![1],
        };
        assert!(c.content_type.is_empty());
    }

    #[test]
    fn mcp_content_unicode_content_type() {
        let c = MCPContent {
            content_type: "text/plain; charset=utf-8".into(),
            data: "héllo".as_bytes().to_vec(),
        };
        assert!(c.content_type.contains("charset=utf-8"));
    }

    #[test]
    fn mcp_content_partial_eq() {
        let a = MCPContent {
            content_type: "a".into(),
            data: vec![1],
        };
        let b = MCPContent {
            content_type: "a".into(),
            data: vec![1],
        };
        let c = MCPContent {
            content_type: "a".into(),
            data: vec![2],
        };
        assert_eq!(a, b);
        assert_ne!(a, c);
    }
}
