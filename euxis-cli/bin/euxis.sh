#!/usr/bin/env bash
#
# euxis - Multi-Provider AI Agent Framework
#
# USAGE:
#     euxis <command> [args...]
#     euxis <agent> <task> [provider]
#     euxis --help
#
# DESCRIPTION:
#     Primary entry point for the Euxis Fleet system. Coordinates 42 specialist
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
#     agent      One of 41 available agents (see: euxis help agents)
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

# ============================================================================
# Fast Path: Handle --help and --version before loading libraries
# ============================================================================

case "${1:-}" in
  --version)
    echo "euxis 0.1.0"
    exit 0
    ;;
  -h | --help | help)
    source "${EUXIS_HOME}/euxis-core/lib/cli.sh"
    usage
    exit 0
    ;;
esac

# ============================================================================
# Full Initialization (only for commands that need it)
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
EUXIS_BIN="${EUXIS_HOME}/euxis-cli/bin"

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

# Fast boot health check
check_health_fast

# Context display
show_context

# Git branch guard
git_guard

# ============================================================================
# Entry Point
# ============================================================================

# Handle special commands
case "${1:-}" in
  agents)
    usage_agents
    ;;
  search)
    usage_search "${2:-}"
    exit 0
    ;;
  doctor)
    exec "${EUXIS_BIN}/euxis-doctor" "${@:2}"
    ;;
esac

# Dispatch to appropriate handler
dispatch_command "$@"
