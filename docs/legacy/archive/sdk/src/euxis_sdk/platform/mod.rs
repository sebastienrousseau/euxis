//! Platform abstractions for the host environment executing the agent.

/// Extism PDK host mappings enabling WebAssembly to interact with the host C++ core.
pub mod extism;
pub use extism::{call_tool, call_tool_with, get_metrics, sign_payload};
