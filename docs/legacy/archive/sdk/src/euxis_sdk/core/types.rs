use serde::{Deserialize, Serialize};

/// High-level result containing structured tools outputs via MCP interface.
#[derive(Debug, Serialize, Deserialize, PartialEq)]
pub struct MCPResult {
    /// An optional array of structured content items returned by the invoked tool.
    pub content: Option<Vec<MCPContent>>,
    /// An explicit operational fault string if the tool successfully invoked but failed internally.
    pub error: Option<String>,
}

/// Abstract item returned from an execution stream natively (e.g., text, metadata, blobs).
#[derive(Debug, Serialize, Deserialize, PartialEq)]
pub struct MCPContent {
    /// Semantic structural string marking type. Usually `"text"`
    #[serde(rename = "type")]
    pub content_type: String,
    /// Human-readable textual payload
    pub text: String,
}

/// Request serialization envelope binding a precise tool target with typed semantic arguments.
#[derive(Debug, Serialize, Deserialize, PartialEq)]
pub struct ToolCallRequest<T> {
    /// Registered target tool string (e.g. `"sign_payload"`)
    pub name: String,
    /// Arguments payload natively typed explicitly without JSON overhead if possible
    pub arguments: T,
}
