use thiserror::Error;

#[derive(Error, Debug, PartialEq)]
pub enum Error {
    #[error("MCP operation error: {0}")]
    Operation(String),
    #[error("Empty response from {0}")]
    EmptyResponse(String),
    #[error("MCP host bridge is not wired in this SDK build")]
    UnwiredBridge,
    #[error("Extism PDK error: {0}")]
    Extism(String),
    #[error("Serialization error: {0}")]
    Serialization(String),
    #[error("Deserialization error: {0}")]
    Deserialization(String),
    #[error("JSON error: {0}")]
    Json(String),
}

pub type Result<T> = std::result::Result<T, Error>;
