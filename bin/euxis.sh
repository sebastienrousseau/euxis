#!/usr/bin/env bash
#
# Euxis - Multi-Provider AI Agent Framework
# Usage: euxis <agent> <task> [provider]
#        euxis delegate <agent> <task> [provider]
#
# Agents are discovered dynamically from ~/.euxis/prompts/{core,fleet}/*.txt
# Providers: claude (default), gemini, openai, ollama, opencode
#

set -euo pipefail

# ============================================================================
# Configuration
# ============================================================================

EUXIS_HOME="${HOME}/.euxis"
PROMPTS_DIR="${EUXIS_HOME}/prompts"
PROJECTS_DIR="${EUXIS_HOME}/projects"

# Default provider (fallback when no tiering match)
DEFAULT_PROVIDER="claude"

# ============================================================================
# Intelligence Tiering (v5.0)
# Maps agent → optimal provider based on task complexity profile.
# Explicit provider argument always overrides tiering.
# P0 tasks always use Strategic tier (claude) regardless of agent.
# ============================================================================
#   Tier 1: Strategic   — orchestrator, architect, product-manager, reviewer → claude
#   Tier 2: Research    — deep-researcher                                    → gemini
#   Tier 3: Coding      — bug-fixer, legacy-maintainer                      → opencode
#   Tier 4: Utility     — butler, librarian                                 → ollama
#   Default: Standard   — all others                                         → claude

resolve_tiered_provider() {
    local agent="$1"
    local priority="${2:-}"
    # P0 tasks ALWAYS use Strategic tier (claude) regardless of agent
    if [[ "$priority" == "P0" ]]; then
        echo "claude"
        return
    fi
    case "${agent}" in
        # Tier 1: Strategic — best-in-class reasoning and tool use
        orchestrator|architect|product-manager|reviewer)
            echo "claude" ;;
        # Tier 2: Research — massive context window for deep analysis
        deep-researcher)
            echo "gemini" ;;
        # Tier 3: Coding — local code models are fast for diffs and fixes
        bug-fixer|legacy-maintainer)
            echo "opencode" ;;
        # Tier 4: Utility — zero latency, no cost for simple summaries
        butler|librarian)
            echo "ollama" ;;
        # Default (Standard): fall back to primary provider
        *)
            echo "${DEFAULT_PROVIDER}" ;;
    esac
}

# ============================================================================
# Fast Boot Health Check (hash-based, silent on pass)
# ============================================================================

check_health_fast() {
    local hash_file="/tmp/euxis_health_hash"
    local current_hash
    current_hash=$(ls -lR "${PROMPTS_DIR}" 2>/dev/null | md5sum 2>/dev/null | awk '{print $1}')

    # If hash matches last boot, skip the deep audit
    if [[ -f "${hash_file}" ]] && [[ "$(cat "${hash_file}" 2>/dev/null)" == "${current_hash}" ]]; then
        return 0
    fi

    # Brain changed or first boot: run the health check silently
    local health_script="${EUXIS_HOME}/bin/euxis-health"
    if [[ -x "${health_script}" ]]; then
        if ! "${health_script}" --silent; then
            echo "[euxis] WARNING: Fleet integrity check failed. Run 'euxis-health' for details." >&2
        fi
    fi

    echo "${current_hash}" > "${hash_file}"
}

check_health_fast

# ============================================================================
# Utility Functions
# ============================================================================

resolve_agent_path() {
    local agent="$1"
    for dir in "${PROMPTS_DIR}/core" "${PROMPTS_DIR}/fleet"; do
        if [[ -f "${dir}/${agent}.txt" ]]; then
            echo "${dir}/${agent}.txt"
            return 0
        fi
    done
    echo ""
    return 1
}

list_agents() {
    local name
    for dir in "${PROMPTS_DIR}/core" "${PROMPTS_DIR}/fleet"; do
        for f in "${dir}"/*.txt; do
            [[ -f "${f}" ]] || continue
            name=$(basename "${f}" .txt)
            [[ "${name}" == _* ]] && continue
            echo "    ${name}"
        done
    done
}

usage() {
    cat <<EOF
Euxis - Multi-Provider AI Agent Framework

Usage: euxis <agent> <task> [provider]
       euxis delegate <agent> <task> [provider]

Arguments:
    agent       Agent to invoke (see list below)
    task        Task description or file path
    provider    AI provider: claude, gemini, openai, ollama, opencode
                (auto-selected per agent via intelligence tiering if omitted)

Subcommands:
    delegate    Invoke a sub-agent (used by orchestrator for chaining)

Available Agents:
$(list_agents)

Examples:
    euxis architect "Review the authentication module"
    euxis bug-fixer "Fix the null pointer exception in user.py" gemini
    euxis orchestrator "Plan the migration to microservices"

Environment:
    EUXIS_PROJECT        Project name (default: derived from current directory)
    EUXIS_SESSION_ID     Session identifier (default: timestamp)
    EUXIS_OLLAMA_MODEL   Ollama model name (default: llama3.2)
    EUXIS_OPENCODE_MODEL OpenCode model name (default: codellama)

EOF
    exit 1
}

log_info() {
    echo "[euxis] $*" >&2
}

log_error() {
    echo "[euxis] ERROR: $*" >&2
}

# Spinner for LLM wait time (runs in background)
_spinner_pid=""

start_spinner() {
    local msg="${1:-Thinking...}"
    (
        local chars='⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏'
        local i=0
        while true; do
            printf '\r[euxis] %s %s' "${chars:i%10:1}" "${msg}" >&2
            i=$((i + 1))
            sleep 0.1
        done
    ) &
    _spinner_pid=$!
}

stop_spinner() {
    if [[ -n "${_spinner_pid}" ]]; then
        kill "${_spinner_pid}" 2>/dev/null
        wait "${_spinner_pid}" 2>/dev/null || true
        printf '\r\033[K' >&2
        _spinner_pid=""
    fi
}

# ============================================================================
# Context Functions
# ============================================================================

get_project_name() {
    if [[ -n "${EUXIS_PROJECT:-}" ]]; then
        echo "${EUXIS_PROJECT}"
    else
        basename "$(pwd)"
    fi
}

get_session_id() {
    if [[ -n "${EUXIS_SESSION_ID:-}" ]]; then
        echo "${EUXIS_SESSION_ID}"
    else
        date +"%Y%m%d-%H%M%S"
    fi
}

ensure_project_dirs() {
    local project="$1"
    local agent="$2"
    local agent_dir="${PROJECTS_DIR}/${project}/${agent}"

    mkdir -p "${agent_dir}/output"

    # Initialize files if they don't exist
    [[ -f "${agent_dir}/audit.md" ]] || echo "# Audit Log: ${agent}" > "${agent_dir}/audit.md"
    [[ -f "${agent_dir}/memory.md" ]] || echo "# Memory: ${agent}" > "${agent_dir}/memory.md"
}

get_memory_context() {
    local memory_file="$1"
    local lines="${2:-50}"

    if [[ -f "${memory_file}" ]]; then
        tail -n "${lines}" "${memory_file}" 2>/dev/null || true
    fi
}

# ============================================================================
# Tiered Memory Retrieval (MemGPT-inspired)
# ============================================================================

# Tier 1: Hot memory — most recent entries (always included)
get_hot_memory() {
    local memory_file="$1"
    if [[ -f "${memory_file}" ]]; then
        tail -n 20 "${memory_file}" 2>/dev/null || true
    fi
}

# Tier 2: Relevant memory — keyword search against full memory
get_relevant_memory() {
    local memory_file="$1"
    local task="$2"

    if [[ ! -f "${memory_file}" ]]; then
        return
    fi

    # Extract keywords from task (words > 4 chars, deduplicated, max 10)
    local keywords
    keywords=$(echo "${task}" | tr '[:upper:]' '[:lower:]' | \
        grep -oE '[a-z]{5,}' | sort -u | head -n 10)

    if [[ -z "${keywords}" ]]; then
        return
    fi

    # Build grep alternation pattern from keywords
    local pattern
    pattern=$(printf '%s\n' "${keywords}" | tr '\n' '|' | sed 's/|$//')

    # Search full memory, deduplicate, take top 10 matches
    grep -iE "${pattern}" "${memory_file}" 2>/dev/null | \
        sort -u | head -n 10 || true
}

# Tier 3: Cross-agent memory — search sibling agents for relevant context
get_cross_agent_memory() {
    local project_dir="$1"
    local current_agent="$2"
    local task="$3"

    if [[ ! -d "${project_dir}" ]]; then
        return
    fi

    # Extract keywords (same logic as Tier 2)
    local keywords
    keywords=$(echo "${task}" | tr '[:upper:]' '[:lower:]' | \
        grep -oE '[a-z]{5,}' | sort -u | head -n 10)

    if [[ -z "${keywords}" ]]; then
        return
    fi

    local pattern
    pattern=$(printf '%s\n' "${keywords}" | tr '\n' '|' | sed 's/|$//')

    # Search all sibling agent memory files (not our own)
    local sibling_mem
    for sibling_mem in "${project_dir}"/*/memory.md; do
        [[ -f "${sibling_mem}" ]] || continue
        # Skip our own memory
        local sibling_agent
        sibling_agent=$(basename "$(dirname "${sibling_mem}")")
        [[ "${sibling_agent}" == "${current_agent}" ]] && continue

        local matches
        matches=$(grep -iE "${pattern}" "${sibling_mem}" 2>/dev/null | head -n 3 || true)
        if [[ -n "${matches}" ]]; then
            echo "  [${sibling_agent}]:"
            echo "${matches}" | sed 's/^/    /'
        fi
    done
}

build_tiered_memory() {
    local memory_file="$1"
    local task="$2"
    local project_dir="$3"
    local agent="$4"

    local hot relevant cross

    hot=$(get_hot_memory "${memory_file}")
    relevant=$(get_relevant_memory "${memory_file}" "${task}")
    cross=$(get_cross_agent_memory "${project_dir}" "${agent}" "${task}")

    cat <<MEMEOF
### Tier 1: Hot Memory (Recent)
${hot:-<empty>}

### Tier 2: Relevant Memory (Keyword Match)
${relevant:-<no matches>}

### Tier 3: Cross-Agent Context (Read-Only)
${cross:-<no sibling matches>}
MEMEOF
}

# ============================================================================
# Prompt Assembly
# ============================================================================

prepare_prompt() {
    local agent="$1"
    local task="$2"
    local audit_path="$3"
    local memory_path="$4"
    local session_id="$5"
    local model_name="$6"

    local prompt_file
    prompt_file=$(resolve_agent_path "${agent}")
    local protocol_file="${PROMPTS_DIR}/protocols/_protocol.txt"
    local common_file="${PROMPTS_DIR}/protocols/_common.txt"

    if [[ ! -f "${prompt_file}" ]]; then
        log_error "Agent prompt not found: ${prompt_file}"
        exit 1
    fi

    # Load prompt: common (stable prefix for cache), then agent, then protocol
    # Ordering rationale: _common.txt is identical across all agents, so placing
    # it first maximizes prompt-cache hit rates on providers that support prefix
    # caching (e.g. Claude, GPT-4). The agent-specific prompt follows, then the
    # protocol which is also shared but more volatile across versions.
    local prompt=""
    if [[ -f "${common_file}" ]]; then
        prompt=$(cat "${common_file}")$'\n\n'
    fi
    prompt="${prompt}$(cat "${prompt_file}")"
    if [[ -f "${protocol_file}" ]]; then
        prompt="${prompt}"$'\n\n'"$(cat "${protocol_file}")"
    fi

    # Replace variables
    prompt="${prompt//\{\{AUDIT_FILE_PATH\}\}/${audit_path}}"
    prompt="${prompt//\{\{MEMORY_FILE_PATH\}\}/${memory_path}}"
    prompt="${prompt//\{\{SESSION_ID\}\}/${session_id}}"
    prompt="${prompt//\{\{MODEL_NAME\}\}/${model_name}}"

    # Build tiered memory context
    local project_dir
    project_dir=$(dirname "$(dirname "${memory_path}")")  # up from agent/ to project/
    # If project_dir resolution fails, fall back to flat memory
    if [[ ! -d "${project_dir}" ]]; then
        project_dir=""
    fi

    local memory_context
    memory_context=$(build_tiered_memory "${memory_path}" "${task}" "${project_dir}" "${agent}")

    # Build fleet roster for manifest-producing agents
    local fleet_roster=""
    local registry_file="${EUXIS_HOME}/registry.json"
    if [[ -f "${registry_file}" ]] && command -v jq &>/dev/null; then
        fleet_roster=$(jq -r '.agents[].id' "${registry_file}" | tr '\n' ', ' | sed 's/,$//')
    fi

    # Build final prompt
    cat <<EOF
${prompt}

---
## MEMORY CONTEXT (Tiered Retrieval)
${memory_context}

---
## FLEET ROSTER (Valid Agent IDs)
Use ONLY agents from this list in mission manifests: ${fleet_roster}

---
## CURRENT TASK
${task}
EOF
}

# ============================================================================
# Provider Routing (v5.0 — specialized model + flags per provider)
# ============================================================================

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
        opencode) # The Local Specialist (Coding)
            PROVIDER_MODEL="${EUXIS_OPENCODE_MODEL:-codellama}"
            PROVIDER_FLAGS="--local"
            ;;
    esac
}

run_claude() {
    local full_prompt="$1"
    # Use agentic mode with file tools so agents can make actual code changes.
    # --dangerously-skip-permissions avoids interactive prompts in background loops.
    # --max-turns caps agent reasoning to prevent runaway execution.
    echo "${full_prompt}" | claude \
        --print \
        --model "${PROVIDER_MODEL}" \
        --tools "Read,Edit,Write,Bash" \
        --allowedTools "Read,Edit,Write,Bash(grep:*) Bash(find:*) Bash(python3:*) Bash(pytest:*) Bash(cat:*) Bash(ls:*) Bash(test:*) Bash(head:*) Bash(tail:*) Bash(wc:*) Bash(mkdir:*) Bash(touch:*) Bash(pip:*) Bash(uv:*)" \
        --dangerously-skip-permissions \
        --max-turns 25
}

run_gemini() {
    local full_prompt="$1"
    if command -v gemini &>/dev/null; then
        # Gemini CLI may attempt tool calls; explicitly disable via prompt guard.
        local guard
        guard="IMPORTANT: This is plain-text mode. Do not call tools or output ACTION/OBSERVATION steps. Respond only with the final answer in the required Output Format."
        local err_file
        err_file=$(mktemp 2>/dev/null || echo "/tmp/euxis_gemini_err.$$") 
        local out
        out=$(printf '%s\n\n%s' "${guard}" "${full_prompt}" | NODE_OPTIONS="--no-deprecation" \
            gemini --output-format text -p "" 2> "${err_file}")
        local rc=$?
        # Filter known noisy Gemini CLI stderr lines while preserving real errors.
        if [[ -s "${err_file}" ]]; then
            grep -vE '^(Error executing tool|Tool execution denied by policy|Tool ".*" not found in registry|Loaded cached credentials|Hook registry initialized|\\(node:.*\\) \\[DEP0040\\])' "${err_file}" >&2 || true
        fi
        rm -f "${err_file}"
        # Filter known noisy lines that sometimes appear on stdout.
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

run_opencode() {
    local full_prompt="$1"
    if command -v opencode &>/dev/null; then
        echo "${full_prompt}" | opencode --model "${PROVIDER_MODEL}"
    else
        log_error "opencode not found. Install from https://github.com/opencode-ai/opencode or use a different provider."
        exit 1
    fi
}

execute_provider() {
    local provider="$1"
    local full_prompt="$2"

    # Resolve model and flags before execution
    resolve_provider_config "${provider}"

    case "${provider}" in
        claude)    run_claude "${full_prompt}" ;;
        gemini)    run_gemini "${full_prompt}" ;;
        openai)    run_openai "${full_prompt}" ;;
        ollama)    run_ollama "${full_prompt}" ;;
        opencode)  run_opencode "${full_prompt}" ;;
    esac
}

# ============================================================================
# Argument Parsing & Validation
# ============================================================================

parse_args() {
    if [[ $# -lt 2 ]]; then
        usage
    fi

    AGENT="$1"
    TASK="$2"
    # If provider is explicitly given, use it; otherwise, auto-tier by agent
    PROVIDER="${3:-}"

    # Sanitize agent name: reject characters outside [a-zA-Z0-9_-]
    if [[ "${AGENT}" =~ [^a-zA-Z0-9_-] ]]; then
        log_error "Invalid agent name: ${AGENT} (only alphanumeric, hyphens, and underscores allowed)"
        exit 1
    fi

    # Validate agent (dynamic discovery from prompts subdirectories)
    local prompt_file
    prompt_file=$(resolve_agent_path "${AGENT}") || true
    if [[ -z "${prompt_file}" || "${AGENT}" == _* ]]; then
        log_error "Unknown agent: ${AGENT}"
        echo "Available agents:" >&2
        list_agents >&2
        exit 1
    fi

    # Resolve provider: explicit arg > intelligence tiering > default
    if [[ -z "${PROVIDER}" ]]; then
        PROVIDER=$(resolve_tiered_provider "${AGENT}")
    fi

    # Validate provider
    case "${PROVIDER}" in
        claude|gemini|openai|ollama|opencode) ;;
        *)
            log_error "Unknown provider: ${PROVIDER}"
            echo "Valid providers: claude, gemini, openai, ollama, opencode" >&2
            exit 1
            ;;
    esac
}

# ============================================================================
# Session Setup
# ============================================================================

setup_session() {
    PROJECT=$(get_project_name)
    SESSION_ID=$(get_session_id)

    ensure_project_dirs "${PROJECT}" "${AGENT}"

    AUDIT_PATH="${PROJECTS_DIR}/${PROJECT}/${AGENT}/audit.md"
    MEMORY_PATH="${PROJECTS_DIR}/${PROJECT}/${AGENT}/memory.md"
    OUTPUT_PATH="${PROJECTS_DIR}/${PROJECT}/${AGENT}/output/${SESSION_ID}.md"

    # Resolve model name from provider config
    resolve_provider_config "${PROVIDER}"
    MODEL_NAME="${PROVIDER_MODEL}"
}

# ============================================================================
# Output Capture
# ============================================================================

capture_output() {
    local agent="$1"
    local project="$2"
    local provider="$3"
    local task="$4"
    local session_id="$5"
    local output_path="$6"
    local output="$7"

    cat > "${output_path}" <<OUTEOF
# Output: ${agent} — ${session_id}
**Project:** ${project}
**Provider:** ${provider}
**Task:** ${task}

---

${output}
OUTEOF
}

# ============================================================================
# Main
# ============================================================================

main() {
    # Phase 1: Parse and validate arguments
    parse_args "$@"

    # Phase 2: Setup session paths and directories
    setup_session

    # Phase 3: Log session metadata
    log_info "Agent: ${AGENT}"
    log_info "Project: ${PROJECT}"
    log_info "Provider: ${PROVIDER}"
    log_info "Session: ${SESSION_ID}"
    log_info "Output: ${OUTPUT_PATH}"

    # Phase 4: Assemble prompt with context
    local full_prompt
    full_prompt=$(prepare_prompt "${AGENT}" "${TASK}" "${AUDIT_PATH}" "${MEMORY_PATH}" "${SESSION_ID}" "${MODEL_NAME}")

    # Phase 5: Execute with selected provider (with spinner feedback)
    start_spinner "${AGENT} (${PROVIDER})..."
    local output
    output=$(execute_provider "${PROVIDER}" "${full_prompt}")
    stop_spinner

    # Phase 6: Capture output to session file
    capture_output "${AGENT}" "${PROJECT}" "${PROVIDER}" "${TASK}" "${SESSION_ID}" "${OUTPUT_PATH}" "${output}"

    # Phase 7: Print output to stdout
    # When stdout is piped or redirected, extract JSON for dispatch consumption.
    # Tries multiple extraction strategies in order of reliability.
    if [[ ! -t 1 ]]; then
        local extracted=""

        # Strategy 1: Last ```json fenced block (preferred, most reliable)
        if echo "${output}" | grep -q '```json'; then
            extracted=$(echo "${output}" | awk '/^```json$/{found=""; capture=1; next} /^```$/ && capture{capture=0} capture{found=found (found?"\n":"") $0} END{print found}')
        fi

        # Strategy 2: JSON inside <!-- EUXIS_HANDOFF ... --> with "dispatches" key
        if [[ -z "$extracted" ]] && echo "${output}" | grep -q 'EUXIS_HANDOFF'; then
            local handoff
            handoff=$(echo "${output}" | sed -n '/<!-- EUXIS_HANDOFF/,/-->/p' | sed '1d;$d')
            if echo "$handoff" | jq -e 'has("dispatches")' &>/dev/null; then
                extracted="$handoff"
            fi
        fi

        # Strategy 3: Any JSON object with "dispatches" array anywhere in output
        if [[ -z "$extracted" ]] && command -v python3 &>/dev/null; then
            extracted=$(python3 -c "
import json, re, sys
text = sys.stdin.read()
for m in re.finditer(r'\{[^{}]*\"dispatches\"[^{}]*\[.*?\].*?\}', text, re.DOTALL):
    try:
        obj = json.loads(m.group())
        if 'dispatches' in obj:
            print(json.dumps(obj, indent=2))
            sys.exit(0)
    except json.JSONDecodeError:
        pass
" <<< "${output}" 2>/dev/null || true)
        fi

        if [[ -n "$extracted" ]] && echo "$extracted" | jq empty 2>/dev/null; then
            echo "$extracted"
        else
            echo "${output}"
        fi
    else
        echo "${output}"
    fi
}

# ============================================================================
# Delegate: invoke a sub-agent from the orchestrator (or any agent)
# Usage: euxis delegate <agent> <task> [provider]
# ============================================================================

delegate() {
    if [[ $# -lt 2 ]]; then
        log_error "delegate requires: <agent> <task> [provider]"
        exit 1
    fi

    local sub_agent="$1"
    local sub_task="$2"
    local provider="${3:-$(resolve_tiered_provider "${sub_agent}")}"

    log_info "Delegating to ${sub_agent}..."

    # Re-invoke euxis for the sub-agent (inherits EUXIS_PROJECT)
    EUXIS_PROJECT="${EUXIS_PROJECT:-$(get_project_name)}" \
        "$0" "${sub_agent}" "${sub_task}" "${provider}"
}

# ============================================================================
# Entry Point
# ============================================================================

case "${1:-}" in
    delegate) shift; delegate "$@" ;;
    *)        main "$@" ;;
esac
