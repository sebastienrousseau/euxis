#!/usr/bin/env bash
# (c) 2026 Euxis Fleet. All rights reserved.
# Euxis Library: Provider routing and execution
[[ -n "${_EUXIS_LIB_PROVIDERS:-}" ]] && return; _EUXIS_LIB_PROVIDERS=1

set -euo pipefail


EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"

source "${EUXIS_HOME}/core/lib/common.sh"

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
        # A-Tier: Research — massive context window for deep analysis + web search
        researcher|deep-researcher|compliance-officer)
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

# Validate model name against safe character allowlist.
# Allows: alphanumeric, dash, dot, slash, underscore, colon (covers all
# known model naming conventions: org/model, model:tag, model-version).
_validate_model_name() {
    local model="$1"
    if [[ ! "${model}" =~ ^[a-zA-Z0-9._/:@-]+$ ]]; then
        log_error "Invalid model name: '${model}' — contains disallowed characters"
        exit 1
    fi
    if [[ "${#model}" -gt 128 ]]; then
        log_error "Invalid model name: '${model}' — exceeds 128 character limit"
        exit 1
    fi
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
        gemini)   # The Researcher (Massive Context, built-in search grounding)
            PROVIDER_MODEL="gemini-2.0-flash"
            PROVIDER_FLAGS=""
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
            ;;
        *)
            log_error "Unknown provider: '${provider}'"
            exit 1
            ;;
    esac

    # Validate model name (especially important for env-var-sourced values)
    _validate_model_name "${PROVIDER_MODEL}"
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
        log_warn "timeout command not available - running without timeout protection"
        "$@"
    fi
}

run_claude() {
    local full_prompt="$1"

    # Check for optional dependencies and adjust tools accordingly
    local allowed_tools="Read,Edit,Write,Bash(grep:*) Bash(find:*) Bash(python3:*) Bash(pytest:*) Bash(cat:*) Bash(ls:*) Bash(test:*) Bash(head:*) Bash(tail:*) Bash(wc:*) Bash(mkdir:*) Bash(touch:*) Bash(pip:*) Bash(uv:*) Bash(curl:*)"

    # Add jq support if available, otherwise warn
    if command -v jq &>/dev/null; then
        allowed_tools="${allowed_tools} Bash(jq:*)"
    else
        log_warn "jq not available - JSON processing may be limited"
    fi

    # Add gh support if available, otherwise warn
    if command -v gh &>/dev/null; then
        allowed_tools="${allowed_tools} Bash(gh:*)"
    else
        log_warn "gh CLI not available - GitHub operations may be limited"
    fi

    # Use timeout wrapper for the claude command
    # Tools include WebSearch and WebFetch for external research capability
    printf '%s\n' "${full_prompt}" | run_with_timeout "${EUXIS_API_TIMEOUT}" "claude API" \
        claude \
        --print \
        --model "${PROVIDER_MODEL}" \
        --tools "Read,Edit,Write,Bash,WebSearch,WebFetch" \
        --allowedTools "${allowed_tools}" \
        --dangerously-skip-permissions \
        --max-turns "${EUXIS_MAX_TURNS:-25}"
}

run_gemini() {
    local full_prompt="$1"
    if command -v gemini &>/dev/null; then
        local gemini_home gemini_config
        gemini_home="${EUXIS_GEMINI_HOME:-/tmp/euxis-gemini-home}"
        gemini_config="${gemini_home}/.gemini"

        if [[ ! -d "${gemini_config}" ]]; then
            umask 077
            mkdir -p "${gemini_config}/tmp" "${gemini_config}/policies"
            if [[ -d "${HOME}/.gemini" ]]; then
                cp -a "${HOME}/.gemini/oauth_creds.json" "${gemini_config}/" 2>/dev/null || true
                cp -a "${HOME}/.gemini/google_accounts.json" "${gemini_config}/" 2>/dev/null || true
                cp -a "${HOME}/.gemini/settings.json" "${gemini_config}/" 2>/dev/null || true
                cp -a "${HOME}/.gemini/state.json" "${gemini_config}/" 2>/dev/null || true
                cp -a "${HOME}/.gemini/installation_id" "${gemini_config}/" 2>/dev/null || true
                cp -a "${HOME}/.gemini/trustedFolders.json" "${gemini_config}/" 2>/dev/null || true
                if [[ -d "${HOME}/.gemini/policies" ]]; then
                    cp -a "${HOME}/.gemini/policies/." "${gemini_config}/policies/" 2>/dev/null || true
                fi
            fi
        fi

        local guard
        guard="IMPORTANT: This is plain-text mode. Do not call tools or output ACTION/OBSERVATION steps. Respond only with the final answer in the required Output Format."
        local err_file old_umask
        old_umask=$(umask)
        umask 077
        err_file=$(mktemp "${TMPDIR:-/tmp}/euxis_gemini_err.XXXXXX" 2>/dev/null) || { umask "${old_umask}"; echo "[euxis] Error: failed to create temp file" >&2; return 1; }
        umask "${old_umask}"
        local out prompt
        prompt=$(printf '%s\n\n%s' "${guard}" "${full_prompt}")
        # shellcheck disable=SC2086
        if command -v mise &>/dev/null; then
            out=$(HOME="${gemini_home}" NODE_OPTIONS="--no-deprecation" mise exec -- gemini ${PROVIDER_FLAGS} --output-format text -p "${prompt}" 2> "${err_file}")
        else
            out=$(HOME="${gemini_home}" NODE_OPTIONS="--no-deprecation" gemini ${PROVIDER_FLAGS} --output-format text -p "${prompt}" 2> "${err_file}")
        fi
        local rc=$?
        if [[ -s "${err_file}" ]]; then
            grep -vE '^(Error executing tool|Tool execution denied by policy|Tool ".*" not found in registry|Loaded cached credentials|Hook registry initialized|\\(node:.*\\) \\[DEP0040\\])' "${err_file}" >&2 || true
        fi
        rm -f "${err_file}"
        printf '%s' "${out}" | grep -vE '^(Error executing tool|Tool execution denied by policy|Tool ".*" not found in registry|Loaded cached credentials|Hook registry initialized)$' || true
        return "${rc}"
    else
        log_error "gemini CLI not found — install via: npm i -g @anthropic-ai/gemini-cli"
        exit 1
    fi
}

run_openai() {
    local full_prompt="$1"
    if command -v codex &>/dev/null; then
        printf '%s\n' "${full_prompt}" | codex --model "${PROVIDER_MODEL}"
    else
        log_error "codex CLI not found — install via: npm i -g @openai/codex"
        exit 1
    fi
}

run_ollama() {
    local full_prompt="$1"
    if command -v ollama &>/dev/null; then
        printf '%s\n' "${full_prompt}" | ollama run "${PROVIDER_MODEL}"
    else
        log_error "ollama not found — install via: brew install ollama"
        exit 1
    fi
}


run_qwen() {
    local full_prompt="$1"
    if command -v qwen &>/dev/null; then
        printf '%s\n' "${full_prompt}" | qwen --model "${PROVIDER_MODEL}" -p ""
    else
        log_error "qwen not found — install via: brew install qwen-code"
        exit 1
    fi
}

run_crush() {
    local full_prompt="$1"
    if command -v crush &>/dev/null; then
        printf '%s\n' "${full_prompt}" | crush --model "${PROVIDER_MODEL}"
    else
        log_error "crush not found — install via: brew install charmbracelet/tap/crush"
        exit 1
    fi
}


run_kiro_cli() {
    local full_prompt="$1"
    if command -v kiro-cli &>/dev/null; then
        printf '%s\n' "${full_prompt}" | kiro-cli chat
    else
        log_error "kiro-cli not found — install via: npm i -g kiro-cli"
        exit 1
    fi
}

run_goose() {
    local full_prompt="$1"
    if command -v goose &>/dev/null; then
        printf '%s\n' "${full_prompt}" | goose run --model "${PROVIDER_MODEL}"
    else
        log_error "goose not found — install via: brew install block/tap/goose"
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
        *)
            log_error "Unknown provider: '${provider}'"
            exit 1
            ;;
    esac
}
