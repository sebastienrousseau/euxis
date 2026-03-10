use serde::{Deserialize, Serialize};

/// High-level result for MCP tool calls
#[derive(Debug, Serialize, Deserialize, PartialEq)]
pub struct MCPResult {
    pub content: Option<Vec<MCPContent>>,
    pub error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize, PartialEq)]
pub struct MCPContent {
    #[serde(rename = "type")]
    pub content_type: String,
    pub text: String,
}

#[derive(Debug, Serialize, Deserialize, PartialEq)]
pub struct ToolCallRequest {
    pub name: String,
    pub arguments: serde_json::Value,
}
