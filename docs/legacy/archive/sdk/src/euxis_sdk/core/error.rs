use thiserror::Error;

/// Core domain errors specific to the Euxis Platform abstractions.
#[derive(Error, Debug, PartialEq)]
pub enum Error {
    /// Native tool call failed gracefully returning a managed protocol error message
    #[error("MCP operation error: {0}")]
    Operation(String),
    /// Function execution unexpectedly returned a zero-sized or absent response stream
    #[error("Empty response from {0}")]
    EmptyResponse(String),
    /// Host function `host_call_tool` implementation does not exist in standard local tests natively
    #[error("MCP host bridge is not wired in this SDK build")]
    UnwiredBridge,
    /// Memory segmentation fault triggered via Extism boundaries
    #[error("Extism PDK error: {0}")]
    Extism(String),
    /// MessagePack format serialization failed before dispatching payload
    #[error("Serialization error: {0}")]
    Serialization(String),
    /// Returned payload format from host could not be deserialized natively from MessagePack
    #[error("Deserialization error: {0}")]
    Deserialization(String),
    /// Fallback JSON parsing error for textual tools like `get_metrics`
    #[error("JSON error: {0}")]
    Json(String),
}

/// Convenience `Result` type wrapping `euxis_sdk::core::error::Error`.
pub type Result<T> = std::result::Result<T, Error>;
