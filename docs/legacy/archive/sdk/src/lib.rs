// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2024-2026 Sebastien Rousseau

#![deny(missing_docs)]
//! Euxis Rust SDK for building Wasm Agents with MCP support.
//! 
//! This crate provides the Extism PDK interfaces securely mapped over the
//! Model Context Protocol (MCP) using MessagePack semantics for minimal footprint.

/// The principal SDK abstraction hierarchy exported natively
pub mod euxis_sdk;

pub use euxis_sdk::core::error::{Result, Error};
pub use euxis_sdk::core::types::{MCPResult, MCPContent};
pub use euxis_sdk::platform::{call_tool, sign_payload, get_metrics};

