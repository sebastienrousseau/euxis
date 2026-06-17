// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2024-2026 Sebastien Rousseau · https://sebastienrousseau.com

#![warn(missing_docs)]

//! # Euxis SDK
//!
//! The Euxis Rust SDK provides types and host bindings for building
//! WebAssembly agents that run inside the Euxis runtime.
//!
//! ## Architecture
//!
//! The SDK is split into two modules:
//!
//! - [`core`] — platform-agnostic error types and data structures.
//! - [`platform`] — host function bindings that call back into the
//!   Euxis runtime (metrics, signing, MCP tool invocation).
//!
//! ## Quick start
//!
//! ```no_run
//! use euxis_sdk::{get_metrics, sign_payload, call_tool};
//!
//! # fn main() -> euxis_sdk::MCPResult<()> {
//! let metrics = get_metrics()?;
//! let sig = sign_payload("hello")?;
//! let result = call_tool("lint", serde_json::json!({"path": "src/"}))?;
//! # Ok(())
//! # }
//! ```

pub mod core;
pub mod platform;

// Re-exports for convenience.
pub use core::{Error, MCPContent, MCPResult};
pub use platform::{call_tool, get_metrics, sign_payload};
