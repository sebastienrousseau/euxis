// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2024-2026 Sebastien Rousseau

use serde::{Deserialize, Serialize};

/// High-level result for MCP tool calls
#[derive(Debug, Serialize, Deserialize)]
pub struct MCPResult {
    pub content: Option<Vec<MCPContent>>,
    pub error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct MCPContent {
    #[serde(rename = "type")]
    pub content_type: String,
    pub text: String,
}

#[derive(Debug, Serialize, Deserialize)]
struct ToolCallRequest {
    pub name: String,
    pub arguments: serde_json::Value,
}

fn first_content_text(result: MCPResult, operation: &str) -> Result<String, String> {
    if let Some(error) = result.error {
        return Err(error);
    }
    if let Some(content) = result.content {
        if let Some(first) = content.first() {
            return Ok(first.text.clone());
        }
    }
    Err(format!("Empty response from {operation}"))
}

/// Call an MCP tool from a Euxis Wasm agent.
///
/// Example:
/// ```no_run
/// let metrics = euxis_sdk_rust::call_tool("get_metrics", serde_json::json!({}));
/// let _ = metrics.ok();
/// ```
pub fn call_tool(name: &str, arguments: serde_json::Value) -> Result<MCPResult, String> {
    let _request = ToolCallRequest {
        name: name.to_string(),
        arguments,
    };
    Err("MCP host bridge is not wired in this SDK build".to_string())
}

fn sign_payload_with<F>(payload: &str, call: F) -> Result<String, String>
where
    F: Fn(&str, serde_json::Value) -> Result<MCPResult, String>,
{
    let result = call("sign_payload", serde_json::json!({"payload": payload}))?;
    first_content_text(result, "sign_payload")
}

/// Sign a payload using Euxis Cryptographic Provenance.
pub fn sign_payload(payload: &str) -> Result<String, String> {
    sign_payload_with(payload, call_tool)
}

fn get_metrics_with<F>(call: F) -> Result<serde_json::Value, String>
where
    F: Fn(&str, serde_json::Value) -> Result<MCPResult, String>,
{
    let result = call("get_metrics", serde_json::json!({}))?;
    let payload = first_content_text(result, "get_metrics")?;
    serde_json::from_str(&payload).map_err(|error| error.to_string())
}

/// Get Euxis mesh metrics.
pub fn get_metrics() -> Result<serde_json::Value, String> {
    get_metrics_with(call_tool)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn text_result(value: &str) -> MCPResult {
        MCPResult {
            content: Some(vec![MCPContent {
                content_type: "text".to_string(),
                text: value.to_string(),
            }]),
            error: None,
        }
    }

    #[test]
    fn call_tool_reports_unwired_bridge() {
        let result = call_tool("noop", serde_json::json!({}));
        assert_eq!(
            result.expect_err("call_tool should fail for unwired host bridge"),
            "MCP host bridge is not wired in this SDK build"
        );
    }

    #[test]
    fn sign_payload_with_returns_signature_text() {
        let signature = sign_payload_with("data", |name, args| {
            assert_eq!(name, "sign_payload");
            assert_eq!(args["payload"], "data");
            Ok(text_result("signed-payload"))
        })
        .expect("signature should parse");
        assert_eq!(signature, "signed-payload");
    }

    #[test]
    fn sign_payload_with_bubbles_tool_error() {
        let error = sign_payload_with("data", |_, _| {
            Ok(MCPResult {
                content: None,
                error: Some("denied".to_string()),
            })
        })
        .expect_err("tool error should be returned");
        assert_eq!(error, "denied");
    }

    #[test]
    fn sign_payload_with_rejects_empty_content() {
        let error = sign_payload_with("data", |_, _| {
            Ok(MCPResult {
                content: Some(vec![]),
                error: None,
            })
        })
        .expect_err("empty content should fail");
        assert_eq!(error, "Empty response from sign_payload");
    }

    #[test]
    fn get_metrics_with_parses_json() {
        let metrics = get_metrics_with(|name, args| {
            assert_eq!(name, "get_metrics");
            assert_eq!(args, serde_json::json!({}));
            Ok(text_result("{\"cpu\": 0.12, \"mem\": 42}"))
        })
        .expect("metrics payload should parse");
        assert_eq!(metrics["cpu"], 0.12);
        assert_eq!(metrics["mem"], 42);
    }

    #[test]
    fn get_metrics_with_rejects_invalid_json() {
        let error = get_metrics_with(|_, _| Ok(text_result("not-json")))
            .expect_err("invalid JSON should return an error");
        assert!(error.contains("expected ident"));
    }

    #[test]
    fn get_metrics_with_propagates_operation_error() {
        let error = get_metrics_with(|_, _| {
            Ok(MCPResult {
                content: None,
                error: Some("forbidden".to_string()),
            })
        })
        .expect_err("operation error should be returned");
        assert_eq!(error, "forbidden");
    }

    #[test]
    fn public_sign_and_metrics_propagate_unwired_bridge_error() {
        let sign_error = sign_payload("x").expect_err("host bridge is not wired");
        assert_eq!(sign_error, "MCP host bridge is not wired in this SDK build");

        let metrics_error = get_metrics().expect_err("host bridge is not wired");
        assert_eq!(metrics_error, "MCP host bridge is not wired in this SDK build");
    }
}
