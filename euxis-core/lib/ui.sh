#!/usr/bin/env bash
# Lightweight UI helpers inspired by Bubble Tea package-manager example.

[[ -n "${_EUXIS_LIB_UI:-}" ]] && return; _EUXIS_LIB_UI=1

set -euo pipefail

# Global UI state
_EUXIS_UI_HEADER_SHOWN=${_EUXIS_UI_HEADER_SHOWN:-0}
_EUXIS_UI_PROGRESS_TOTAL=0
_EUXIS_UI_PROGRESS_CURRENT=0
_EUXIS_UI_SPINNER_INDEX=0

# CI/CD environment detection
_ui_is_ci() {
  # Common CI environment variables
  [[ -n "${CI:-}" ]] ||
  [[ -n "${GITHUB_ACTIONS:-}" ]] ||
  [[ -n "${GITLAB_CI:-}" ]] ||
  [[ -n "${CIRCLECI:-}" ]] ||
  [[ -n "${JENKINS_URL:-}" ]] ||
  [[ -n "${TRAVIS:-}" ]] ||
  [[ -n "${BUILDKITE:-}" ]] ||
  [[ -n "${DRONE:-}" ]] ||
  [[ -n "${TF_BUILD:-}" ]] ||  # Azure DevOps
  [[ -n "${CODEBUILD_BUILD_ID:-}" ]]  # AWS CodeBuild
}

# Dumb terminal detection
_ui_is_dumb_terminal() {
  [[ "${TERM:-}" == "dumb" ]] ||
  [[ -z "${TERM:-}" ]] ||
  [[ "${TERM:-}" == "unknown" ]]
}

# Full UI capability check
ui_enabled() {
  # Not a TTY
  [[ -t 1 ]] || return 1
  # Explicitly disabled
  [[ "${EUXIS_UI:-1}" != "0" ]] || return 1
  # Dumb terminal
  ! _ui_is_dumb_terminal || return 1
  # In CI (allow override with EUXIS_UI=1)
  if _ui_is_ci && [[ "${EUXIS_UI:-}" != "1" ]]; then
    return 1
  fi
  return 0
}

# Color output capability
_ui_colors_enabled() {
  # Explicit NO_COLOR standard
  [[ -z "${NO_COLOR:-}" ]] || return 1
  # Dumb terminal
  ! _ui_is_dumb_terminal || return 1
  # CI without explicit color enable
  if _ui_is_ci && [[ "${FORCE_COLOR:-}" != "1" ]]; then
    return 1
  fi
  return 0
}

_ui_color() {
  local code="$1"
  if ! _ui_colors_enabled; then
    printf ''
  else
    printf '\033[%sm' "$code"
  fi
}

_ui_reset() {
  if [[ -n "${NO_COLOR:-}" ]]; then
    printf ''
  else
    printf '\033[0m'
  fi
}

ui_cyan() {
  _ui_color '36'
}

ui_reset() {
  _ui_reset
}

_ui_spinner_char() {
  local chars=("⠋" "⠙" "⠹" "⠸" "⠼" "⠴" "⠦" "⠧" "⠇" "⠏")
  local idx=$(( _EUXIS_UI_SPINNER_INDEX % 10 ))
  _EUXIS_UI_SPINNER_INDEX=$(( _EUXIS_UI_SPINNER_INDEX + 1 ))
  printf '%s' "${chars[$idx]}"
}

_ui_progress_bar() {
  local current="$1" total="$2" width="${3:-24}"
  if [[ "$total" -le 0 ]]; then
    total=1
  fi
  if [[ "$current" -gt "$total" ]]; then
    current="$total"
  fi
  local filled=$(( current * width / total ))
  local empty=$(( width - filled ))
  printf '%0.s█' $(seq 1 "$filled")
  printf '%0.s░' $(seq 1 "$empty")
}

# Unicode support detection
_ui_supports_unicode() {
  [[ "${LANG:-}" == *UTF-8* ]] || [[ "${LC_ALL:-}" == *UTF-8* ]] || [[ "${TERM:-}" == *256color* ]] || [[ "${TERM:-}" == *xterm* ]]
}

ui_header() {
  local title="$1"
  local subtitle="${2:-}"
  ui_enabled || return 0
  [[ "${EUXIS_UI_SUPPRESS:-0}" == "1" ]] && return 0
  if [[ "$_EUXIS_UI_HEADER_SHOWN" -eq 1 ]]; then
    return 0
  fi
  _EUXIS_UI_HEADER_SHOWN=1
  export _EUXIS_UI_HEADER_SHOWN

  local cyan magenta yellow reset bold dim
  cyan=$(_ui_color '36')
  magenta=$(_ui_color '35')
  yellow=$(_ui_color '33')
  bold=$(_ui_color '1')
  dim=$(_ui_color '2')
  reset=$(_ui_reset)

  printf "\n"
  if _ui_supports_unicode; then
    # Unicode ASCII art logo
    printf "%s%s    ███████╗██╗   ██╗██╗  ██╗██╗███████╗%s\n" "$bold" "$magenta" "$reset"
    printf "%s%s    ██╔════╝██║   ██║╚██╗██╔╝██║██╔════╝%s\n" "$bold" "$magenta" "$reset"
    printf "%s%s    █████╗  ██║   ██║ ╚███╔╝ ██║███████╗%s\n" "$bold" "$cyan" "$reset"
    printf "%s%s    ██╔══╝  ██║   ██║ ██╔██╗ ██║╚════██║%s\n" "$bold" "$cyan" "$reset"
    printf "%s%s    ███████╗╚██████╔╝██╔╝ ██╗██║███████║%s\n" "$bold" "$magenta" "$reset"
    printf "%s%s    ╚══════╝ ╚═════╝ ╚═╝  ╚═╝╚═╝╚══════╝%s\n" "$bold" "$magenta" "$reset"
    printf "    %s%s⚡ v0.0.2%s\n\n" "$bold" "$yellow" "$reset"
  else
    # ASCII fallback for older terminals
    printf "%s%s    EUXIS%s\n" "$bold" "$magenta" "$reset"
    printf "    %s%s* v0.0.2%s\n\n" "$bold" "$yellow" "$reset"
  fi
}

ui_signature() {
  local grey blue glow white reset
  if _ui_colors_enabled; then
    grey='\033[38;2;120;120;120m'
    blue='\033[38;2;0;122;255m'
    glow='\033[1;38;2;0;209;255m'
    white='\033[1;38;2;255;255;255m'
    reset='\033[0m'
  else
    grey=''
    blue=''
    glow=''
    white=''
    reset=''
  fi

  printf "\n  %b────────────────────────────────────────────────────────%b\n" "$grey" "$reset"
  printf "  %bTHE ARCHITECT ᛫ Sebastien Rousseau ᛫ %bhttps://sebastienrousseau.com%b\n" "$grey" "$blue" "$reset"
  printf "  %bTHE ENGINE %bᛞ %bEUXIS ᛫ Enterprise Unified Execution Intelligence System ᛫ %bhttps://euxis.co%b\n\n" "$grey" "$glow" "$white" "$blue" "$reset"
}

ui_auto_header() {
  ui_enabled || return 0
  local cmd
  cmd=$(basename "$0")
  cmd=${cmd#euxis-}
  if [[ "$cmd" == "euxis" || "$cmd" == "euxis.sh" ]]; then
    ui_header "Euxis" "Command Runner"
    return 0
  fi
  # Title case-ish
  cmd=${cmd//-/ }
  ui_header "Euxis ${cmd}" "" 
}

ui_is_subcommand() {
  case "$1" in
    dispatch|loop|council|bus|graph|squad|playbook|combo|synthesize|codex|hooks|verify|health|lint|certify|test|audit|bench|cortex|kaizen|daemon|optimize|sync-docs|deploy|voice|gym|ui)
      return 0
      ;;
  esac
  return 1
}

ui_auto_header_args() {
  ui_enabled || return 0
  local cmd="${1:-}"
  # Show logo for help with special subtitle
  if [[ "$cmd" == "-h" || "$cmd" == "--help" || "$cmd" == "help" ]]; then
    ui_header "Euxis" "v0.0.2"
    return 0
  fi
  if [[ -z "$cmd" ]]; then
    ui_header "Euxis" "Command Runner"
    return 0
  fi
  ui_header "Euxis ${cmd//-/ }" ""
}

ui_progress_begin() {
  local total="$1"
  local label="$2"
  ui_enabled || return 0
  _EUXIS_UI_PROGRESS_TOTAL="$total"
  _EUXIS_UI_PROGRESS_CURRENT=0
  local spinner
  spinner=$(_ui_spinner_char)
  local bar
  bar=$(_ui_progress_bar 0 "$total")
  printf "\r\033[K%s %s  %s  %d/%d" "$spinner" "$label" "$bar" 0 "$total"
}

ui_progress_step() {
  local label="$1"
  ui_enabled || return 0
  _EUXIS_UI_PROGRESS_CURRENT=$(( _EUXIS_UI_PROGRESS_CURRENT + 1 ))
  local spinner
  spinner=$(_ui_spinner_char)
  local bar
  bar=$(_ui_progress_bar "$_EUXIS_UI_PROGRESS_CURRENT" "$_EUXIS_UI_PROGRESS_TOTAL")
  printf "\r\033[K%s %s  %s  %d/%d" "$spinner" "$label" "$bar" "$_EUXIS_UI_PROGRESS_CURRENT" "$_EUXIS_UI_PROGRESS_TOTAL"
}

ui_progress_done() {
  local label="$1"
  ui_enabled || return 0
  local bar
  bar=$(_ui_progress_bar "$_EUXIS_UI_PROGRESS_TOTAL" "$_EUXIS_UI_PROGRESS_TOTAL")
  printf "\r\033[K✓ %s  %s  %d/%d\n" "$label" "$bar" "$_EUXIS_UI_PROGRESS_TOTAL" "$_EUXIS_UI_PROGRESS_TOTAL"
}

ui_task_ok() {
  local label="$1"
  ui_enabled || return 0
  printf "✓ %s\n" "$label"
}

ui_task_warn() {
  local label="$1"
  ui_enabled || return 0
  printf "⚠ %s\n" "$label"
}

ui_task_fail() {
  local label="$1"
  ui_enabled || return 0
  printf "✗ %s\n" "$label"
}

# ============================================================================
# Progress Orchestrator - Multi-line status for squad/combo operations
# ============================================================================

# State tracking
declare -A _EUXIS_ORCHESTRATOR_TASKS
_EUXIS_ORCHESTRATOR_TITLE=""
_EUXIS_ORCHESTRATOR_COUNT=0

# Status constants
readonly UI_STATUS_WAITING="WAITING"
readonly UI_STATUS_RUNNING="RUNNING"
readonly UI_STATUS_SUCCESS="SUCCESS"
readonly UI_STATUS_FAILED="FAILED"
readonly UI_STATUS_SKIPPED="SKIPPED"

# Initialize orchestrator display
ui_orchestrator_begin() {
  local title="$1"
  shift
  local tasks=("$@")

  ui_enabled || return 0

  _EUXIS_ORCHESTRATOR_TITLE="$title"
  _EUXIS_ORCHESTRATOR_COUNT=${#tasks[@]}

  # Initialize all tasks as WAITING
  for task in "${tasks[@]}"; do
    _EUXIS_ORCHESTRATOR_TASKS["$task"]="$UI_STATUS_WAITING"
  done

  _ui_orchestrator_render
}

# Update a task status
ui_orchestrator_update() {
  local task="$1"
  local status="$2"
  local detail="${3:-}"

  ui_enabled || return 0

  _EUXIS_ORCHESTRATOR_TASKS["$task"]="$status"
  if [[ -n "$detail" ]]; then
    _EUXIS_ORCHESTRATOR_TASKS["${task}_detail"]="$detail"
  fi

  _ui_orchestrator_render
}

# Complete orchestrator
ui_orchestrator_done() {
  local success="${1:-true}"

  ui_enabled || return 0

  # Move cursor to end and print summary
  printf "\n"

  local succeeded=0 failed=0
  for task in "${!_EUXIS_ORCHESTRATOR_TASKS[@]}"; do
    [[ "$task" == *_detail ]] && continue
    case "${_EUXIS_ORCHESTRATOR_TASKS[$task]}" in
      "$UI_STATUS_SUCCESS") ((succeeded++)) ;;
      "$UI_STATUS_FAILED")  ((failed++)) ;;
    esac
  done

  if [[ "$success" == "true" ]] && [[ $failed -eq 0 ]]; then
    printf "  \033[32m✓\033[0m %s complete (%d/%d succeeded)\n" "$_EUXIS_ORCHESTRATOR_TITLE" "$succeeded" "$_EUXIS_ORCHESTRATOR_COUNT"
  else
    printf "  \033[31m✗\033[0m %s failed (%d succeeded, %d failed)\n" "$_EUXIS_ORCHESTRATOR_TITLE" "$succeeded" "$failed"
  fi

  # Clear state
  _EUXIS_ORCHESTRATOR_TASKS=()
  _EUXIS_ORCHESTRATOR_TITLE=""
  _EUXIS_ORCHESTRATOR_COUNT=0
}

# Internal render function
_ui_orchestrator_render() {
  local cyan='\033[36m'
  local green='\033[32m'
  local yellow='\033[33m'
  local red='\033[31m'
  local dim='\033[2m'
  local reset='\033[0m'
  local bold='\033[1m'

  # Unicode vs ASCII
  local spinner_char icon_waiting icon_running icon_success icon_failed icon_skipped
  if _ui_supports_unicode; then
    spinner_char=$(_ui_spinner_char)
    icon_waiting="○"
    icon_running="◐"
    icon_success="✓"
    icon_failed="✗"
    icon_skipped="◌"
  else
    spinner_char="*"
    icon_waiting="-"
    icon_running=">"
    icon_success="+"
    icon_failed="x"
    icon_skipped="."
  fi

  # Clear previous lines and redraw
  # Move up N+1 lines (tasks + title)
  local line_count=$(( _EUXIS_ORCHESTRATOR_COUNT + 1 ))
  printf "\033[%dA" "$line_count" 2>/dev/null || true

  # Title line
  printf "\033[K  ${bold}[${spinner_char}]${reset} ${bold}%s${reset}...\n" "$_EUXIS_ORCHESTRATOR_TITLE"

  # Task lines
  for task in "${!_EUXIS_ORCHESTRATOR_TASKS[@]}"; do
    [[ "$task" == *_detail ]] && continue

    local status="${_EUXIS_ORCHESTRATOR_TASKS[$task]}"
    local detail="${_EUXIS_ORCHESTRATOR_TASKS[${task}_detail]:-}"
    local icon color status_text

    case "$status" in
      "$UI_STATUS_WAITING")
        icon="$icon_waiting"
        color="$dim"
        status_text="waiting"
        ;;
      "$UI_STATUS_RUNNING")
        icon="$icon_running"
        color="$yellow"
        status_text="${detail:-running...}"
        ;;
      "$UI_STATUS_SUCCESS")
        icon="$icon_success"
        color="$green"
        status_text="done"
        ;;
      "$UI_STATUS_FAILED")
        icon="$icon_failed"
        color="$red"
        status_text="${detail:-failed}"
        ;;
      "$UI_STATUS_SKIPPED")
        icon="$icon_skipped"
        color="$dim"
        status_text="skipped"
        ;;
    esac

    printf "\033[K    ${color}${icon}${reset} ${cyan}%-14s${reset} ${dim}[%s]${reset}\n" "$task" "$status_text"
  done
}

# Simple inline spinner for single operations
ui_spinner_start() {
  local label="$1"
  ui_enabled || return 0

  _EUXIS_SPINNER_LABEL="$label"
  _EUXIS_SPINNER_PID=""

  # Start background spinner
  (
    while true; do
      for c in "⠋" "⠙" "⠹" "⠸" "⠼" "⠴" "⠦" "⠧" "⠇" "⠏"; do
        printf "\r\033[K  %s %s" "$c" "$_EUXIS_SPINNER_LABEL"
        sleep 0.1
      done
    done
  ) &
  _EUXIS_SPINNER_PID=$!
}

ui_spinner_stop() {
  local success="${1:-true}"
  local message="${2:-}"

  ui_enabled || return 0

  # Kill spinner
  if [[ -n "${_EUXIS_SPINNER_PID:-}" ]]; then
    kill "$_EUXIS_SPINNER_PID" 2>/dev/null || true
    wait "$_EUXIS_SPINNER_PID" 2>/dev/null || true
  fi

  # Print result
  if [[ "$success" == "true" ]]; then
    printf "\r\033[K  \033[32m✓\033[0m %s\n" "${message:-$_EUXIS_SPINNER_LABEL}"
  else
    printf "\r\033[K  \033[31m✗\033[0m %s\n" "${message:-$_EUXIS_SPINNER_LABEL}"
  fi

  _EUXIS_SPINNER_PID=""
  _EUXIS_SPINNER_LABEL=""
}
