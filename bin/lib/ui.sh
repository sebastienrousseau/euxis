#!/usr/bin/env bash
# (c) 2026 Euxis Fleet. All rights reserved.
# Lightweight UI helpers inspired by Bubble Tea package-manager example.

[[ -n "${_EUXIS_LIB_UI:-}" ]] && return; _EUXIS_LIB_UI=1

set -euo pipefail

# Global UI state
_EUXIS_UI_HEADER_SHOWN=${_EUXIS_UI_HEADER_SHOWN:-0}
_EUXIS_UI_PROGRESS_TOTAL=0
_EUXIS_UI_PROGRESS_CURRENT=0
_EUXIS_UI_SPINNER_INDEX=0

ui_enabled() {
  [[ -t 1 ]] || return 1
  [[ "${EUXIS_UI:-1}" != "0" ]]
}

_ui_color() {
  local code="$1"
  if [[ -n "${NO_COLOR:-}" ]]; then
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

  local cyan reset
  cyan=$(_ui_color '36')
  reset=$(_ui_reset)

  printf "\n"
  printf "%s┏━┛┃ ┃┃ ┃┛┏━┛%s\n" "$cyan" "$reset"
  printf "%s┏━┛┃ ┃ ┛ ┃━━┃%s\n" "$cyan" "$reset"
  printf "%s━━┛━━┛┛ ┛┛━━┛%s\n" "$cyan" "$reset"
  printf "%s%s%s\n" "$cyan" "$title" "$reset"
  if [[ -n "$subtitle" ]]; then
    printf "%s› %s%s\n" "$cyan" "$subtitle" "$reset"
  fi
  printf "\n"
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
  if [[ -z "$cmd" || "$cmd" == "-h" || "$cmd" == "--help" ]]; then
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
