#!/usr/bin/env bash
#
# Euxis - Multi-Provider AI Agent Framework
# Usage: euxis <agent> <task> [provider]
#        euxis delegate <agent> <task> [provider]
#
# Agents are discovered dynamically from ~/.euxis/prompts/{core,fleet}/*.txt
# Providers: claude (default), gemini, openai, ollama, opencode, qwen, crush, kilo, amazon-q, goose
#

set -euo pipefail

# ============================================================================
# Configuration & Library Loading
# ============================================================================

EUXIS_HOME="${HOME}/.euxis"
PROMPTS_DIR="${EUXIS_HOME}/prompts"
PROJECTS_DIR="${EUXIS_HOME}/data/projects"
source "${EUXIS_HOME}/bin/lib/common.sh"
source "${EUXIS_HOME}/bin/lib/providers.sh"
source "${EUXIS_HOME}/bin/lib/agents.sh"
source "${EUXIS_HOME}/bin/lib/memory.sh"
source "${EUXIS_HOME}/bin/lib/session.sh"
source "${EUXIS_HOME}/bin/lib/template.sh"
source "${EUXIS_HOME}/bin/lib/skill-detector.sh"
source "${EUXIS_HOME}/bin/lib/prompt.sh"
source "${EUXIS_HOME}/bin/lib/cli.sh"
source "${EUXIS_HOME}/bin/lib/dispatch.sh"

# Fast boot health check (moved to cli.sh)
check_health_fast

# Context display (moved to cli.sh)
show_context

# Git branch guard (moved to cli.sh)
git_guard

# Usage function (moved to cli.sh)

# Argument parsing, session setup, and output capture (moved to cli.sh)

# Main execution (moved to dispatch.sh)

# Delegate function (moved to dispatch.sh)

# ============================================================================
# Entry Point
# ============================================================================

EUXIS_BIN="${EUXIS_HOME}/bin"

# Dispatch to appropriate handler
dispatch_command "$@"
