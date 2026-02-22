#!/usr/bin/env bash
#
# Euxis CLI Module - Command-line interface and user interaction
# Part of the Euxis Enterprise Unified eXecution Intelligence System
#
[[ -n "${_EUXIS_LIB_CLI:-}" ]] && return; _EUXIS_LIB_CLI=1

set -euo pipefail

# Source validation library for security checks
source "${EUXIS_HOME}/euxis-core/lib/validation.sh"

# ============================================================================
# Usage Information - Beautiful CLI Output
# ============================================================================

# Unicode support detection (WSL/rxvt fallback)
_supports_unicode() {
    [[ "${LANG:-}" == *UTF-8* ]] || [[ "${LC_ALL:-}" == *UTF-8* ]] || [[ "${TERM:-}" == *256color* ]]
}

# Icon definitions with ASCII fallback
_setup_icons() {
    if _supports_unicode; then
        ICON_CHECK="✓"
        ICON_BULLET="▸"
        ICON_ARROW="→"
        ICON_BOLT="⚡"
    else
        ICON_CHECK="*"
        ICON_BULLET=">"
        ICON_ARROW="->"
        ICON_BOLT="*"
    fi
}

# Color definitions (respects NO_COLOR)
_setup_colors() {
    _setup_icons
    if [[ -n "${NO_COLOR:-}" ]] || [[ ! -t 1 ]]; then
        RESET="" BOLD="" DIM=""
        RED="" GREEN="" YELLOW="" BLUE="" MAGENTA="" CYAN="" WHITE=""
        BG_BLUE="" BG_GREEN="" BG_MAGENTA=""
    else
        RESET='\033[0m'
        BOLD='\033[1m'
        DIM='\033[2m'
        RED='\033[38;5;203m'
        GREEN='\033[38;5;42m'
        YELLOW='\033[38;5;220m'
        BLUE='\033[38;5;69m'
        MAGENTA='\033[38;5;211m'
        CYAN='\033[38;5;87m'
        WHITE='\033[38;5;255m'
        BG_BLUE='\033[48;5;24m'
        BG_GREEN='\033[48;5;22m'
        BG_MAGENTA='\033[48;5;53m'
    fi
}

_print_header() {
    # ASCII logo is shown by ui_header, just add tagline
    echo -e "  ${DIM}Enterprise Unified eXecution Intelligence System${RESET}"
    echo -e "  ${DIM}Multi-Provider Agent Framework • 53 Agents${RESET}"
    echo -e ""
}

_print_section() {
    local title="$1"
    echo -e "  ${BOLD}${CYAN}${ICON_BULLET} ${title}${RESET}"
}

_print_cmd() {
    local cmd="$1"
    local desc="$2"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "$cmd" "$desc"
}

_print_agent_grid() {
    local agents
    agents=$(list_agents 2>/dev/null | tr '\n' ' ') || true

    local count=0
    local line=""
    for agent in $agents; do
        [[ "$agent" == _* ]] && continue
        line+="${GREEN}✓${RESET} ${CYAN}${agent}${RESET}  "
        count=$((count + 1))
        if (( count % 6 == 0 )); then
            echo -e "    ${line}"
            line=""
        fi
    done
    [[ -n "$line" ]] && echo -e "    ${line}"
    true  # Ensure success exit
}

usage() {
    _setup_colors

    _print_header

    echo -e "  ${BOLD}${WHITE}USAGE${RESET}"
    echo -e "    ${CYAN}\$${RESET} euxis ${DIM}<command> [options]${RESET}"
    echo -e "    ${CYAN}\$${RESET} euxis ${DIM}<agent> <task> [--provider <name>]${RESET}"
    echo -e ""

    _print_section "CORE COMMANDS"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "agent <cmd>" "Manage agent plugins (register, list)"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "squad <cmd>" "Manage agent squads (list, deploy, info)"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "combo <cmd>" "Chain agents sequentially (list, run)"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "playbook <cmd>" "Phased execution with gates (list, run)"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "dispatch <manifest>" "Deploy fleet from architect manifest"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "cortex <cmd>" "Vector memory interface (ChromaDB)"
    echo -e ""

    _print_section "QUALITY & INFRA"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "verify" "Run pre-commit quality gates"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "health" "Check system probes and connectivity"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "certify" "Full certification pipeline"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "deploy" "Spin up via Docker Compose"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "doctor" "Verify installation and environment"
    echo -e ""

    _print_section "AGENT ECOSYSTEM (50 Total)"
    printf "    ${DIM}%-7s${RESET} %s\n" "Core:" "architect, orchestrator, planner, reviewer, guard, auditor..."
    printf "    ${DIM}%-7s${RESET} %s\n" "Fleet:" "debugger, tester, pentester, researcher, designer, writer..."
    echo -e ""
    echo -e "    Run ${CYAN}euxis agents${RESET} to see the full list with descriptions."
    echo -e "    Run ${CYAN}euxis search <keyword>${RESET} to find agents by capability."
    echo -e ""

    _print_section "EXAMPLES"
    echo -e "    ${CYAN}\$${RESET} euxis architect ${DIM}\"Design a REST API for user auth\"${RESET}"
    echo -e "    ${CYAN}\$${RESET} euxis squad deploy quality ${DIM}\"Audit the payment module\"${RESET}"
    echo -e "       ${DIM}${ICON_ARROW} Deploys: reviewer, inspector, sentinel, pentester, auditor${RESET}"
    echo -e "    ${CYAN}\$${RESET} euxis combo run envision ${DIM}\"Build a CLI for data pipelines\"${RESET}"
    echo -e "       ${DIM}${ICON_ARROW} Chain: deep-researcher ${ICON_ARROW} planner ${ICON_ARROW} architect ${ICON_ARROW} reviewer${RESET}"
    echo -e "    ${CYAN}\$${RESET} euxis playbook run python ${DIM}\"Refactor the utils module\"${RESET}"
    echo -e "       ${DIM}${ICON_ARROW} Phases: analyze ${ICON_ARROW} implement ${ICON_ARROW} test ${ICON_ARROW} review${RESET}"
    echo -e ""

    _print_section "GLOBAL FLAGS"
    printf "    ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "--provider <name>" "Override AI provider (claude, gemini, openai, ollama...)"
    printf "    ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "--json" "Output as JSON (for piping to jq)"
    printf "    ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "--verbose" "Enable verbose logging"
    printf "    ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "--no-color" "Disable colored output"
    echo -e ""

    echo -e "  ${DIM}────────────────────────────────────────────────────────${RESET}"
    echo -e "  ${DIM}Docs:${RESET} ${CYAN}https://docs.euxis.co${RESET}  ${DIM}Issues:${RESET} ${CYAN}github.com/euxis/euxis${RESET}"
    echo -e ""

    exit 0
}

# Detailed agent listing (euxis agents)
usage_agents() {
    _setup_colors

    _print_header

    echo -e "  ${BOLD}${WHITE}AGENT ECOSYSTEM${RESET} ${DIM}— 50 Specialists${RESET}"
    echo -e ""

    _print_section "CORE AGENTS (12)"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "arbiter" "Resolves conflicts between agents"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "architect" "Designs systems and creates manifests"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "auditor" "Reviews code quality and compliance"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "critic" "Provides constructive feedback"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "guard" "Security gatekeeper and validator"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "historian" "Maintains project history and context"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "librarian" "Manages documentation and knowledge"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "orchestrator" "Coordinates multi-agent workflows"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "pair" "Collaborative coding partner"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "planner" "Creates project plans and roadmaps"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "reviewer" "Code review and approval"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "route" "Intelligent task routing"
    echo -e ""

    _print_section "FLEET AGENTS (38)"
    echo -e ""
    echo -e "    ${BOLD}Development${RESET}"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "debugger" "Finds and fixes bugs"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "tester" "Writes and runs tests"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "repairer" "Fixes broken code and configs"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "maintainer" "Handles upgrades and refactoring"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "optimizer" "Improves performance"
    echo -e ""
    echo -e "    ${BOLD}Research${RESET}"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "researcher" "Gathers information and context"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "deep-researcher" "Deep-dive analysis"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "investigator" "Root cause analysis"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "inspector" "Code inspection and review"
    echo -e ""
    echo -e "    ${BOLD}Security${RESET}"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "pentester" "Penetration testing"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "sentinel" "Threat detection"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "watchdog" "Continuous monitoring"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "gatekeeper" "Access control"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "cryptographer" "Cryptography specialist"
    echo -e ""
    echo -e "    ${BOLD}Creative${RESET}"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "designer" "UI/UX design"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "writer" "Technical writing"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "animator" "Motion and interaction"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "marketer" "Marketing content"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "evangelist" "Developer advocacy"
    echo -e ""
    echo -e "    ${BOLD}Operations${RESET}"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "automaton" "Task automation"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "butler" "Service orchestration"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "custodian" "Resource management"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "heal" "Self-healing operations"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "govern" "Policy enforcement"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "trace" "Distributed tracing"
    echo -e ""
    echo -e "    ${BOLD}Integration${RESET}"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "bridge" "System integration"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "conduit" "Data pipelines"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "ambassador" "External communications"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "interactor" "User interaction"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "responder" "Event handling"
    echo -e ""
    echo -e "    ${BOLD}Analytics${RESET}"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "strategist" "Strategic planning"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "tactician" "Tactical execution"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "accountant" "Cost analysis"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "ledger" "Audit trails"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "telemetrist" "Metrics collection"
    echo -e ""
    echo -e "    ${BOLD}Language${RESET}"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "polyglot" "Multi-language support"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "localizer" "Internationalization"
    printf "    ${CYAN}%-14s${RESET} ${DIM}%s${RESET}\n" "distill" "Content summarization"
    echo -e ""

    _print_section "PROVIDERS"
    echo -e "    ${GREEN}${ICON_CHECK}${RESET} claude    ${GREEN}${ICON_CHECK}${RESET} gemini    ${GREEN}${ICON_CHECK}${RESET} openai    ${GREEN}${ICON_CHECK}${RESET} ollama"
    echo -e "    ${GREEN}${ICON_CHECK}${RESET} qwen      ${GREEN}${ICON_CHECK}${RESET} crush     ${GREEN}${ICON_CHECK}${RESET} kiro-cli  ${GREEN}${ICON_CHECK}${RESET} goose"
    echo -e ""

    exit 0
}

# Search agents by capability
usage_search() {
    local query="${1:-}"
    _setup_colors

    if [[ -z "$query" ]]; then
        echo -e "${RED}Usage:${RESET} euxis search <keyword>"
        echo -e "${DIM}Example: euxis search security${RESET}"
        exit 1
    fi

    echo -e "${BOLD}Searching agents for:${RESET} ${CYAN}${query}${RESET}"
    echo -e ""

    # Simple keyword matching against agent categories
    local -A agent_tags=(
        ["security"]="guard auditor pentester sentinel watchdog gatekeeper cryptographer"
        ["testing"]="tester debugger inspector investigator"
        ["design"]="architect designer animator planner"
        ["writing"]="writer librarian evangelist marketer localizer"
        ["research"]="researcher deep-researcher investigator historian"
        ["operations"]="automaton butler custodian heal govern trace orchestrator"
        ["integration"]="bridge conduit ambassador interactor responder"
        ["analytics"]="strategist tactician accountant ledger telemetrist"
        ["code"]="debugger tester repairer maintainer optimizer pair reviewer"
        ["performance"]="optimizer bench telemetrist"
    )

    local found=0
    local query_lower="${query,,}"

    for tag in "${!agent_tags[@]}"; do
        if [[ "$tag" == *"$query_lower"* ]]; then
            echo -e "  ${GREEN}${ICON_CHECK}${RESET} ${BOLD}${tag}${RESET}: ${CYAN}${agent_tags[$tag]}${RESET}"
            found=1
        fi
    done

    # Also search individual agent names
    local all_agents="arbiter architect auditor critic guard historian librarian orchestrator pair planner reviewer route debugger tester repairer maintainer optimizer researcher deep-researcher investigator inspector pentester sentinel watchdog gatekeeper cryptographer designer writer animator marketer evangelist automaton butler custodian heal govern trace bridge conduit ambassador interactor responder strategist tactician accountant ledger telemetrist polyglot localizer distill"

    for agent in $all_agents; do
        if [[ "$agent" == *"$query_lower"* ]]; then
            echo -e "  ${GREEN}${ICON_CHECK}${RESET} ${CYAN}${agent}${RESET}"
            found=1
        fi
    done

    if [[ $found -eq 0 ]]; then
        echo -e "  ${DIM}No agents found matching '${query}'${RESET}"
        echo -e "  ${DIM}Try: security, testing, design, research, operations${RESET}"
    fi
    echo ""
}

# ============================================================================
# Argument Parsing & Validation
# ============================================================================

parse_args() {
    local _t_parse
    _t_parse=$(_perf_start)

    if [[ $# -lt 2 ]]; then
        usage
    fi

    AGENT="$1"
    TASK="$2"
    PROVIDER=""
    shift 2

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --provider)
                if [[ $# -lt 2 ]]; then
                    log_error "--provider requires a value"
                    exit 2
                fi
                PROVIDER="$2"
                shift 2
                ;;
            --provider=*)
                PROVIDER="${1#*=}"
                shift
                ;;
            --json)
                export EUXIS_JSON=1
                shift
                ;;
            --verbose)
                export EUXIS_DEBUG=1
                shift
                ;;
            --no-color)
                export NO_COLOR=1
                shift
                ;;
            *)
                # Backward compatibility: third positional provider
                if [[ -z "${PROVIDER}" ]]; then
                    PROVIDER="$1"
                    shift
                else
                    log_error "Unexpected argument: $1"
                    exit 2
                fi
                ;;
        esac
    done

    # Security validation: validate agent name and task input
    if ! validate_agent_name "${AGENT}"; then
        _perf_record "cli_error" "$(_perf_elapsed_ms "${_t_parse}")" "cli" "invalid_agent_name"
        exit 2
    fi

    if ! validate_task_input "${TASK}"; then
        _perf_record "cli_error" "$(_perf_elapsed_ms "${_t_parse}")" "cli" "invalid_task_input"
        exit 2
    fi

    local prompt_file
    prompt_file=$(resolve_agent_path "${AGENT}") || true
    if [[ -z "${prompt_file}" || "${AGENT}" == _* ]]; then
        _perf_record "cli_error" "$(_perf_elapsed_ms "${_t_parse}")" "cli" "unknown_agent:${AGENT}"
        log_error "Unknown agent: ${AGENT}"
        echo "Available agents:" >&2
        list_agents >&2
        exit 1
    fi

    if [[ -z "${PROVIDER}" ]]; then
        local session_provider=""
        session_provider=$(resolve_session_provider 2>/dev/null || true)
        if [[ -n "${session_provider}" ]]; then
            PROVIDER="${session_provider}"
        else
            PROVIDER=$(resolve_tiered_provider "${AGENT}")
        fi
    fi

    case "${PROVIDER}" in
        claude|gemini|openai|ollama|qwen|crush|kiro-cli|goose) ;;
        *)
            log_error "Unknown provider: ${PROVIDER}"
            echo "Valid providers: claude, gemini, openai, ollama, qwen, crush, kiro-cli, goose" >&2
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
    export EUXIS_SESSION_ID="${SESSION_ID}"

    ensure_project_dirs "${PROJECT}" "${AGENT}"

    AUDIT_PATH="${PROJECTS_DIR}/${PROJECT}/${AGENT}/audit.md"
    MEMORY_PATH="${PROJECTS_DIR}/${PROJECT}/${AGENT}/memory.md"
    OUTPUT_PATH="${PROJECTS_DIR}/${PROJECT}/${AGENT}/output/${SESSION_ID}.md"

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
# Context Display (PWD Beacon)
# ============================================================================

show_context_fast() {
    [[ -t 1 ]] || return 0

    local CYAN='\033[0;36m'
    local YELLOW='\033[1;33m'
    local NC='\033[0m'
    [[ -n "${NO_COLOR:-}" ]] && { CYAN=''; YELLOW=''; NC=''; }

    # Pure bash git repo detection (super fast)
    local dir="$PWD"
    local repo_root=""
    while [[ "$dir" != "/" && "$dir" != "" ]]; do
        if [[ -d "$dir/.git" ]]; then
            repo_root="$dir"
            break
        elif [[ -f "$dir/.git" ]]; then
            repo_root="$dir"
            break
        fi
        dir="${dir%/*}"
    done

    local branch="no-git"
    if [[ -n "$repo_root" && -f "$repo_root/.git/HEAD" ]]; then
        read -r head_content < "$repo_root/.git/HEAD" || true
        if [[ "$head_content" == ref:\ refs/heads/* ]]; then
            branch="${head_content#ref: refs/heads/}"
        else
            branch="HEAD"
        fi
    fi

    if [[ -z "$repo_root" ]]; then
        repo_root="$PWD"
    fi
    local relative_path=".${PWD#"$repo_root"}"

    echo -e "Scope: ${CYAN}${repo_root##*/}${NC}/${relative_path}  Branch: ${YELLOW}${branch}${NC}"
    echo ""
}

show_context() {
    # Skip context display for non-interactive/piped output
    [[ -t 1 ]] || return 0

    local repo_root branch relative_path
    repo_root=$(git rev-parse --show-toplevel 2>/dev/null || echo "$PWD")
    branch=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "no-git")
    relative_path=".${PWD#"$repo_root"}"

    local CYAN YELLOW NC
    if [[ -n "${NO_COLOR:-}" ]]; then
        CYAN='' YELLOW='' NC=''
    else
        CYAN='\033[0;36m'
        YELLOW='\033[1;33m'
        NC='\033[0m'
    fi

    echo -e "Scope: ${CYAN}${repo_root##*/}${NC}/${relative_path}  Branch: ${YELLOW}${branch}${NC}"
    echo ""
}

# ============================================================================
# Git Branch Guard (Feature Branch Enforcer)
# ============================================================================

git_guard() {
    [[ -t 0 ]] || return 0
    # Skip in dispatch mode (dispatched agents should not prompt for branch)
    [[ "${EUXIS_DISPATCH:-}" == "true" ]] && return 0

    local current_branch
    current_branch=$(git rev-parse --abbrev-ref HEAD 2>/dev/null) || return 0

    case "${current_branch}" in
        feat/*|fix/*|refactor/*|chore/*) return 0 ;;
    esac

    local protected="main master develop production staging"
    if [[ " ${protected} " == *" ${current_branch} "* ]]; then
        echo "[euxis] SAFETY STOP: You are on a protected branch ('${current_branch}')."

        local latest_tag base_ver major minor patch next_patch next_ver new_branch
        latest_tag=$(git describe --tags --abbrev=0 2>/dev/null || echo "v0.0.0")
        base_ver="${latest_tag#v}"
        IFS='.' read -r major minor patch <<< "${base_ver}"
        major="${major:-0}"; minor="${minor:-0}"; patch="${patch:-0}"
        next_patch=$((patch + 1))
        next_ver="v${major}.${minor}.${next_patch}"
        new_branch="feat/${next_ver}"

        echo "   Euxis must work in a feature branch."
        echo "   Proposed Branch: ${new_branch} (derived from ${latest_tag})"

        read -p "   Create and switch to '${new_branch}'? (y/n) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            git checkout -b "${new_branch}"
            echo "[euxis] Switched to ${new_branch}. Proceeding..."
        else
            echo "[euxis] Aborted. Please switch branches manually before running Euxis."
            exit 1
        fi
    else
        echo "[euxis] Warning: Non-standard branch name '${current_branch}'. Proceeding..."
    fi
}

# ============================================================================
# Fast Boot Health Check (hash-based, silent on pass)
# ============================================================================

check_health_fast() {
    # Skip in dispatch mode or when explicitly disabled
    [[ "${EUXIS_HEALTH_CHECK:-}" == "skip" ]] && return 0
    [[ "${EUXIS_DISPATCH:-}" == "true" ]] && return 0

    local hash_file="${TMPDIR:-/tmp}/euxis_health_hash"
    local current_hash
    current_hash=$(ls -lR "${PROMPTS_DIR}" 2>/dev/null | md5sum 2>/dev/null | awk '{print $1}')

    if [[ -f "${hash_file}" ]] && [[ "$(< "${hash_file}")" == "${current_hash}" ]]; then
        return 0
    fi

    local health_script="${EUXIS_HOME}/euxis-cli/bin/euxis-health"
    if [[ -x "${health_script}" ]]; then
        if ! "${health_script}" --silent; then
            echo "[euxis] WARNING: Fleet integrity check failed. Run 'euxis-health' for details." >&2
        fi
    fi

    echo "${current_hash}" > "${hash_file}"
}
