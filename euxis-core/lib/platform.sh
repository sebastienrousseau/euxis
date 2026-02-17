#!/usr/bin/env bash
#
# Euxis Platform Utilities - Cross-platform compatibility layer
# Supports: macOS, Linux, WSL
#

[[ -n "${_EUXIS_LIB_PLATFORM:-}" ]] && return; _EUXIS_LIB_PLATFORM=1

set -euo pipefail

# ============================================================================
# Platform Detection
# ============================================================================

_platform_detect() {
    # WSL detection: check multiple indicators
    if [[ -n "${WSL_DISTRO_NAME:-}" ]]; then
        echo "wsl"
    elif [[ -f /proc/version ]] && grep -qi microsoft /proc/version 2>/dev/null; then
        echo "wsl"
    elif [[ -n "${WSLENV:-}" ]]; then
        echo "wsl"
    elif [[ "$(uname -s)" == "Darwin" ]]; then
        echo "macos"
    elif [[ "$(uname -s)" == "Linux" ]]; then
        echo "linux"
    else
        echo "unknown"
    fi
}

# WSL version detection (1 or 2)
_wsl_version() {
    if [[ "$EUXIS_PLATFORM" != "wsl" ]]; then
        echo "0"
        return
    fi
    # WSL2 has /run/WSL directory
    if [[ -d /run/WSL ]]; then
        echo "2"
    else
        echo "1"
    fi
}

EUXIS_PLATFORM="${EUXIS_PLATFORM:-$(_platform_detect)}"
export EUXIS_PLATFORM

# ============================================================================
# Path Handling (XDG Base Directory Specification)
# ============================================================================

# Config directory
platform_config_dir() {
    case "$EUXIS_PLATFORM" in
        macos)  echo "${HOME}/Library/Application Support/euxis" ;;
        *)      echo "${XDG_CONFIG_HOME:-$HOME/.config}/euxis" ;;
    esac
}

# Data directory
platform_data_dir() {
    case "$EUXIS_PLATFORM" in
        macos)  echo "${HOME}/Library/Application Support/euxis" ;;
        *)      echo "${XDG_DATA_HOME:-$HOME/.local/share}/euxis" ;;
    esac
}

# Cache directory
platform_cache_dir() {
    case "$EUXIS_PLATFORM" in
        macos)  echo "${HOME}/Library/Caches/euxis" ;;
        *)      echo "${XDG_CACHE_HOME:-$HOME/.cache}/euxis" ;;
    esac
}

# Temp directory
platform_temp_dir() {
    echo "${TMPDIR:-/tmp}/euxis-$$"
}

# ============================================================================
# Clipboard Operations
# ============================================================================

platform_clipboard_copy() {
    local content="$1"
    case "$EUXIS_PLATFORM" in
        wsl)
            echo -n "$content" | powershell.exe -NoProfile -Command "Set-Clipboard -Value ([Console]::In.ReadToEnd())" 2>/dev/null
            ;;
        macos)
            echo -n "$content" | pbcopy
            ;;
        linux)
            if command -v wl-copy &>/dev/null; then
                echo -n "$content" | wl-copy
            elif command -v xclip &>/dev/null; then
                echo -n "$content" | xclip -selection clipboard
            elif command -v xsel &>/dev/null; then
                echo -n "$content" | xsel --clipboard --input
            else
                return 1
            fi
            ;;
        *)
            return 1
            ;;
    esac
}

platform_clipboard_paste() {
    case "$EUXIS_PLATFORM" in
        wsl)
            powershell.exe -NoProfile -Command "Get-Clipboard" 2>/dev/null | tr -d '\r'
            ;;
        macos)
            pbpaste
            ;;
        linux)
            if command -v wl-paste &>/dev/null; then
                wl-paste
            elif command -v xclip &>/dev/null; then
                xclip -selection clipboard -o
            elif command -v xsel &>/dev/null; then
                xsel --clipboard --output
            else
                return 1
            fi
            ;;
        *)
            return 1
            ;;
    esac
}

# ============================================================================
# Notifications
# ============================================================================

platform_notify() {
    local title="$1"
    local message="$2"
    local urgency="${3:-normal}"  # low, normal, critical

    case "$EUXIS_PLATFORM" in
        wsl)
            if command -v wsl-notify-send &>/dev/null; then
                wsl-notify-send --category "euxis" "$title" "$message"
            elif command -v powershell.exe &>/dev/null; then
                powershell.exe -NoProfile -Command "
                    [Windows.UI.Notifications.ToastNotificationManager, Windows.UI.Notifications, ContentType = WindowsRuntime] | Out-Null
                    [Windows.Data.Xml.Dom.XmlDocument, Windows.Data.Xml.Dom.XmlDocument, ContentType = WindowsRuntime] | Out-Null
                    \$template = '<toast><visual><binding template=\"ToastText02\"><text id=\"1\">$title</text><text id=\"2\">$message</text></binding></visual></toast>'
                    \$xml = New-Object Windows.Data.Xml.Dom.XmlDocument
                    \$xml.LoadXml(\$template)
                    \$toast = [Windows.UI.Notifications.ToastNotification]::new(\$xml)
                    [Windows.UI.Notifications.ToastNotificationManager]::CreateToastNotifier('Euxis').Show(\$toast)
                " 2>/dev/null || true
            fi
            ;;
        macos)
            osascript -e "display notification \"$message\" with title \"$title\"" 2>/dev/null || true
            ;;
        linux)
            if command -v notify-send &>/dev/null; then
                notify-send -u "$urgency" "$title" "$message"
            fi
            ;;
    esac
}

# ============================================================================
# Open URLs/Files
# ============================================================================

platform_open() {
    local target="$1"
    case "$EUXIS_PLATFORM" in
        wsl)
            cmd.exe /c start "" "$target" 2>/dev/null || explorer.exe "$target" 2>/dev/null
            ;;
        macos)
            open "$target"
            ;;
        linux)
            if command -v xdg-open &>/dev/null; then
                xdg-open "$target"
            elif command -v gnome-open &>/dev/null; then
                gnome-open "$target"
            fi
            ;;
    esac
}

# ============================================================================
# Terminal Capabilities
# ============================================================================

platform_supports_256color() {
    [[ "${TERM:-}" == *256color* ]] || [[ "${COLORTERM:-}" == "truecolor" ]] || [[ "${COLORTERM:-}" == "24bit" ]]
}

platform_supports_truecolor() {
    [[ "${COLORTERM:-}" == "truecolor" ]] || [[ "${COLORTERM:-}" == "24bit" ]]
}

platform_supports_unicode() {
    [[ "${LANG:-}" == *UTF-8* ]] || [[ "${LC_ALL:-}" == *UTF-8* ]] || [[ "${LC_CTYPE:-}" == *UTF-8* ]]
}

platform_term_width() {
    if command -v tput &>/dev/null; then
        tput cols 2>/dev/null || echo 80
    else
        echo 80
    fi
}

platform_term_height() {
    if command -v tput &>/dev/null; then
        tput lines 2>/dev/null || echo 24
    else
        echo 24
    fi
}

# ============================================================================
# Process Management
# ============================================================================

platform_is_interactive() {
    [[ -t 0 ]] && [[ -t 1 ]]
}

platform_get_shell() {
    basename "${SHELL:-/bin/bash}"
}

# ============================================================================
# Homebrew Support (macOS)
# ============================================================================

platform_homebrew_prefix() {
    if [[ "$EUXIS_PLATFORM" == "macos" ]]; then
        if [[ -d "/opt/homebrew" ]]; then
            echo "/opt/homebrew"  # Apple Silicon
        elif [[ -d "/usr/local/Homebrew" ]]; then
            echo "/usr/local"     # Intel
        fi
    fi
}
