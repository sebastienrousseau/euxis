#!/usr/bin/env bash
#
# euxis - Enterprise Unified eXecution Intelligence System
#
# USAGE:
#     euxis <command> [args...]
#     euxis <agent> <task> [provider]
#     euxis --help
#
# DESCRIPTION:
#     Primary entry point for the Euxis Fleet system. Coordinates 53 specialist
#     AI agents across multiple providers for engineering tasks. Supports single
#     agent execution, fleet dispatch, squad deployment, and playbook execution.
#
# OPTIONS:
#     -h, --help          Show this help message and exit
#     --version           Show version information
#
# COMMANDS:
#     dispatch <manifest> Deploy fleet from architect manifest
#     verify              Run pre-commit quality gate (5 checks)
#     health [--silent]   System health check
#     squad <cmd> [args]  Squad management and deployment
#     playbook <cmd>      Phased squad execution via playbooks
#     combo <cmd>         Sequential agent chain execution
#     cortex <cmd>        Tri-typed vector memory operations
#     [Additional commands - see usage output]
#
# AGENT MODE:
#     euxis <agent> <task> [provider]
#
#     agent      One of 53 available agents (see: euxis help agents)
#     task       Task description in quotes
#     provider   AI provider: claude, gemini, openai, ollama, qwen,
#                crush, kiro-cli, goose (auto-selected if omitted)
#
# ENVIRONMENT VARIABLES:
#     EUXIS_HOME          Euxis installation directory (default: ~/.euxis)
#     EUXIS_PROJECT       Project name (default: derived from pwd)
#     EUXIS_SESSION_ID    Session identifier (default: timestamp)
#     EUXIS_DEBUG         Enable debug output (0|1, default: 0)
#
# DEPENDENCIES:
#     - bash 4.0+
#     - git (for repository operations)
#     - At least one AI provider CLI installed
#
# EXAMPLES:
#     # Single agent execution
#     euxis architect "Review the authentication module"
#
#     # Override provider
#     euxis debugger "Find the memory leak" ollama
#
#     # Fleet operations
#     euxis squad deploy quality
#     euxis playbook run zero-to-one
#
#     # System operations
#     euxis verify
#     euxis health
#
# EXIT CODES:
#     0    Success
#     1    General error
#     2    Usage error
#     3    Agent not found
#     4    Provider error

set -euo pipefail

EUXIS_HOME="${EUXIS_HOME:-$HOME/.euxis}"

# Fast-path: handle --version and --help without sourcing large libraries
case "${1:-}" in
  --version)
    echo "euxis 0.1.0"
    exit 0
    ;;
  "" | -h | --help | help)
    # Inline ANSI colors for ultra-fast startup (<2ms)
    [[ -n "${NO_COLOR:-}" || ! -t 1 ]] && NO_COLOR=1
    if [[ -z "${NO_COLOR:-}" ]]; then
        RESET=$'\033[0m'; BOLD=$'\033[1m'; DIM=$'\033[2m'; RED=$'\033[38;5;203m'; GREEN=$'\033[38;5;42m'; YELLOW=$'\033[38;5;220m'; BLUE=$'\033[38;5;69m'; MAGENTA=$'\033[38;5;211m'; CYAN=$'\033[38;5;87m'; WHITE=$'\033[38;5;255m'
        ICON_CHECK="✓"; ICON_BULLET="▸"; ICON_ARROW="→"; ICON_BOLT="⚡"
        # Banner
        printf "\n%s%s    ███████╗██╗   ██╗██╗  ██╗██╗███████╗%s\n" "$BOLD" "$MAGENTA" "$RESET"
        printf "%s%s    ██╔════╝██║   ██║╚██╗██╔╝██║██╔════╝%s\n" "$BOLD" "$MAGENTA" "$RESET"
        printf "%s%s    █████╗  ██║   ██║ ╚███╔╝ ██║███████╗%s\n" "$BOLD" "$CYAN" "$RESET"
        printf "%s%s    ██╔══╝  ██║   ██║ ██╔██╗ ██║╚════██║%s\n" "$BOLD" "$CYAN" "$RESET"
        printf "%s%s    ███████╗╚██████╔╝██╔╝ ██╗██║███████║%s\n" "$BOLD" "$MAGENTA" "$RESET"
        printf "%s%s    ╚══════╝ ╚═════╝ ╚═╝  ╚═╝╚═╝╚══════╝%s\n" "$BOLD" "$MAGENTA" "$RESET"
        printf "    %s%s⚡ v0.0.1%s\n\n" "$BOLD" "$YELLOW" "$RESET"
    else
        RESET=''; BOLD=''; DIM=''; RED=''; GREEN=''; YELLOW=''; BLUE=''; MAGENTA=''; CYAN=''; WHITE=''
        ICON_CHECK="*"; ICON_BULLET=">"; ICON_ARROW="->"; ICON_BOLT="*"
        printf "\n    EUXIS\n    * v0.0.1\n\n"
    fi

    # Fast Context Detector
    dir="$PWD"; repo_root=""
    while [[ "$dir" != "/" && "$dir" != "" ]]; do
        if [[ -d "$dir/.git" || -f "$dir/.git" ]]; then repo_root="$dir"; break; fi
        dir="${dir%/*}"
    done
    branch="no-git"
    if [[ -n "$repo_root" && -f "$repo_root/.git/HEAD" ]]; then
        read -r head_content < "$repo_root/.git/HEAD" || true
        [[ "$head_content" == ref:\ refs/heads/* ]] && branch="${head_content#ref: refs/heads/}" || branch="HEAD"
    fi
    [[ -z "$repo_root" ]] && repo_root="$PWD"
    echo -e "Scope: ${CYAN}${repo_root##*/}${RESET}/.${PWD#"$repo_root"}  Branch: ${YELLOW}${branch}${RESET}\n"

    # Fast Usage Print
    echo -e "  ${DIM}Enterprise Unified eXecution Intelligence System${RESET}"
    echo -e "  ${DIM}Multi-Provider Agent Framework • 53 Agents${RESET}\n"
    echo -e "  ${BOLD}${WHITE}USAGE${RESET}"
    echo -e "    ${CYAN}\$${RESET} euxis ${DIM}<command> [options]${RESET}"
    echo -e "    ${CYAN}\$${RESET} euxis ${DIM}<agent> <task> [--provider <name>]${RESET}\n"
    
    echo -e "  ${BOLD}${CYAN}${ICON_BULLET} CORE COMMANDS${RESET}"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "agent <cmd>" "Manage agent plugins (register, list)"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "squad <cmd>" "Manage agent squads (list, deploy, info)"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "combo <cmd>" "Chain agents sequentially (list, run)"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "playbook <cmd>" "Phased execution with gates (list, run)"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "dispatch <manifest>" "Deploy fleet from architect manifest"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n\n" "cortex <cmd>" "Vector memory interface (ChromaDB)"

    echo -e "  ${BOLD}${CYAN}${ICON_BULLET} QUALITY & INFRA${RESET}"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "verify" "Run pre-commit quality gates"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "health" "Check system probes and connectivity"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "certify" "Full certification pipeline"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "deploy" "Spin up via Docker Compose"
    printf "    ${GREEN}${ICON_CHECK}${RESET} ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n\n" "doctor" "Verify installation and environment"

    echo -e "  ${BOLD}${CYAN}${ICON_BULLET} AGENT ECOSYSTEM (50 Total)${RESET}"
    printf "    ${DIM}%-7s${RESET} %s\n" "Core:" "architect, orchestrator, planner, reviewer, guard, auditor..."
    printf "    ${DIM}%-7s${RESET} %s\n\n" "Fleet:" "debugger, tester, pentester, researcher, designer, writer..."
    echo -e "    Run ${CYAN}euxis agents${RESET} to see the full list with descriptions."
    echo -e "    Run ${CYAN}euxis search <keyword>${RESET} to find agents by capability.\n"

    echo -e "  ${BOLD}${CYAN}${ICON_BULLET} EXAMPLES${RESET}"
    echo -e "    ${CYAN}\$${RESET} euxis architect ${DIM}\"Design a REST API for user auth\"${RESET}"
    echo -e "    ${CYAN}\$${RESET} euxis squad deploy quality ${DIM}\"Audit the payment module\"${RESET}"
    echo -e "       ${DIM}${ICON_ARROW} Deploys: reviewer, inspector, sentinel, pentester, auditor${RESET}"
    echo -e "    ${CYAN}\$${RESET} euxis combo run envision ${DIM}\"Build a CLI for data pipelines\"${RESET}"
    echo -e "       ${DIM}${ICON_ARROW} Chain: deep-researcher ${ICON_ARROW} planner ${ICON_ARROW} architect ${ICON_ARROW} reviewer${RESET}"
    echo -e "    ${CYAN}\$${RESET} euxis playbook run python ${DIM}\"Refactor the utils module\"${RESET}"
    echo -e "       ${DIM}${ICON_ARROW} Phases: analyze ${ICON_ARROW} implement ${ICON_ARROW} test ${ICON_ARROW} review${RESET}\n"

    echo -e "  ${BOLD}${CYAN}${ICON_BULLET} GLOBAL FLAGS${RESET}"
    printf "    ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "--provider <name>" "Override AI provider (claude, gemini, openai, ollama...)"
    printf "    ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "--json" "Output as JSON (for piping to jq)"
    printf "    ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n" "--verbose" "Enable verbose logging"
    printf "    ${CYAN}%-20s${RESET} ${DIM}%s${RESET}\n\n" "--no-color" "Disable colored output"

    echo -e "  ${DIM}────────────────────────────────────────────────────────${RESET}"
    echo -e "  ${DIM}Docs:${RESET} ${CYAN}https://docs.euxis.co${RESET}  ${DIM}Issues:${RESET} ${CYAN}github.com/euxis/euxis${RESET}\n"
    exit 0
    ;;
  agents)
    source "${EUXIS_HOME}/euxis-core/lib/cli.sh"
    usage_agents
    exit 0
    ;;
  search)
    source "${EUXIS_HOME}/euxis-core/lib/cli.sh"
    usage_search "${2:-}"
    exit 0
    ;;
  doctor)
    exec "${EUXIS_HOME}/euxis-cli/bin/euxis-doctor" "${@:2}"
    ;;
esac

# ============================================================================
# Full boot: only reached for agent execution and subcommands.
# ============================================================================

if [[ -f "${EUXIS_HOME}/euxis-core/lib/ui.sh" ]]; then
  # shellcheck source=core/lib/ui.sh
  source "${EUXIS_HOME}/euxis-core/lib/ui.sh"
  ui_auto_header_args "${1:-}"
  if ui_is_subcommand "${1:-}"; then
    export EUXIS_UI_SUPPRESS=1
  fi
fi

PROMPTS_DIR="${EUXIS_HOME}/euxis-core/agents/prompts"
PROJECTS_DIR="${EUXIS_HOME}/euxis-runtime/data/projects"
source "${EUXIS_HOME}/euxis-core/lib/common.sh"
source "${EUXIS_HOME}/euxis-core/lib/providers.sh"
source "${EUXIS_HOME}/euxis-core/lib/agents.sh"
source "${EUXIS_HOME}/euxis-core/lib/memory.sh"
source "${EUXIS_HOME}/euxis-core/lib/session.sh"
source "${EUXIS_HOME}/euxis-core/lib/template.sh"
source "${EUXIS_HOME}/euxis-core/lib/skill-detector.sh"
source "${EUXIS_HOME}/euxis-core/lib/prompt.sh"
source "${EUXIS_HOME}/euxis-core/lib/cli.sh"
source "${EUXIS_HOME}/euxis-core/lib/dispatch.sh"

# Fast boot health check (moved to cli.sh)
check_health_fast

# Context display (moved to cli.sh)
show_context

# Git branch guard (moved to cli.sh)
git_guard

# ============================================================================
# Entry Point
# ============================================================================

EUXIS_BIN="${EUXIS_HOME}/euxis-cli/bin"

# Dispatch to appropriate handler
dispatch_command "$@"
