#!/usr/bin/env bash
## ============================================================================
## Euxis Library: Platform Abstraction Layer
## ============================================================================
##
## Unify terminal operations across macOS, Linux, and WSL environments.
##
## This library abstracts platform-specific behaviors (clipboard, notifications,
## file paths, terminal capabilities) into a single API. Consumer code calls
## platform_* functions without conditionals — this layer handles the dispatch.
##
## ARCHITECTURE:
##
##   ┌─────────────┐     ┌───────────────────┐     ┌─────────────────┐
##   │  Consumer   │ ──► │  platform.sh API  │ ──► │ Native Binaries │
##   │   Code      │     │  (this library)   │     │ pbcopy, xclip,  │
##   └─────────────┘     └───────────────────┘     │ clip.exe, etc.  │
##                                                  └─────────────────┘
##
## DATA FLOW:
##   1. On first source, _platform_detect() identifies the runtime environment
##   2. EUXIS_PLATFORM is exported globally (macos | linux | wsl | unknown)
##   3. All platform_* functions dispatch based on this cached value
##
## DEPENDENCIES:
##   - bash 4.0+ (associative arrays, [[ ]])
##
##   | Platform | Clipboard    | Notifications    | Open URLs      |
##   |----------|--------------|------------------|----------------|
##   | macOS    | pbcopy       | osascript        | open           |
##   | Linux    | xclip/wl-copy| notify-send      | xdg-open       |
##   | WSL      | clip.exe     | wsl-notify-send  | explorer.exe   |
##
## PLATFORM NOTES:
##
## ### Platform: macOS
##   - Uses native Cocoa APIs via pbcopy/pbpaste and osascript
##   - Homebrew prefix auto-detected (/opt/homebrew for Apple Silicon)
##   - XDG paths mapped to ~/Library/Application Support
##
## ### Platform: Linux
##   - Supports both X11 (xclip/xsel) and Wayland (wl-copy/wl-paste)
##   - Falls back gracefully if clipboard tools missing
##   - Follows XDG Base Directory Specification
##
## ### Platform: WSL
##   - WSL1 vs WSL2 detected via /run/WSL presence
##   - Clipboard uses PowerShell interop (clip.exe unreliable in WSL2)
##   - Notifications via wsl-notify-send or native Windows Toast API
##   - File paths bridged via /mnt/c and wslpath when needed
##
## SECURITY:
##   - No secrets handling in this library
##   - Clipboard operations are ephemeral (no persistence)
##   - PowerShell commands use -NoProfile to prevent injection
##
## IDEMPOTENCY:
##   - Safe to source multiple times (include guard protects re-entry)
##   - EUXIS_PLATFORM computed once and cached
##
## USAGE:
##   source "${EUXIS_HOME}/euxis-core/lib/platform.sh"
##   platform_clipboard_copy "Hello, World"
##   platform_notify "Euxis" "Deployment complete"
##   platform_open "https://euxis.co"
##
## ============================================================================

# Include guard — prevents duplicate sourcing
[[ -n "${_EUXIS_LIB_PLATFORM:-}" ]] && return; _EUXIS_LIB_PLATFORM=1

set -euo pipefail

# ============================================================================
# Platform Detection
# ============================================================================

## _platform_detect — Identify runtime environment (macos | linux | wsl).
##
## Detection priority: WSL indicators → Darwin kernel → Linux kernel.
## WSL detection uses three signals: WSL_DISTRO_NAME, /proc/version, WSLENV.
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

## _wsl_version — Detect WSL generation (1 or 2).
##
## WSL2 identified by /run/WSL directory presence. Returns "0" if not WSL.
## This affects I/O performance expectations (WSL2 has native Linux FS).
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
#
# Path resolution follows XDG on Linux/WSL, Apple conventions on macOS.
#
# | Function              | macOS                              | Linux/WSL                    |
# |-----------------------|------------------------------------|------------------------------|
# | platform_config_dir   | ~/Library/Application Support/euxis| $XDG_CONFIG_HOME/euxis       |
# | platform_data_dir     | ~/Library/Application Support/euxis| $XDG_DATA_HOME/euxis         |
# | platform_cache_dir    | ~/Library/Caches/euxis             | $XDG_CACHE_HOME/euxis        |
# | platform_temp_dir     | $TMPDIR/euxis-$$                   | /tmp/euxis-$$                |

## platform_config_dir — Return platform-appropriate config directory.
platform_config_dir() {
    case "$EUXIS_PLATFORM" in
        macos)  echo "${HOME}/Library/Application Support/euxis" ;;
        *)      echo "${XDG_CONFIG_HOME:-$HOME/.config}/euxis" ;;
    esac
}

## platform_data_dir — Return platform-appropriate data directory.
platform_data_dir() {
    case "$EUXIS_PLATFORM" in
        macos)  echo "${HOME}/Library/Application Support/euxis" ;;
        *)      echo "${XDG_DATA_HOME:-$HOME/.local/share}/euxis" ;;
    esac
}

## platform_cache_dir — Return platform-appropriate cache directory.
platform_cache_dir() {
    case "$EUXIS_PLATFORM" in
        macos)  echo "${HOME}/Library/Caches/euxis" ;;
        *)      echo "${XDG_CACHE_HOME:-$HOME/.cache}/euxis" ;;
    esac
}

## platform_temp_dir — Return session-scoped temp directory.
platform_temp_dir() {
    echo "${TMPDIR:-/tmp}/euxis-$$"
}

# ============================================================================
# Clipboard Operations
# ============================================================================
#
# ### Platform: macOS
#   Uses pbcopy/pbpaste (always available).
#
# ### Platform: Linux
#   Wayland: wl-copy/wl-paste (preferred)
#   X11: xclip -selection clipboard (fallback)
#   X11: xsel --clipboard (second fallback)
#
# ### Platform: WSL
#   Uses PowerShell Set-Clipboard/Get-Clipboard for reliability.
#   clip.exe is write-only and strips trailing newlines — avoided.
#
# RETURNS:
#   0 on success, 1 if no clipboard tool available.

## platform_clipboard_copy — Copy text to system clipboard.
##
## ARGUMENTS:
##   $1  (string)  Content to copy
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

## platform_clipboard_paste — Read text from system clipboard.
##
## OUTPUTS:
##   stdout  Clipboard contents (CRLF converted to LF on WSL)
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
#
# ### Platform: macOS
#   Uses osascript to trigger native Notification Center alerts.
#
# ### Platform: Linux
#   Uses notify-send (libnotify) with urgency levels.
#
# ### Platform: WSL
#   Prefers wsl-notify-send if available, falls back to Windows Toast API
#   via PowerShell. Toast notifications require Windows 10 1903+.

## platform_notify — Display native desktop notification.
##
## ARGUMENTS:
##   $1  (string)  Notification title
##   $2  (string)  Notification message body
##   $3  (string)  Urgency: low | normal | critical (default: normal)
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
#
# Opens URLs in default browser, files in associated applications.
#
# ### Platform: macOS — Uses `open` (LaunchServices)
# ### Platform: Linux — Uses xdg-open (freedesktop.org standard)
# ### Platform: WSL — Uses explorer.exe (bridges to Windows shell)

## platform_open — Open URL or file in default application.
##
## ARGUMENTS:
##   $1  (string)  URL or file path to open
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
#
# Feature detection for progressive enhancement of CLI output.
# Used by ui.sh to decide color depth and glyph rendering.
#
# | Function                    | Checks                              |
# |-----------------------------|-------------------------------------|
# | platform_supports_256color  | TERM contains "256color"            |
# | platform_supports_truecolor | COLORTERM is "truecolor" or "24bit" |
# | platform_supports_unicode   | LANG/LC_* contains "UTF-8"          |

## platform_supports_256color — Check for 256-color terminal support.
platform_supports_256color() {
    [[ "${TERM:-}" == *256color* ]] || [[ "${COLORTERM:-}" == "truecolor" ]] || [[ "${COLORTERM:-}" == "24bit" ]]
}

## platform_supports_truecolor — Check for 24-bit color support.
platform_supports_truecolor() {
    [[ "${COLORTERM:-}" == "truecolor" ]] || [[ "${COLORTERM:-}" == "24bit" ]]
}

## platform_supports_unicode — Check for UTF-8 locale support.
platform_supports_unicode() {
    [[ "${LANG:-}" == *UTF-8* ]] || [[ "${LC_ALL:-}" == *UTF-8* ]] || [[ "${LC_CTYPE:-}" == *UTF-8* ]]
}

## platform_term_width — Get terminal width in columns (default: 80).
platform_term_width() {
    if command -v tput &>/dev/null; then
        tput cols 2>/dev/null || echo 80
    else
        echo 80
    fi
}

## platform_term_height — Get terminal height in rows (default: 24).
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

## platform_is_interactive — Check if running in interactive terminal.
##
## Returns true (0) if both stdin and stdout are TTYs.
## Used to decide whether to show spinners, colors, or prompts.
platform_is_interactive() {
    [[ -t 0 ]] && [[ -t 1 ]]
}

## platform_get_shell — Get user's default shell name (bash, zsh, fish, etc).
platform_get_shell() {
    basename "${SHELL:-/bin/bash}"
}

# ============================================================================
# Homebrew Support (macOS Only)
# ============================================================================
#
# ### Platform: macOS
#   - Apple Silicon: /opt/homebrew
#   - Intel: /usr/local
#
# ### Platform: Linux/WSL
#   Returns empty string (Linuxbrew not supported by this function).

## platform_homebrew_prefix — Get Homebrew installation prefix on macOS.
##
## OUTPUTS:
##   stdout  Homebrew prefix path, or empty if not macOS/not installed
platform_homebrew_prefix() {
    if [[ "$EUXIS_PLATFORM" == "macos" ]]; then
        if [[ -d "/opt/homebrew" ]]; then
            echo "/opt/homebrew"  # Apple Silicon
        elif [[ -d "/usr/local/Homebrew" ]]; then
            echo "/usr/local"     # Intel
        fi
    fi
}
