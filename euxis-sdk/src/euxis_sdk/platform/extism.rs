use crate::euxis_sdk::core::error::{Error, Result};
use crate::euxis_sdk::core::types::{MCPResult, ToolCallRequest, MCPContent};
use extism_pdk::*;
use rmp_serde;

#[host_fn]
extern "ExtismHost" {
    fn host_call_tool(input: Vec<u8>) -> Vec<u8>;
}

fn first_content_text(result: MCPResult, operation: &str) -> Result<String> {
    if let Some(error) = result.error {
        return Err(Error::Operation(error));
    }
    if let Some(content) = result.content {
        if let Some(first) = content.first() {
            return Ok(first.text.clone());
        }
    }
    Err(Error::EmptyResponse(operation.to_string()))
}

/// Call an MCP tool from a Euxis Wasm agent.
pub fn call_tool(name: &str, arguments: serde_json::Value) -> Result<MCPResult> {
    let request = ToolCallRequest {
        name: name.to_string(),
        arguments,
    };
    
    // Serialize request to MessagePack
    let payload = rmp_serde::to_vec_named(&request)
        .map_err(|e| Error::Serialization(e.to_string()))?;
        
    // Call host environment via Extism host_fn
    let host_response = unsafe { host_call_tool(payload) }
        .map_err(|e| Error::Extism(e.to_string()))?;
        
    // Deserialize response from JSON (or could be msgpack too, using json for now based on original impl, wait actually the integration requires us to integrate messagepack over Wasm boundary)
    // Both sides of Wasm boundary are using MessagePack now!
    let mcp_result: MCPResult = rmp_serde::from_slice(&host_response)
        .map_err(|e| Error::Deserialization(e.to_string()))?;
        
    Ok(mcp_result)
}

fn sign_payload_with<F>(payload: &str, call: F) -> Result<String>
where
    F: Fn(&str, serde_json::Value) -> Result<MCPResult>,
{
    let result = call("sign_payload", serde_json::json!({"payload": payload}))?;
    first_content_text(result, "sign_payload")
}

/// Sign a payload using Euxis Cryptographic Provenance.
pub fn sign_payload(payload: &str) -> Result<String> {
    sign_payload_with(payload, call_tool)
}

fn get_metrics_with<F>(call: F) -> Result<serde_json::Value>
where
    F: Fn(&str, serde_json::Value) -> Result<MCPResult>,
{
    let result = call("get_metrics", serde_json::json!({}))?;
    let payload = first_content_text(result, "get_metrics")?;
    serde_json::from_str(&payload).map_err(|e| Error::Json(e.to_string()))
}

/// Get Euxis mesh metrics.
pub fn get_metrics() -> Result<serde_json::Value> {
    get_metrics_with(call_tool)
}
