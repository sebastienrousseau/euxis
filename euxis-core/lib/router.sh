#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors
#
# router.sh — Intelligent model routing for cost optimization
#
# Implements multi-tier model selection based on:
#   1. Agent capability tags (from registry)
#   2. Task complexity analysis (ClawRouter)
#   3. Local model fallback (Ollama)
#   4. Prompt caching hints
#
# Cost tiers (Feb 2026 estimates per 1M tokens):
#   TIER_ROUTINE:  ~$0.10  (Gemini Flash-Lite, local models)
#   TIER_DATA:     ~$0.27  (DeepSeek-V3, Haiku 3.5)
#   TIER_CODE:     ~$3.00  (GPT-5.2 Turbo, Sonnet 3.7)
#   TIER_REASON:   ~$15.00 (Claude Opus 4.6)
#
# Usage:
#   source "${EUXIS_HOME}/euxis-core/lib/router.sh"
#   model=$(router_select_model "architect" "Design a new API")
#   router_should_use_local "heartbeat check"  # returns 0 if local OK

# ============================================================================
# Configuration
# ============================================================================

EUXIS_HOME="${EUXIS_HOME:-$HOME/.euxis}"
ROUTER_REGISTRY="${EUXIS_HOME}/euxis-core/agents/registry.json"
ROUTER_CONFIG="${EUXIS_HOME}/euxis-core/config/router.json"

# Default models per tier (can be overridden via router.json or env)
ROUTER_MODEL_ROUTINE="${EUXIS_MODEL_ROUTINE:-gemini-2.5-flash-lite}"
ROUTER_MODEL_DATA="${EUXIS_MODEL_DATA:-deepseek-v3}"
ROUTER_MODEL_CODE="${EUXIS_MODEL_CODE:-claude-sonnet-4}"
ROUTER_MODEL_REASON="${EUXIS_MODEL_REASON:-claude-opus-4}"

# Local model settings
ROUTER_LOCAL_MODEL="${EUXIS_LOCAL_MODEL:-qwen3:32b}"
ROUTER_LOCAL_ENDPOINT="${EUXIS_LOCAL_ENDPOINT:-http://localhost:11434}"

# Prompt caching settings
ROUTER_CACHE_TTL="${EUXIS_CACHE_TTL:-3300}"  # 55 minutes (under 60min TTL)
ROUTER_CACHE_MIN_TOKENS="${EUXIS_CACHE_MIN_TOKENS:-1024}"

# ============================================================================
# Capability-to-Tier Mapping
# ============================================================================

# Map capability tags to cost tiers
# Returns: routine | data | code | reason
router_capability_tier() {
    local capability="$1"

    case "$capability" in
        # Routine tier - heartbeats, status, simple checks
        heartbeat|status|ping|health|monitor)
            echo "routine"
            ;;

        # Data tier - formatting, parsing, transformation
        formatting|parsing|logging|metrics|telemetry|localization|i18n)
            echo "data"
            ;;

        # Code tier - development, testing, debugging
        code-review|debugging|testing|refactoring|documentation|api-design)
            echo "code"
            ;;

        # Reason tier - architecture, planning, security, research
        architecture|planning|security|research|audit|synthesis|strategy)
            echo "reason"
            ;;

        # Default to code tier for unknown capabilities
        *)
            echo "code"
            ;;
    esac
}

# Get agent's primary tier based on capability tags
# Usage: router_agent_tier "architect" → "reason"
router_agent_tier() {
    local agent_id="$1"

    if [[ ! -f "$ROUTER_REGISTRY" ]]; then
        echo "code"  # Safe default
        return
    fi

    # Get agent's capability tags
    local capabilities
    capabilities=$(jq -r --arg id "$agent_id" \
        '.agents[] | select(.id == $id) | .capability_tags // [] | .[]' \
        "$ROUTER_REGISTRY" 2>/dev/null)

    if [[ -z "$capabilities" ]]; then
        echo "code"
        return
    fi

    # Find highest tier among capabilities
    local highest_tier="routine"
    local tier_rank=0

    while IFS= read -r cap; do
        local tier
        tier=$(router_capability_tier "$cap")

        local rank=0
        case "$tier" in
            routine) rank=1 ;;
            data)    rank=2 ;;
            code)    rank=3 ;;
            reason)  rank=4 ;;
        esac

        if [[ $rank -gt $tier_rank ]]; then
            tier_rank=$rank
            highest_tier="$tier"
        fi
    done <<< "$capabilities"

    echo "$highest_tier"
}

# ============================================================================
# ClawRouter: Task Complexity Analysis
# ============================================================================

# Analyze task string for complexity indicators
# Returns: routine | data | code | reason
router_analyze_task() {
    local task="$1"
    local task_lower
    task_lower=$(echo "$task" | tr '[:upper:]' '[:lower:]')

    # Reason tier keywords (highest priority)
    if echo "$task_lower" | grep -qE \
        'architect|design|plan|strategy|security audit|research|analyze complex|mission|synthesize'; then
        echo "reason"
        return
    fi

    # Code tier keywords
    if echo "$task_lower" | grep -qE \
        'fix|debug|implement|refactor|write code|test|review|api|function|class|module'; then
        echo "code"
        return
    fi

    # Data tier keywords
    if echo "$task_lower" | grep -qE \
        'format|parse|log|metric|convert|transform|translate|summarize|extract'; then
        echo "data"
        return
    fi

    # Routine tier keywords
    if echo "$task_lower" | grep -qE \
        'check|status|health|ping|heartbeat|verify|confirm|list|count'; then
        echo "routine"
        return
    fi

    # Default to code tier
    echo "code"
}

# ============================================================================
# Model Selection
# ============================================================================

# Get model for a specific tier
# Usage: router_tier_model "code" → "claude-sonnet-4"
router_tier_model() {
    local tier="$1"

    # Check for config file overrides
    if [[ -f "$ROUTER_CONFIG" ]]; then
        local configured
        configured=$(jq -r --arg t "$tier" '.models[$t] // empty' "$ROUTER_CONFIG" 2>/dev/null)
        if [[ -n "$configured" ]]; then
            echo "$configured"
            return
        fi
    fi

    # Use environment/defaults
    case "$tier" in
        routine) echo "$ROUTER_MODEL_ROUTINE" ;;
        data)    echo "$ROUTER_MODEL_DATA" ;;
        code)    echo "$ROUTER_MODEL_CODE" ;;
        reason)  echo "$ROUTER_MODEL_REASON" ;;
        *)       echo "$ROUTER_MODEL_CODE" ;;
    esac
}

# Select optimal model for agent + task combination
# Usage: model=$(router_select_model "architect" "Design a new API")
router_select_model() {
    local agent_id="$1"
    local task="$2"

    # Check for explicit override
    if [[ -n "${EUXIS_MODEL_OVERRIDE:-}" ]]; then
        echo "$EUXIS_MODEL_OVERRIDE"
        return
    fi

    # Get tiers from agent capabilities and task analysis
    local agent_tier task_tier
    agent_tier=$(router_agent_tier "$agent_id")
    task_tier=$(router_analyze_task "$task")

    # Use the higher tier (more capable model)
    local final_tier="$agent_tier"

    local agent_rank=0 task_rank=0
    case "$agent_tier" in
        routine) agent_rank=1 ;;
        data)    agent_rank=2 ;;
        code)    agent_rank=3 ;;
        reason)  agent_rank=4 ;;
    esac
    case "$task_tier" in
        routine) task_rank=1 ;;
        data)    task_rank=2 ;;
        code)    task_rank=3 ;;
        reason)  task_rank=4 ;;
    esac

    if [[ $task_rank -gt $agent_rank ]]; then
        final_tier="$task_tier"
    fi

    router_tier_model "$final_tier"
}

# ============================================================================
# Local Model Fallback (Ollama)
# ============================================================================

# Check if Ollama is available
router_ollama_available() {
    curl -s --connect-timeout 2 "${ROUTER_LOCAL_ENDPOINT}/api/tags" &>/dev/null
}

# Check if task should use local model
# Usage: if router_should_use_local "$task"; then ...
# Note: Auto-fallback is disabled by default (local models can be slow).
#       Set EUXIS_LOCAL_ONLY=true to force local, or enable in router.json.
router_should_use_local() {
    local task="$1"

    # Check explicit flag - this always works if Ollama is available
    if [[ "${EUXIS_LOCAL_ONLY:-false}" == "true" ]]; then
        router_ollama_available && return 0
        return 1
    fi

    # Check if auto-fallback is enabled in config
    local auto_enabled="false"
    if [[ -f "$ROUTER_CONFIG" ]]; then
        auto_enabled=$(jq -r '.local.auto_fallback // .local.enabled // false' "$ROUTER_CONFIG" 2>/dev/null)
    fi

    # Only auto-fallback if explicitly enabled (disabled by default for performance)
    if [[ "$auto_enabled" != "true" ]]; then
        return 1
    fi

    # Check if task is routine and Ollama is available
    local tier
    tier=$(router_analyze_task "$task")

    if [[ "$tier" == "routine" ]] && router_ollama_available; then
        return 0
    fi

    return 1
}

# Get local model name
router_local_model() {
    echo "$ROUTER_LOCAL_MODEL"
}

# ============================================================================
# Prompt Caching
# ============================================================================

# Generate cache control hint for Anthropic API
# Usage: router_cache_hint "system_prompt" → JSON fragment
router_anthropic_cache_hint() {
    local content="$1"
    local token_estimate

    # Rough token estimate (4 chars per token)
    token_estimate=$((${#content} / 4))

    if [[ $token_estimate -ge $ROUTER_CACHE_MIN_TOKENS ]]; then
        echo '{"cache_control": {"type": "ephemeral"}}'
    else
        echo '{}'
    fi
}

# Check if we should send a cache warmup heartbeat
# Usage: if router_should_warmup; then send_heartbeat; fi
router_should_warmup() {
    local cache_file="${EUXIS_HOME}/euxis-runtime/cache/.last_warmup"

    if [[ ! -f "$cache_file" ]]; then
        return 0  # Never warmed up
    fi

    local last_warmup now elapsed
    last_warmup=$(cat "$cache_file")
    now=$(date +%s)
    elapsed=$((now - last_warmup))

    # Warmup if within 5 minutes of TTL expiry
    if [[ $elapsed -gt $((ROUTER_CACHE_TTL - 300)) ]]; then
        return 0
    fi

    return 1
}

# Record cache warmup timestamp
router_record_warmup() {
    local cache_dir="${EUXIS_HOME}/euxis-runtime/cache"
    mkdir -p "$cache_dir"
    date +%s > "$cache_dir/.last_warmup"
}

# ============================================================================
# Manifest Enhancement
# ============================================================================

# Enhance manifest with model assignments
# Usage: router_enhance_manifest "manifest.json" → enhanced JSON
router_enhance_manifest() {
    local manifest="$1"

    if [[ ! -f "$manifest" ]]; then
        echo "{}"
        return 1
    fi

    # Add model field to each dispatch based on routing
    jq '
        .dispatches = [.dispatches[] |
            . + {
                "model": (
                    if .model then .model
                    else "auto"
                    end
                ),
                "tier": (
                    if .tier then .tier
                    else "code"
                    end
                )
            }
        ]
    ' "$manifest"
}

# ============================================================================
# Status / Reporting
# ============================================================================

# Print router configuration status
router_status() {
    echo "┌─────────────────────────────────────────────"
    echo "│ Euxis Router Configuration"
    echo "├─────────────────────────────────────────────"
    echo "│ Tier Models:"
    echo "│   Routine: $ROUTER_MODEL_ROUTINE"
    echo "│   Data:    $ROUTER_MODEL_DATA"
    echo "│   Code:    $ROUTER_MODEL_CODE"
    echo "│   Reason:  $ROUTER_MODEL_REASON"
    echo "├─────────────────────────────────────────────"
    echo "│ Local Fallback:"
    local auto_enabled="false"
    if [[ -f "$ROUTER_CONFIG" ]]; then
        auto_enabled=$(jq -r '.local.auto_fallback // .local.enabled // false' "$ROUTER_CONFIG" 2>/dev/null)
    fi
    if router_ollama_available; then
        echo "│   Ollama:  ✓ Available at $ROUTER_LOCAL_ENDPOINT"
        echo "│   Model:   $ROUTER_LOCAL_MODEL"
        if [[ "$auto_enabled" == "true" ]]; then
            echo "│   Auto:    Enabled (routine tasks use local)"
        else
            echo "│   Auto:    Disabled (use EUXIS_LOCAL_ONLY=true to force)"
        fi
    else
        echo "│   Ollama:  ✗ Not available"
    fi
    echo "├─────────────────────────────────────────────"
    echo "│ Prompt Caching:"
    echo "│   TTL:         ${ROUTER_CACHE_TTL}s (warmup at $((ROUTER_CACHE_TTL - 300))s)"
    echo "│   Min Tokens:  $ROUTER_CACHE_MIN_TOKENS"
    if router_should_warmup; then
        echo "│   Status:      Needs warmup"
    else
        echo "│   Status:      Cache warm"
    fi
    echo "└─────────────────────────────────────────────"
}

# Export estimated cost for a model
# Usage: router_cost_estimate "claude-opus-4" "100000" → "$1.50"
router_cost_estimate() {
    local model="$1"
    local tokens="${2:-1000000}"

    # Cost per 1M tokens (input, approximate)
    local cost_per_million
    case "$model" in
        *flash-lite*|*gemini*flash*)  cost_per_million="0.10" ;;
        *deepseek*|*haiku*)           cost_per_million="0.27" ;;
        *sonnet*|*turbo*)             cost_per_million="3.00" ;;
        *opus*)                       cost_per_million="15.00" ;;
        *local*|*ollama*|*qwen*|*llama*) cost_per_million="0.00" ;;
        *)                            cost_per_million="3.00" ;;
    esac

    # Calculate
    awk "BEGIN {printf \"\$%.2f\", ($tokens / 1000000) * $cost_per_million}"
}
