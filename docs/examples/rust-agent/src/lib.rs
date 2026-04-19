# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

use extism_pdk::*;
use serde::{Deserialize, Serialize};
use euxis_sdk::{get_metrics, sign_payload};

#[derive(Serialize, Deserialize)]
struct Input {
    goal: String,
}

#[derive(Serialize, Deserialize)]
struct Output {
    result: String,
    metrics: Option<serde_json::Value>,
    signature: Option<String>,
}

/// The main entry point for the Euxis Wasm Agent.
/// Triggered by: `euxis-wasm my-agent.wasm '{"goal": "status"}'`
#[plugin_fn]
pub fn run(input: FnMainInput) -> FnMainResult {
    let input: Input = input.json()?;

    // Use the Euxis Rust SDK to call MCP tools
    let metrics = get_metrics().ok();

    let result_text = format!("Agent processed goal: {}", input.goal);

    // Sign our result using Euxis Cryptographic Provenance
    let signature = sign_payload(&result_text).ok();

    let output = Output {
        result: result_text,
        metrics,
        signature,
    };

    Ok(FnMainOutput::json(output)?)
}
