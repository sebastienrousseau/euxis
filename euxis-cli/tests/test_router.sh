#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors
#
# test_router.sh — Tests for cost optimization router
#
# Usage: ./test_router.sh

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EUXIS_HOME="${SCRIPT_DIR}/../.."
export EUXIS_HOME

# Source router library
source "${EUXIS_HOME}/euxis-core/lib/router.sh"

PASS=0
FAIL=0

test_case() {
    local name="$1"
    shift
    if "$@" 2>/dev/null; then
        echo "  [PASS] $name"
        ((PASS++)) || true
    else
        echo "  [FAIL] $name"
        ((FAIL++)) || true
    fi
}

echo "=== Router Tests ==="
echo ""

# ============================================================================
# Capability-to-Tier Mapping
# ============================================================================
echo "--- Capability Tier Mapping ---"

test_case "heartbeat → routine" \
    [ "$(router_capability_tier heartbeat)" = "routine" ]

test_case "formatting → data" \
    [ "$(router_capability_tier formatting)" = "data" ]

test_case "code-review → code" \
    [ "$(router_capability_tier code-review)" = "code" ]

test_case "architecture → reason" \
    [ "$(router_capability_tier architecture)" = "reason" ]

test_case "unknown → code (default)" \
    [ "$(router_capability_tier unknown-capability)" = "code" ]

echo ""

# ============================================================================
# Task Complexity Analysis
# ============================================================================
echo "--- Task Analysis (ClawRouter) ---"

test_case "health check → routine" \
    [ "$(router_analyze_task 'check health status')" = "routine" ]

test_case "format data → data" \
    [ "$(router_analyze_task 'format and parse the JSON log files')" = "data" ]

test_case "fix bug → code" \
    [ "$(router_analyze_task 'fix the authentication bug')" = "code" ]

test_case "debug issue → code" \
    [ "$(router_analyze_task 'debug the memory leak')" = "code" ]

test_case "architect system → reason" \
    [ "$(router_analyze_task 'architect a new API design')" = "reason" ]

test_case "security audit → reason" \
    [ "$(router_analyze_task 'perform security audit')" = "reason" ]

test_case "generic task → code (default)" \
    [ "$(router_analyze_task 'do something')" = "code" ]

echo ""

# ============================================================================
# Model Selection
# ============================================================================
echo "--- Model Selection ---"

test_case "tier model: routine → gemini" \
    [ "$(router_tier_model routine)" = "gemini-2.5-flash-lite" ]

test_case "tier model: data → deepseek" \
    [ "$(router_tier_model data)" = "deepseek-v3" ]

test_case "tier model: code → sonnet" \
    [ "$(router_tier_model code)" = "claude-sonnet-4" ]

test_case "tier model: reason → opus" \
    [ "$(router_tier_model reason)" = "claude-opus-4" ]

echo ""

# ============================================================================
# Override Behavior
# ============================================================================
echo "--- Override Behavior ---"

# Test model override
export EUXIS_MODEL_OVERRIDE="custom-model"
test_case "EUXIS_MODEL_OVERRIDE takes precedence" \
    [ "$(router_select_model architect 'design system')" = "custom-model" ]
unset EUXIS_MODEL_OVERRIDE

echo ""

# ============================================================================
# Cost Estimation
# ============================================================================
echo "--- Cost Estimation ---"

test_case "routine model cost ~\$0.10/1M" \
    [ "$(router_cost_estimate gemini-2.5-flash-lite 1000000)" = "\$0.10" ]

test_case "reason model cost ~\$15.00/1M" \
    [ "$(router_cost_estimate claude-opus-4 1000000)" = "\$15.00" ]

test_case "local model cost \$0.00" \
    [ "$(router_cost_estimate qwen3:32b 1000000)" = "\$0.00" ]

echo ""

# ============================================================================
# Prompt Caching
# ============================================================================
echo "--- Prompt Caching ---"

# Small content should not get cache hint
small_content="Hello world"
cache_hint=$(router_anthropic_cache_hint "$small_content")
test_case "small content: no cache hint" \
    [ "$cache_hint" = "{}" ]

# Large content should get cache hint
large_content=$(printf '%*s' 5000 '' | tr ' ' 'x')
cache_hint=$(router_anthropic_cache_hint "$large_content")
test_case "large content: has cache hint" \
    [ "$cache_hint" = '{"cache_control": {"type": "ephemeral"}}' ]

echo ""

# ============================================================================
# Summary
# ============================================================================
echo "==================================="
echo "  Router Tests: $PASS passed, $FAIL failed"
echo "==================================="

if [[ $FAIL -gt 0 ]]; then
    exit 1
fi
