//! Extism platform bindings providing native execution boundaries across the C++ host.

use crate::euxis_sdk::core::error::{Error, Result};
use crate::euxis_sdk::core::types::{MCPResult, ToolCallRequest};
use serde::Serialize;
use rmp_serde;

#[cfg(not(any(test, feature = "test-mocks")))]
#[extism_pdk::host_fn]
extern "ExtismHost" {
    fn host_call_tool(input: Vec<u8>) -> Vec<u8>;
}

#[cfg(any(test, feature = "test-mocks"))]
pub mod mock {
    //! Mocks for Extism environment to allow integration testing
    
    thread_local! {
        /// Thread local store representing Extism memory host returns
        pub static MOCK_RESPONSE: std::cell::RefCell<Option<Vec<u8>>> = std::cell::RefCell::new(None);
        /// Thread local store representing Extism memory host FFI error
        pub static MOCK_ERROR: std::cell::RefCell<Option<String>> = std::cell::RefCell::new(None);
    }

    /// Extism FFI mock equivalent mapping payload returns natively
    pub unsafe fn host_call_tool(_input: Vec<u8>) -> std::result::Result<Vec<u8>, extism_pdk::Error> {
        match MOCK_ERROR.with(|m| m.borrow().clone()) {
            Some(err) => Err(extism_pdk::Error::msg(err)),
            None => Ok(MOCK_RESPONSE.with(|m| {
                m.borrow().clone().unwrap_or_default()
            })),
        }
    }
    
    /// Test wrapper exposing first_content_text for integration tests
    pub fn mock_first_content_text(result: crate::euxis_sdk::core::types::MCPResult, operation: &str) -> crate::euxis_sdk::core::error::Result<String> {
        super::first_content_text(result, operation)
    }
}
#[cfg(any(test, feature = "test-mocks"))]
use mock::host_call_tool;

pub(crate) fn first_content_text(result: MCPResult, operation: &str) -> Result<String> {
    if let Some(error) = result.error {
        return Err(Error::Operation(error));
    }
    
    result.content
        .and_then(|mut c| c.pop())
        .map(|content| content.text)
        .ok_or_else(|| Error::EmptyResponse(operation.to_string()))
}

fn serialize_request<T: Serialize>(request: &ToolCallRequest<T>) -> Result<Vec<u8>> {
    rmp_serde::to_vec_named(request).map_err(|e| Error::Serialization(e.to_string()))
}

fn deserialize_response(response: &[u8]) -> Result<MCPResult> {
    rmp_serde::from_slice(response).map_err(|e| Error::Deserialization(e.to_string()))
}

/// Call an MCP tool from a Euxis Wasm agent with strongly-typed arguments preventing JSON overhead.
pub fn call_tool_with<T: Serialize>(name: &str, arguments: &T) -> Result<MCPResult> {
    let request = ToolCallRequest {
        name: name.to_string(),
        arguments,
    };
    
    // Serialize request directly to MessagePack to avoid intermediate allocation
    let payload = serialize_request(&request)?;
        
    // Call host environment via Extism host_fn
    let host_response = match unsafe { host_call_tool(payload) } {
        Ok(res) => res,
        Err(e) => return Err(Error::Extism(e.to_string())),
    };
        
    // Deserialize response directly from MessagePack
    let mcp_result: MCPResult = deserialize_response(&host_response)?;
        
    Ok(mcp_result)
}

/// Call an MCP tool from a Euxis Wasm agent using fallback JSON values.
pub fn call_tool(name: &str, arguments: serde_json::Value) -> Result<MCPResult> {
    call_tool_with(name, &arguments)
}

/// Sign a payload using Euxis Cryptographic Provenance.
pub fn sign_payload(payload: &str) -> Result<String> {
    match call_tool("sign_payload", serde_json::json!({ "payload": payload })) {
        Ok(result) => first_content_text(result, "sign_payload"),
        Err(e) => Err(e),
    }
}

/// Get Euxis mesh metrics.
pub fn get_metrics() -> Result<serde_json::Value> {
    let result = match call_tool("get_metrics", serde_json::json!({})) {
        Ok(r) => r,
        Err(e) => return Err(e),
    };
    
    let payload = match first_content_text(result, "get_metrics") {
        Ok(p) => p,
        Err(e) => return Err(e),
    };
    
    match serde_json::from_str(&payload) {
        Ok(val) => Ok(val),
        Err(e) => Err(Error::Json(e.to_string())),
    }
}



