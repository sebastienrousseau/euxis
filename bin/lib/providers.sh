#!/usr/bin/env bash
# Euxis Library: Provider routing and execution
[[ -n "${_EUXIS_LIB_PROVIDERS:-}" ]] && return; _EUXIS_LIB_PROVIDERS=1

EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"

source "${EUXIS_HOME}/bin/lib/common.sh"

# Default provider (fallback when no tiering match)
DEFAULT_PROVIDER="claude"

# Configurable timeouts for provider API calls (in seconds)
EUXIS_API_TIMEOUT="${EUXIS_API_TIMEOUT:-300}"  # 5 minutes default

# ============================================================================
# Intelligence Tiering (v4.7)
# Maps agent -> optimal provider based on task complexity profile.
# Explicit provider argument always overrides tiering.
# P0 tasks always use Strategic tier (claude) regardless of agent.
# ============================================================================

resolve_tiered_provider() {
    local agent="$1"
    local priority="${2:-}"
    # P0 tasks ALWAYS use Strategic tier (claude) regardless of agent
    if [[ "$priority" == "P0" ]]; then
        echo "claude"
        return
    fi
    case "${agent}" in
        # S-Tier: Strategic — best-in-class reasoning and tool use
        orchestrator|architect|product-manager|reviewer)
            echo "claude" ;;
        # A-Tier: Research — massive context window for deep analysis
        deep-researcher|compliance-officer)
            echo "gemini" ;;
        # A-Tier: Enterprise — corporate security contexts
        incident-commander)
            echo "kiro-cli" ;;
        # B-Tier: Coding — agent-native tool use for developer workflows
        bug-fixer|unit-tester|automation-engineer)
            echo "goose" ;;
        # B-Tier: Local Code — specialized local models for diffs and migrations
        legacy-maintainer)
            echo "goose" ;;
        # A-Tier: Domain Specialists — require deep reasoning for domain correctness
        crypto-cryptography-auditor|payments-domain-steward)
            echo "claude" ;;
        # B-Tier: Realtime/Systems — specialized tool use for systems work
        realtime-audio-engineer|rust-crate-steward)
            echo "goose" ;;
        # B-Tier: Math/Logic — strong at algorithmic and optimization tasks
        perf-optimizer|data-steward)
            echo "qwen" ;;
        # C-Tier: Utility — zero latency, no cost for summaries and formatting
        butler|librarian|tech-writer)
            echo "ollama" ;;
        # Default (Standard): fall back to primary provider
        *)
            echo "${DEFAULT_PROVIDER}" ;;
    esac
}

# Resolve provider-specific model name and CLI flags.
# Populates: PROVIDER_MODEL, PROVIDER_FLAGS
resolve_provider_config() {
    local provider="$1"

    case "${provider}" in
        claude)   # The Strategist (Primary)
            PROVIDER_MODEL="claude-sonnet-4-20250514"
            PROVIDER_FLAGS="--cache-prompt --stream"
            ;;
        gemini)   # The Researcher (Massive Context)
            PROVIDER_MODEL="gemini-2.0-flash"
            PROVIDER_FLAGS="--search-web"
            ;;
        openai)   # The Generalist
            PROVIDER_MODEL="gpt-4o"
            PROVIDER_FLAGS="--stream"
            ;;
        ollama)   # The Local Assistant (Fast, zero latency)
            PROVIDER_MODEL="${EUXIS_OLLAMA_MODEL:-llama3.2}"
            PROVIDER_FLAGS="--local"
            ;;
        qwen)     # Alibaba Qwen3-Coder (Open Source, 256K context)
            PROVIDER_MODEL="${EUXIS_QWEN_MODEL:-qwen3-coder}"
            PROVIDER_FLAGS=""
            ;;
        crush)    # Charm Crush (Multi-model TUI agent)
            PROVIDER_MODEL="${EUXIS_CRUSH_MODEL:-claude-sonnet-4}"
            PROVIDER_FLAGS=""
            ;;
        kiro-cli) # Kiro CLI (AI coding assistant)
            PROVIDER_MODEL="kiro-cli"
            PROVIDER_FLAGS=""
            ;;
        goose)    # Block Goose (Open source, MCP-native agent)
            PROVIDER_MODEL="${EUXIS_GOOSE_MODEL:-claude-sonnet-4}"
            PROVIDER_FLAGS=""
            ;;
    esac
}

# Wrapper function to add configurable timeout to provider commands
run_with_timeout() {
    local timeout_seconds="$1"
    shift
    local command_description="$1"
    shift

    if command -v timeout &>/dev/null; then
        # GNU timeout available (Linux, installed via brew on macOS)
        if ! timeout "${timeout_seconds}s" "$@"; then
            local exit_code=$?
            if [[ $exit_code -eq 124 ]]; then
                log_error "Provider command timed out after ${timeout_seconds}s: ${command_description}"
                log_error "Consider increasing EUXIS_API_TIMEOUT or using a different provider"
                return 1
            fi
            return $exit_code
        fi
    elif command -v gtimeout &>/dev/null; then
        # GNU timeout via coreutils on macOS
        if ! gtimeout "${timeout_seconds}s" "$@"; then
            local exit_code=$?
            if [[ $exit_code -eq 124 ]]; then
                log_error "Provider command timed out after ${timeout_seconds}s: ${command_description}"
                log_error "Consider increasing EUXIS_API_TIMEOUT or using a different provider"
                return 1
            fi
            return $exit_code
        fi
    else
        # No timeout command available - run without timeout but log warning
        log_warning "timeout command not available - running without timeout protection"
        "$@"
    fi
}

run_claude() {
    local full_prompt="$1"

    # Check for optional dependencies and adjust tools accordingly
    local allowed_tools="Read,Edit,Write,Bash(grep:*) Bash(find:*) Bash(python3:*) Bash(pytest:*) Bash(cat:*) Bash(ls:*) Bash(test:*) Bash(head:*) Bash(tail:*) Bash(wc:*) Bash(mkdir:*) Bash(touch:*) Bash(pip:*) Bash(uv:*)"

    # Add jq support if available, otherwise warn
    if command -v jq &>/dev/null; then
        allowed_tools="${allowed_tools} Bash(jq:*)"
    else
        log_warning "jq not available - JSON processing may be limited"
    fi

    # Add gh support if available, otherwise warn
    if command -v gh &>/dev/null; then
        allowed_tools="${allowed_tools} Bash(gh:*)"
    else
        log_warning "gh CLI not available - GitHub operations may be limited"
    fi

    # Use timeout wrapper for the claude command
    echo "${full_prompt}" | run_with_timeout "${EUXIS_API_TIMEOUT}" "claude API" \
        claude \
        --print \
        --model "${PROVIDER_MODEL}" \
        --tools "Read,Edit,Write,Bash" \
        --allowedTools "${allowed_tools}" \
        --dangerously-skip-permissions \
        --max-turns "${EUXIS_MAX_TURNS:-25}"
}

run_gemini() {
    local full_prompt="$1"
    if command -v gemini &>/dev/null; then
        local guard
        guard="IMPORTANT: This is plain-text mode. Do not call tools or output ACTION/OBSERVATION steps. Respond only with the final answer in the required Output Format."
        local err_file
        err_file=$(mktemp 2>/dev/null || echo "/tmp/euxis_gemini_err.$$")
        local out
        out=$(printf '%s\n\n%s' "${guard}" "${full_prompt}" | NODE_OPTIONS="--no-deprecation" \
            gemini --output-format text -p "" 2> "${err_file}")
        local rc=$?
        if [[ -s "${err_file}" ]]; then
            grep -vE '^(Error executing tool|Tool execution denied by policy|Tool ".*" not found in registry|Loaded cached credentials|Hook registry initialized|\\(node:.*\\) \\[DEP0040\\])' "${err_file}" >&2 || true
        fi
        rm -f "${err_file}"
        printf '%s' "${out}" | grep -vE '^(Error executing tool|Tool execution denied by policy|Tool ".*" not found in registry|Loaded cached credentials|Hook registry initialized)$' || true
        return "${rc}"
    else
        log_error "Gemini CLI not found. Install it or use a different provider."
        exit 1
    fi
}

run_openai() {
    local full_prompt="$1"
    if command -v codex &>/dev/null; then
        echo "${full_prompt}" | codex --model "${PROVIDER_MODEL}"
    else
        log_error "codex (OpenAI Codex CLI) not found. Install via: npm i -g @openai/codex"
        exit 1
    fi
}

run_ollama() {
    local full_prompt="$1"
    if command -v ollama &>/dev/null; then
        echo "${full_prompt}" | ollama run "${PROVIDER_MODEL}"
    else
        log_error "ollama not found. Install from https://ollama.com or use a different provider."
        exit 1
    fi
}


run_qwen() {
    local full_prompt="$1"
    if command -v qwen &>/dev/null; then
        echo "${full_prompt}" | qwen --model "${PROVIDER_MODEL}" -p ""
    else
        log_error "qwen not found. Install via: brew install qwen-code"
        exit 1
    fi
}

run_crush() {
    local full_prompt="$1"
    if command -v crush &>/dev/null; then
        echo "${full_prompt}" | crush --model "${PROVIDER_MODEL}"
    else
        log_error "crush not found. Install via: brew install charmbracelet/tap/crush"
        exit 1
    fi
}


run_kiro_cli() {
    local full_prompt="$1"
    if command -v kiro-cli &>/dev/null; then
        echo "${full_prompt}" | kiro-cli chat
    else
        log_error "kiro-cli not found. Install via: https://kiro.dev"
        exit 1
    fi
}

run_goose() {
    local full_prompt="$1"
    if command -v goose &>/dev/null; then
        echo "${full_prompt}" | goose run --model "${PROVIDER_MODEL}"
    else
        log_error "goose not found. Install from: https://github.com/block/goose"
        exit 1
    fi
}

execute_provider() {
    local provider="$1"
    local full_prompt="$2"

    # Mock mode for testing (no API calls)
    if [[ "${EUXIS_MOCK_PROVIDER:-}" == "true" ]]; then
        echo "[MOCK] Provider: ${provider}, Model: ${PROVIDER_MODEL:-unknown}"
        echo "[MOCK] Prompt received (${#full_prompt} chars)"
        return 0
    fi

    # Resolve model and flags before execution
    resolve_provider_config "${provider}"

    case "${provider}" in
        claude)    run_claude "${full_prompt}" ;;
        gemini)    run_gemini "${full_prompt}" ;;
        openai)    run_openai "${full_prompt}" ;;
        ollama)    run_ollama "${full_prompt}" ;;
        qwen)      run_qwen "${full_prompt}" ;;
        crush)     run_crush "${full_prompt}" ;;
        kiro-cli)  run_kiro_cli "${full_prompt}" ;;
        goose)     run_goose "${full_prompt}" ;;
    esac
}
