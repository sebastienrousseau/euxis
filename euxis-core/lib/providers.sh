#!/usr/bin/env bash
# Euxis Library: Provider routing and execution
[[ -n "${_EUXIS_LIB_PROVIDERS:-}" ]] && return; _EUXIS_LIB_PROVIDERS=1

set -euo pipefail


EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"

COMMON_SH="${EUXIS_HOME}/euxis-core/lib/common.sh"
if [[ -f "${COMMON_SH}" ]]; then
  # shellcheck source=core/lib/common.sh
  source "${COMMON_SH}"
else
  # shellcheck source=core/lib/common.sh
  source "${REPO_ROOT}/core/lib/common.sh"
fi

# Default provider (fallback when no tiering match)
DEFAULT_PROVIDER="${EUXIS_DEFAULT_PROVIDER:-claude}"

# Configurable timeouts for provider API calls (in seconds)
EUXIS_API_TIMEOUT="${EUXIS_API_TIMEOUT:-300}"  # 5 minutes default

# ============================================================================
# Provider Fallback Chain (v0.1.0)
# Automatic failover when primary provider is unavailable.
# Ensures mission-critical tasks (guard, verify) are never completely offline.
# ============================================================================

# Fallback chain: cloud -> cloud alternative -> local
PROVIDER_FALLBACK_CHAIN="${EUXIS_FALLBACK_CHAIN:-claude:gemini:ollama}"

# Max retries before switching providers
EUXIS_MAX_RETRIES="${EUXIS_MAX_RETRIES:-2}"

# Retry backoff (seconds)
EUXIS_RETRY_BACKOFF="${EUXIS_RETRY_BACKOFF:-2}"

# Get next fallback provider from chain
# Usage: _get_fallback_provider <current_provider>
_get_fallback_provider() {
    local current="$1"
    local chain="${PROVIDER_FALLBACK_CHAIN}"
    local found_current=0

    IFS=':' read -ra providers <<< "$chain"
    for p in "${providers[@]}"; do
        if [[ $found_current -eq 1 ]]; then
            # Return next provider in chain
            if command -v "$p" &>/dev/null || [[ "$p" == "ollama" ]]; then
                echo "$p"
                return 0
            fi
        fi
        if [[ "$p" == "$current" ]]; then
            found_current=1
        fi
    done

    # No fallback available
    return 1
}

# Check if provider is available
# Usage: _provider_available <provider>
_provider_available() {
    local provider="$1"

    case "$provider" in
        claude)
            command -v claude &>/dev/null
            ;;
        gemini)
            command -v gemini &>/dev/null
            ;;
        openai)
            [[ -n "${OPENAI_API_KEY:-}" ]] || command -v openai &>/dev/null
            ;;
        ollama)
            command -v ollama &>/dev/null && ollama list &>/dev/null
            ;;
        goose)
            command -v goose &>/dev/null
            ;;
        *)
            command -v "$provider" &>/dev/null
            ;;
    esac
}

# Execute with retry and fallback
# Usage: run_with_fallback <provider> <prompt> <agent>
# Returns: output on success, exits on failure
run_with_fallback() {
    local provider="$1"
    local prompt="$2"
    local agent="${3:-unknown}"
    local retry=0
    local current_provider="$provider"
    local output=""
    local exit_code=0

    # Source UI for status display if available
    if [[ -f "${EUXIS_HOME}/euxis-core/lib/ui.sh" ]]; then
        source "${EUXIS_HOME}/euxis-core/lib/ui.sh" 2>/dev/null || true
    fi

    while true; do
        # Check provider availability
        if ! _provider_available "$current_provider"; then
            _log_provider_switch "$agent" "$current_provider" "unavailable"
            current_provider=$(_get_fallback_provider "$current_provider") || {
                log_error "All providers exhausted. No fallback available."
                return 1
            }
            _log_provider_switch "$agent" "$current_provider" "switching"
            continue
        fi

        # Resolve provider config
        resolve_provider_config "$current_provider"

        # Execute based on provider
        case "$current_provider" in
            claude)  output=$(run_claude "$prompt" 2>&1); exit_code=$? ;;
            gemini)  output=$(run_gemini "$prompt" 2>&1); exit_code=$? ;;
            openai)  output=$(run_openai "$prompt" 2>&1); exit_code=$? ;;
            ollama)  output=$(run_ollama "$prompt" 2>&1); exit_code=$? ;;
            goose)   output=$(run_goose "$prompt" 2>&1); exit_code=$? ;;
            *)       output=$(run_generic "$current_provider" "$prompt" 2>&1); exit_code=$? ;;
        esac

        if [[ $exit_code -eq 0 ]]; then
            echo "$output"
            return 0
        fi

        # Check if error is retryable
        if _is_retryable_error "$output"; then
            retry=$((retry + 1))
            if [[ $retry -le ${EUXIS_MAX_RETRIES} ]]; then
                _log_retry "$agent" "$current_provider" "$retry"
                sleep "${EUXIS_RETRY_BACKOFF}"
                continue
            fi
        fi

        # Try fallback provider
        local fallback
        fallback=$(_get_fallback_provider "$current_provider") || {
            log_error "Provider $current_provider failed and no fallback available"
            echo "$output"
            return $exit_code
        }

        _log_provider_switch "$agent" "$fallback" "failover"
        current_provider="$fallback"
        retry=0
    done
}

# Check if error is retryable (timeout, rate limit, temporary)
_is_retryable_error() {
    local error="$1"

    # Retryable patterns
    [[ "$error" == *"timeout"* ]] && return 0
    [[ "$error" == *"rate limit"* ]] && return 0
    [[ "$error" == *"429"* ]] && return 0
    [[ "$error" == *"503"* ]] && return 0
    [[ "$error" == *"temporary"* ]] && return 0
    [[ "$error" == *"connection"* ]] && return 0

    return 1
}

# Log provider switch for UX
_log_provider_switch() {
    local agent="$1"
    local provider="$2"
    local reason="$3"

    local icon color msg
    case "$reason" in
        unavailable)
            icon="✖"; color="\033[38;5;203m"
            msg="[Provider unavailable]"
            ;;
        switching)
            icon="⚠"; color="\033[38;5;220m"
            msg="[Switching to $provider]"
            ;;
        failover)
            icon="⚠"; color="\033[38;5;220m"
            msg="[Failover to $provider]"
            ;;
    esac

    echo -e "${color}${icon}\033[0m ${agent}  ${msg}" >&2
}

# Log retry attempt
_log_retry() {
    local agent="$1"
    local provider="$2"
    local attempt="$3"

    echo -e "\033[38;5;220m⚠\033[0m ${agent}  [Retry $attempt with backoff...]" >&2
}

# ============================================================================
# Intelligence Tiering (v4.7)
# Maps agent -> optimal provider based on task complexity profile.
# Explicit provider argument always overrides tiering.
# P0 tasks always use Strategic tier (claude) regardless of agent.
# ============================================================================

resolve_tiered_provider() {
    local agent="$1"
    local priority="${2:-}"
    if [[ -n "${EUXIS_FORCE_PROVIDER:-}" ]]; then
        echo "${EUXIS_FORCE_PROVIDER}"
        return
    fi
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
            PROVIDER_MODEL="${EUXIS_OPENAI_MODEL:-gpt-4o}"
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
    # Default max turns: 50 for complex analysis tasks, configurable via EUXIS_MAX_TURNS
    # SECURITY: Removed --dangerously-skip-permissions flag (S3-002 fix)
    # Permission checks are enforced - use allowedTools for fine-grained control
    printf '%s\n' "${full_prompt}" | run_with_timeout "${EUXIS_API_TIMEOUT}" "claude API" \
        claude \
        --print \
        --model "${PROVIDER_MODEL}" \
        --tools "Read,Edit,Write,Bash,WebSearch,WebFetch" \
        --allowedTools "${allowed_tools}" \
        --max-turns "${EUXIS_MAX_TURNS:-50}"
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
        # SECURITY: Use private temp directory to prevent race conditions (S4-002 fix)
        local err_file secure_tmpdir
        secure_tmpdir="${XDG_RUNTIME_DIR:-${TMPDIR:-/tmp}}/euxis-$$"
        mkdir -p "$secure_tmpdir" 2>/dev/null
        chmod 700 "$secure_tmpdir" 2>/dev/null
        err_file="${secure_tmpdir}/gemini_err"
        touch "$err_file" 2>/dev/null && chmod 600 "$err_file" 2>/dev/null
        if [[ ! -w "$err_file" ]]; then
            echo "[euxis] Error: failed to create secure temp file" >&2
            return 1
        fi
        # Cleanup on exit
        trap "rm -rf '$secure_tmpdir' 2>/dev/null" EXIT
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
        if [[ -z "${PROVIDER_MODEL}" || "${PROVIDER_MODEL}" == "auto" ]]; then
            printf '%s\n' "${full_prompt}" | codex exec -
        else
            printf '%s\n' "${full_prompt}" | codex exec --model "${PROVIDER_MODEL}" -
        fi
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
