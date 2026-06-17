// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2024-2026 Sebastien Rousseau · https://sebastienrousseau.com

//! Host function bindings for the Euxis runtime.
//!
//! When compiled to `wasm32`, these functions call into the host via
//! Extism's `host_fn` mechanism. On native targets a stub
//! implementation is provided so that `cargo doc` and `cargo check`
//! work without cross-compilation.
//!
//! # Example
//!
//! ```no_run
//! use euxis_sdk::platform::{get_metrics, sign_payload, call_tool};
//!
//! # fn main() -> euxis_sdk::MCPResult<()> {
//! let m = get_metrics()?;
//! let s = sign_payload("data")?;
//! let c = call_tool("lint", serde_json::json!({"fix": true}))?;
//! # Ok(())
//! # }
//! ```

use crate::core::{Error, MCPContent, MCPResult};

// ── wasm32 host bindings ────────────────────────────────────────────

#[cfg(target_arch = "wasm32")]
mod host {
    #[extism_pdk::host_fn]
    extern "ExtismHost" {
        pub fn euxis_get_metrics() -> String;
        pub fn euxis_sign_payload(payload: String) -> String;
        pub fn euxis_call_tool(request: String) -> String;
    }
}

// ── native stubs (for cargo doc / cargo check) ──────────────────────

#[cfg(not(target_arch = "wasm32"))]
mod host {
    use super::Error;

    /// Stub: returns an error on native targets.
    pub unsafe fn euxis_get_metrics() -> Result<String, extism_pdk::Error> {
        Err(extism_pdk::Error::msg(
            Error::Host("not running in wasm".into()).to_string(),
        ))
    }

    /// Stub: returns an error on native targets.
    pub unsafe fn euxis_sign_payload(
        _payload: String,
    ) -> Result<String, extism_pdk::Error> {
        Err(extism_pdk::Error::msg(
            Error::Host("not running in wasm".into()).to_string(),
        ))
    }

    /// Stub: returns an error on native targets.
    pub unsafe fn euxis_call_tool(
        _request: String,
    ) -> Result<String, extism_pdk::Error> {
        Err(extism_pdk::Error::msg(
            Error::Host("not running in wasm".into()).to_string(),
        ))
    }
}

// ── Internal processing (testable without host) ─────────────────────

fn process_host_result(
    raw: Result<String, extism_pdk::Error>,
) -> MCPResult<String> {
    raw.map_err(|e| Error::Host(e.to_string()))
}

fn parse_json_value(raw: &str) -> MCPResult<serde_json::Value> {
    serde_json::from_str(raw).map_err(|e| Error::Serialization(e.to_string()))
}

fn parse_mcp_content(raw: &str) -> MCPResult<MCPContent> {
    serde_json::from_str(raw).map_err(|e| Error::Serialization(e.to_string()))
}

fn serialize_tool_request(tool: &str, params: serde_json::Value) -> MCPResult<String> {
    let req = ToolRequest { tool, params };
    serde_json::to_string(&req).map_err(|e| Error::Serialization(e.to_string()))
}

/// Fetch runtime metrics from the Euxis host.
///
/// Returns a JSON value containing agent-level metrics such as
/// invocation count, latency percentiles, and active-session data.
///
/// # Errors
///
/// Returns [`Error::Host`] if the host call fails, or
/// [`Error::Serialization`] if the response is not valid JSON.
///
/// # Example
///
/// ```no_run
/// # fn main() -> euxis_sdk::MCPResult<()> {
/// let metrics = euxis_sdk::get_metrics()?;
/// println!("metrics: {metrics}");
/// # Ok(())
/// # }
/// ```
pub fn get_metrics() -> MCPResult<serde_json::Value> {
    let raw = process_host_result(unsafe { host::euxis_get_metrics() })?;
    parse_json_value(&raw)
}

/// Sign a payload using the Euxis cryptographic provenance system.
///
/// The host applies HMAC-SHA256 (or the configured algorithm) and
/// returns the base64-encoded signature.
///
/// # Errors
///
/// Returns [`Error::Host`] if signing fails.
///
/// # Example
///
/// ```no_run
/// # fn main() -> euxis_sdk::MCPResult<()> {
/// let sig = euxis_sdk::sign_payload("hello world")?;
/// assert!(!sig.is_empty());
/// # Ok(())
/// # }
/// ```
pub fn sign_payload(payload: &str) -> MCPResult<String> {
    process_host_result(unsafe { host::euxis_sign_payload(payload.to_owned()) })
}

/// Internal request struct serialized to the host.
#[derive(serde::Serialize)]
struct ToolRequest<'a> {
    tool: &'a str,
    params: serde_json::Value,
}

/// Invoke an MCP tool by name with the given parameters.
///
/// The host dispatches the call to the registered MCP tool and returns
/// the response as an [`MCPContent`] block.
///
/// # Errors
///
/// Returns [`Error::Host`] if the tool call fails, or
/// [`Error::Serialization`] if the request/response codec fails.
///
/// # Example
///
/// ```no_run
/// # fn main() -> euxis_sdk::MCPResult<()> {
/// let result = euxis_sdk::call_tool(
///     "lint",
///     serde_json::json!({"path": "src/main.rs"}),
/// )?;
/// println!("type: {}, len: {}", result.content_type, result.data.len());
/// # Ok(())
/// # }
/// ```
pub fn call_tool(tool: &str, params: serde_json::Value) -> MCPResult<MCPContent> {
    let json = serialize_tool_request(tool, params)?;
    let raw = process_host_result(unsafe { host::euxis_call_tool(json) })?;
    parse_mcp_content(&raw)
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    // ── process_host_result ──────────────────────────────────────────

    #[test]
    fn process_host_result_ok() {
        let r: Result<String, extism_pdk::Error> = Ok("hello".into());
        let result = process_host_result(r);
        assert_eq!(result.unwrap(), "hello");
    }

    #[test]
    fn process_host_result_err() {
        let r: Result<String, extism_pdk::Error> =
            Err(extism_pdk::Error::msg("host down"));
        let result = process_host_result(r);
        match result {
            Err(Error::Host(msg)) => assert!(msg.contains("host down")),
            other => panic!("expected Host error, got {:?}", other),
        }
    }

    #[test]
    fn process_host_result_empty_ok() {
        let r: Result<String, extism_pdk::Error> = Ok(String::new());
        assert_eq!(process_host_result(r).unwrap(), "");
    }

    #[test]
    fn process_host_result_large_ok() {
        let big = "x".repeat(100_000);
        let r: Result<String, extism_pdk::Error> = Ok(big.clone());
        assert_eq!(process_host_result(r).unwrap(), big);
    }

    // ── parse_json_value ─────────────────────────────────────────────

    #[test]
    fn parse_json_value_object() {
        let v = parse_json_value(r#"{"key":"value"}"#).unwrap();
        assert_eq!(v["key"], "value");
    }

    #[test]
    fn parse_json_value_array() {
        let v = parse_json_value("[1,2,3]").unwrap();
        assert_eq!(v.as_array().unwrap().len(), 3);
    }

    #[test]
    fn parse_json_value_null() {
        let v = parse_json_value("null").unwrap();
        assert!(v.is_null());
    }

    #[test]
    fn parse_json_value_number() {
        let v = parse_json_value("42").unwrap();
        assert_eq!(v.as_i64().unwrap(), 42);
    }

    #[test]
    fn parse_json_value_float() {
        let v = parse_json_value("3.14").unwrap();
        assert!((v.as_f64().unwrap() - 3.14).abs() < f64::EPSILON);
    }

    #[test]
    fn parse_json_value_string() {
        let v = parse_json_value(r#""hello""#).unwrap();
        assert_eq!(v.as_str().unwrap(), "hello");
    }

    #[test]
    fn parse_json_value_bool() {
        let v = parse_json_value("true").unwrap();
        assert!(v.as_bool().unwrap());
    }

    #[test]
    fn parse_json_value_nested() {
        let v = parse_json_value(r#"{"a":{"b":{"c":1}}}"#).unwrap();
        assert_eq!(v["a"]["b"]["c"], 1);
    }

    #[test]
    fn parse_json_value_empty_object() {
        let v = parse_json_value("{}").unwrap();
        assert!(v.as_object().unwrap().is_empty());
    }

    #[test]
    fn parse_json_value_empty_array() {
        let v = parse_json_value("[]").unwrap();
        assert!(v.as_array().unwrap().is_empty());
    }

    #[test]
    fn parse_json_value_invalid() {
        let r = parse_json_value("not json");
        match r {
            Err(Error::Serialization(msg)) => {
                assert!(!msg.is_empty());
            }
            other => panic!("expected Serialization error, got {:?}", other),
        }
    }

    #[test]
    fn parse_json_value_truncated() {
        let r = parse_json_value(r#"{"key":"val"#);
        assert!(r.is_err());
    }

    #[test]
    fn parse_json_value_empty_string() {
        let r = parse_json_value("");
        assert!(r.is_err());
    }

    #[test]
    fn parse_json_value_unicode() {
        let v = parse_json_value(r#"{"msg":"日本語"}"#).unwrap();
        assert_eq!(v["msg"], "日本語");
    }

    // ── parse_mcp_content ────────────────────────────────────────────

    #[test]
    fn parse_mcp_content_valid() {
        let raw = r#"{"content_type":"application/json","data":[1,2,3]}"#;
        let c = parse_mcp_content(raw).unwrap();
        assert_eq!(c.content_type, "application/json");
        assert_eq!(c.data, vec![1, 2, 3]);
    }

    #[test]
    fn parse_mcp_content_empty_data() {
        let raw = r#"{"content_type":"text/plain","data":[]}"#;
        let c = parse_mcp_content(raw).unwrap();
        assert!(c.data.is_empty());
    }

    #[test]
    fn parse_mcp_content_missing_field() {
        let raw = r#"{"content_type":"text/plain"}"#;
        let r = parse_mcp_content(raw);
        match r {
            Err(Error::Serialization(_)) => {}
            other => panic!("expected Serialization error, got {:?}", other),
        }
    }

    #[test]
    fn parse_mcp_content_invalid_json() {
        let r = parse_mcp_content("not json");
        assert!(r.is_err());
    }

    #[test]
    fn parse_mcp_content_wrong_type() {
        let raw = r#"{"content_type":42,"data":[]}"#;
        let r = parse_mcp_content(raw);
        assert!(r.is_err());
    }

    #[test]
    fn parse_mcp_content_extra_fields() {
        let raw = r#"{"content_type":"text/html","data":[60],"extra":"ok"}"#;
        let c = parse_mcp_content(raw).unwrap();
        assert_eq!(c.content_type, "text/html");
    }

    #[test]
    fn parse_mcp_content_large_data() {
        let data: Vec<u8> = (0..=255).cycle().take(1000).collect();
        let raw = serde_json::json!({
            "content_type": "application/octet-stream",
            "data": data,
        });
        let c = parse_mcp_content(&raw.to_string()).unwrap();
        assert_eq!(c.data.len(), 1000);
    }

    // ── serialize_tool_request ───────────────────────────────────────

    #[test]
    fn serialize_tool_request_basic() {
        let json = serialize_tool_request("lint", json!({"path": "src/"})).unwrap();
        let v: serde_json::Value = serde_json::from_str(&json).unwrap();
        assert_eq!(v["tool"], "lint");
        assert_eq!(v["params"]["path"], "src/");
    }

    #[test]
    fn serialize_tool_request_empty_params() {
        let json = serialize_tool_request("test", json!({})).unwrap();
        let v: serde_json::Value = serde_json::from_str(&json).unwrap();
        assert_eq!(v["tool"], "test");
        assert!(v["params"].as_object().unwrap().is_empty());
    }

    #[test]
    fn serialize_tool_request_null_params() {
        let json = serialize_tool_request("ping", json!(null)).unwrap();
        let v: serde_json::Value = serde_json::from_str(&json).unwrap();
        assert!(v["params"].is_null());
    }

    #[test]
    fn serialize_tool_request_complex_params() {
        let params = json!({
            "files": ["a.rs", "b.rs"],
            "config": {"strict": true, "level": 3},
            "timeout": 30
        });
        let json = serialize_tool_request("analyze", params.clone()).unwrap();
        let v: serde_json::Value = serde_json::from_str(&json).unwrap();
        assert_eq!(v["params"], params);
    }

    #[test]
    fn serialize_tool_request_empty_tool_name() {
        let json = serialize_tool_request("", json!({})).unwrap();
        let v: serde_json::Value = serde_json::from_str(&json).unwrap();
        assert_eq!(v["tool"], "");
    }

    #[test]
    fn serialize_tool_request_special_chars_in_name() {
        let json = serialize_tool_request("my-tool_v2.0", json!({})).unwrap();
        let v: serde_json::Value = serde_json::from_str(&json).unwrap();
        assert_eq!(v["tool"], "my-tool_v2.0");
    }

    #[test]
    fn serialize_tool_request_unicode_tool_name() {
        let json = serialize_tool_request("ツール", json!({"key": "val"})).unwrap();
        let v: serde_json::Value = serde_json::from_str(&json).unwrap();
        assert_eq!(v["tool"], "ツール");
    }

    #[test]
    fn serialize_tool_request_array_params() {
        let json = serialize_tool_request("batch", json!([1, 2, 3])).unwrap();
        let v: serde_json::Value = serde_json::from_str(&json).unwrap();
        assert_eq!(v["params"], json!([1, 2, 3]));
    }

    #[test]
    fn serialize_tool_request_roundtrip() {
        let json = serialize_tool_request("echo", json!({"msg": "hello"})).unwrap();
        let v: serde_json::Value = serde_json::from_str(&json).unwrap();
        assert_eq!(v["tool"], "echo");
        assert_eq!(v["params"]["msg"], "hello");
    }

    // ── Public API (native stubs → Host error) ──────────────────────

    #[test]
    fn get_metrics_returns_host_error_on_native() {
        let r = get_metrics();
        match r {
            Err(Error::Host(msg)) => {
                assert!(msg.contains("not running in wasm"));
            }
            other => panic!("expected Host error, got {:?}", other),
        }
    }

    #[test]
    fn sign_payload_returns_host_error_on_native() {
        let r = sign_payload("test data");
        match r {
            Err(Error::Host(msg)) => {
                assert!(msg.contains("not running in wasm"));
            }
            other => panic!("expected Host error, got {:?}", other),
        }
    }

    #[test]
    fn sign_payload_empty_input_returns_host_error() {
        let r = sign_payload("");
        assert!(r.is_err());
    }

    #[test]
    fn sign_payload_unicode_input_returns_host_error() {
        let r = sign_payload("日本語テスト");
        assert!(r.is_err());
    }

    #[test]
    fn call_tool_returns_host_error_on_native() {
        let r = call_tool("lint", json!({"path": "src/"}));
        match r {
            Err(Error::Host(msg)) => {
                assert!(msg.contains("not running in wasm"));
            }
            other => panic!("expected Host error, got {:?}", other),
        }
    }

    #[test]
    fn call_tool_serializes_request_before_host_call() {
        // Even though the host call fails, the request must serialize first.
        // We verify that call_tool with valid params returns a Host error
        // (not a Serialization error), proving serialization succeeded.
        let r = call_tool("test", json!({"a": 1}));
        match r {
            Err(Error::Host(_)) => {}
            other => panic!("expected Host error, got {:?}", other),
        }
    }

    #[test]
    fn call_tool_empty_tool_name() {
        let r = call_tool("", json!({}));
        // Serialization succeeds (empty string is valid JSON), so we get Host error
        match r {
            Err(Error::Host(_)) => {}
            other => panic!("expected Host error, got {:?}", other),
        }
    }

    // ── End-to-end processing pipeline ──────────────────────────────

    #[test]
    fn metrics_pipeline_success() {
        // Simulates what get_metrics() does when the host returns valid JSON.
        let raw: Result<String, extism_pdk::Error> =
            Ok(r#"{"invocations":42,"latency_p99":120}"#.into());
        let s = process_host_result(raw).unwrap();
        let v = parse_json_value(&s).unwrap();
        assert_eq!(v["invocations"], 42);
        assert_eq!(v["latency_p99"], 120);
    }

    #[test]
    fn metrics_pipeline_host_error() {
        let raw: Result<String, extism_pdk::Error> =
            Err(extism_pdk::Error::msg("connection refused"));
        let r = process_host_result(raw);
        assert!(matches!(r, Err(Error::Host(_))));
    }

    #[test]
    fn metrics_pipeline_bad_json() {
        let raw: Result<String, extism_pdk::Error> = Ok("not-json".into());
        let s = process_host_result(raw).unwrap();
        let r = parse_json_value(&s);
        assert!(matches!(r, Err(Error::Serialization(_))));
    }

    #[test]
    fn sign_pipeline_success() {
        let raw: Result<String, extism_pdk::Error> =
            Ok("c2lnbmF0dXJl".into()); // base64
        let sig = process_host_result(raw).unwrap();
        assert_eq!(sig, "c2lnbmF0dXJl");
    }

    #[test]
    fn sign_pipeline_host_error() {
        let raw: Result<String, extism_pdk::Error> =
            Err(extism_pdk::Error::msg("key not found"));
        let r = process_host_result(raw);
        match r {
            Err(Error::Host(msg)) => assert!(msg.contains("key not found")),
            other => panic!("expected Host error, got {:?}", other),
        }
    }

    #[test]
    fn tool_pipeline_success() {
        let json = serialize_tool_request("lint", json!({"fix": true})).unwrap();
        assert!(json.contains("lint"));
        assert!(json.contains("fix"));

        let raw: Result<String, extism_pdk::Error> = Ok(
            r#"{"content_type":"application/json","data":[123,125]}"#.into(),
        );
        let s = process_host_result(raw).unwrap();
        let c = parse_mcp_content(&s).unwrap();
        assert_eq!(c.content_type, "application/json");
        assert_eq!(c.data, vec![123, 125]); // {}
    }

    #[test]
    fn tool_pipeline_host_error() {
        let json = serialize_tool_request("unknown", json!({})).unwrap();
        assert!(json.contains("unknown"));

        let raw: Result<String, extism_pdk::Error> =
            Err(extism_pdk::Error::msg("tool not registered"));
        let r = process_host_result(raw);
        assert!(matches!(r, Err(Error::Host(_))));
    }

    #[test]
    fn tool_pipeline_bad_response() {
        let raw: Result<String, extism_pdk::Error> = Ok("garbage".into());
        let s = process_host_result(raw).unwrap();
        let r = parse_mcp_content(&s);
        assert!(matches!(r, Err(Error::Serialization(_))));
    }

    #[test]
    fn tool_pipeline_partial_response() {
        let raw: Result<String, extism_pdk::Error> =
            Ok(r#"{"content_type":"text/plain"}"#.into());
        let s = process_host_result(raw).unwrap();
        let r = parse_mcp_content(&s);
        // Missing "data" field
        assert!(matches!(r, Err(Error::Serialization(_))));
    }
}
