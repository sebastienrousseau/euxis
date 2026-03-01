#!/usr/bin/env bash
#
# Euxis Installer
# One-line install: curl -fsSL https://euxis.dev/install.sh | bash
#
# This script:
#   1. Detects platform (macOS, Linux, WSL)
#   2. Checks prerequisites
#   3. Clones or updates Euxis
#   4. Sets up shell integration
#   5. Runs doctor to verify
#

set -euo pipefail

# ============================================================================
# Configuration
# ============================================================================

EUXIS_REPO="${EUXIS_REPO:-https://github.com/euxis/euxis.git}"
EUXIS_BRANCH="${EUXIS_BRANCH:-main}"
EUXIS_HOME="${EUXIS_HOME:-$HOME/.euxis}"
VERSION="0.0.3"

# ============================================================================
# Colors & Output (respects NO_COLOR and pipes)
# ============================================================================

setup_colors() {
    if [[ -n "${NO_COLOR:-}" ]] || [[ ! -t 1 ]]; then
        RESET="" BOLD="" DIM=""
        RED="" GREEN="" YELLOW="" CYAN="" MAGENTA=""
    else
        RESET='\033[0m'
        BOLD='\033[1m'
        DIM='\033[2m'
        RED='\033[38;5;203m'
        GREEN='\033[38;5;42m'
        YELLOW='\033[38;5;220m'
        CYAN='\033[38;5;87m'
        MAGENTA='\033[38;5;211m'
    fi
}

setup_icons() {
    if [[ "${LANG:-}" == *UTF-8* ]] || [[ "${LC_ALL:-}" == *UTF-8* ]]; then
        ICON_OK="✓"
        ICON_FAIL="✗"
        ICON_WARN="⚠"
        ICON_ARROW="→"
        ICON_BULLET="▸"
        ICON_BOLT="⚡"
    else
        ICON_OK="+"
        ICON_FAIL="x"
        ICON_WARN="!"
        ICON_ARROW="->"
        ICON_BULLET=">"
        ICON_BOLT="*"
    fi
}

print_logo() {
    echo -e ""
    echo -e "${BOLD}${CYAN}    ███████${RESET}"
    echo -e "${BOLD}${CYAN}    ██${RESET}"
    echo -e "${BOLD}${CYAN}    █████${RESET}"
    echo -e "${BOLD}${CYAN}    ██${RESET}"
    echo -e "${BOLD}${CYAN}    ███████${RESET}"
    echo -e "${BOLD}${YELLOW}    ${ICON_BOLT} v${VERSION}${RESET}"
    echo -e ""
}

info()  { echo -e "  ${CYAN}${ICON_BULLET}${RESET} $1"; }
ok()    { echo -e "  ${GREEN}${ICON_OK}${RESET} $1"; }
warn()  { echo -e "  ${YELLOW}${ICON_WARN}${RESET} $1"; }
error() { echo -e "  ${RED}${ICON_FAIL}${RESET} $1" >&2; }
step()  { echo -e "\n${BOLD}${CYAN}${ICON_BULLET} $1${RESET}"; }

# ============================================================================
# Platform Detection
# ============================================================================

detect_platform() {
    local os arch

    os="$(uname -s)"
    arch="$(uname -m)"

    case "$os" in
        Darwin)
            PLATFORM="macos"
            if [[ "$arch" == "arm64" ]]; then
                ARCH="arm64"
                ARCH_LABEL="Apple Silicon"
            else
                ARCH="x86_64"
                ARCH_LABEL="Intel"
            fi
            ;;
        Linux)
            # Check for WSL
            if grep -qiE "(microsoft|wsl)" /proc/version 2>/dev/null; then
                PLATFORM="wsl"
                if [[ -n "${WSL_DISTRO_NAME:-}" ]]; then
                    ARCH_LABEL="WSL ($WSL_DISTRO_NAME)"
                else
                    ARCH_LABEL="WSL"
                fi
            else
                PLATFORM="linux"
                ARCH_LABEL="Linux"
            fi
            ARCH="$arch"
            ;;
        *)
            error "Unsupported operating system: $os"
            exit 1
            ;;
    esac

    export PLATFORM ARCH ARCH_LABEL
}

# ============================================================================
# Prerequisites
# ============================================================================

check_prerequisites() {
    step "Checking prerequisites"

    local missing=()

    # Required: git
    if command -v git &>/dev/null; then
        ok "git $(git --version | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')"
    else
        missing+=("git")
        error "git not found"
    fi

    # Required: bash 4+
    local bash_version="${BASH_VERSION%%.*}"
    if [[ "$bash_version" -ge 4 ]]; then
        ok "bash $BASH_VERSION"
    else
        warn "bash $BASH_VERSION (version 4+ recommended)"
    fi

    # Optional but recommended
    if command -v jq &>/dev/null; then
        ok "jq $(jq --version 2>&1 | grep -oE '[0-9]+\.[0-9]+')"
    else
        warn "jq not found (optional, but recommended)"
    fi

    if command -v python3 &>/dev/null; then
        ok "python3 $(python3 --version 2>&1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')"
    else
        warn "python3 not found (required for some features)"
    fi

    # Check for at least one AI provider
    local providers_found=0
    for provider in claude gemini goose ollama; do
        if command -v "$provider" &>/dev/null; then
            ok "$provider CLI available"
            providers_found=$((providers_found + 1))
        fi
    done

    if [[ $providers_found -eq 0 ]]; then
        warn "No AI providers found. Install at least one: claude, gemini, ollama, goose"
    fi

    if [[ ${#missing[@]} -gt 0 ]]; then
        error "Missing required dependencies: ${missing[*]}"
        echo ""
        echo "  Install them first:"
        case "$PLATFORM" in
            macos)
                echo "    brew install ${missing[*]}"
                ;;
            linux|wsl)
                echo "    sudo apt install ${missing[*]}"
                ;;
        esac
        exit 1
    fi
}

# ============================================================================
# Installation
# ============================================================================

install_euxis() {
    step "Installing Euxis"

    if [[ -d "$EUXIS_HOME" ]]; then
        info "Existing installation found at $EUXIS_HOME"
        info "Updating..."

        cd "$EUXIS_HOME"
        if git pull --ff-only origin "$EUXIS_BRANCH" &>/dev/null; then
            ok "Updated to latest version"
        else
            warn "Could not auto-update. Try: cd $EUXIS_HOME && git pull"
        fi
    else
        info "Cloning Euxis to $EUXIS_HOME"

        if git clone --depth 1 --branch "$EUXIS_BRANCH" "$EUXIS_REPO" "$EUXIS_HOME" &>/dev/null; then
            ok "Cloned successfully"
        else
            error "Failed to clone repository"
            exit 1
        fi
    fi

    # Make scripts executable
    chmod +x "$EUXIS_HOME"/euxis-cli/bin/* 2>/dev/null || true
}

# ============================================================================
# Shell Integration
# ============================================================================

setup_shell() {
    step "Setting up shell integration"

    local shell_name
    shell_name="$(basename "${SHELL:-/bin/bash}")"

    local path_export="export PATH=\"\$PATH:$EUXIS_HOME/euxis-cli/bin\""
    local home_export="export EUXIS_HOME=\"$EUXIS_HOME\""

    case "$shell_name" in
        bash)
            local rc="$HOME/.bashrc"
            [[ "$PLATFORM" == "macos" ]] && rc="$HOME/.bash_profile"

            if ! grep -q "EUXIS_HOME" "$rc" 2>/dev/null; then
                echo "" >> "$rc"
                echo "# Euxis" >> "$rc"
                echo "$home_export" >> "$rc"
                echo "$path_export" >> "$rc"
                ok "Added to $rc"
            else
                ok "Already in $rc"
            fi
            ;;
        zsh)
            local rc="$HOME/.zshrc"

            if ! grep -q "EUXIS_HOME" "$rc" 2>/dev/null; then
                echo "" >> "$rc"
                echo "# Euxis" >> "$rc"
                echo "$home_export" >> "$rc"
                echo "$path_export" >> "$rc"
                ok "Added to $rc"
            else
                ok "Already in $rc"
            fi
            ;;
        fish)
            local fish_config="$HOME/.config/fish/config.fish"

            if ! grep -q "EUXIS_HOME" "$fish_config" 2>/dev/null; then
                mkdir -p "$(dirname "$fish_config")"
                echo "" >> "$fish_config"
                echo "# Euxis" >> "$fish_config"
                echo "set -gx EUXIS_HOME $EUXIS_HOME" >> "$fish_config"
                echo "fish_add_path $EUXIS_HOME/euxis-cli/bin" >> "$fish_config"
                ok "Added to $fish_config"
            else
                ok "Already in $fish_config"
            fi
            ;;
        *)
            warn "Unknown shell: $shell_name"
            info "Add manually to your shell config:"
            echo "    $home_export"
            echo "    $path_export"
            ;;
    esac

    # Export for current session
    export EUXIS_HOME
    export PATH="$PATH:$EUXIS_HOME/euxis-cli/bin"
}

# ============================================================================
# Verify Installation
# ============================================================================

verify_installation() {
    step "Verifying installation"

    if [[ -x "$EUXIS_HOME/euxis-cli/bin/euxis" ]]; then
        ok "euxis CLI is executable"
    else
        error "euxis CLI not found or not executable"
        exit 1
    fi

    # Run doctor
    info "Running euxis doctor..."
    echo ""
    "$EUXIS_HOME/euxis-cli/bin/euxis-doctor" || true
}

# ============================================================================
# Success Message
# ============================================================================

print_success() {
    echo ""
    echo -e "  ${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
    echo -e "  ${BOLD}${GREEN}${ICON_OK} Installation Complete!${RESET}"
    echo -e "  ${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
    echo ""
    echo -e "  ${BOLD}Next steps:${RESET}"
    echo ""
    echo -e "    ${DIM}1.${RESET} Restart your shell or run:"
    echo -e "       ${CYAN}source ~/.$(basename "$SHELL")rc${RESET}"
    echo ""
    echo -e "    ${DIM}2.${RESET} Get started:"
    echo -e "       ${CYAN}euxis --help${RESET}"
    echo -e "       ${CYAN}euxis architect \"Design a REST API\"${RESET}"
    echo ""
    echo -e "    ${DIM}3.${RESET} Install shell completions:"
    echo -e "       ${CYAN}euxis doctor --fix${RESET}"
    echo ""
    echo -e "  ${DIM}Docs:${RESET} ${CYAN}https://docs.euxis.co${RESET}"
    echo ""
}

# ============================================================================
# Main
# ============================================================================

main() {
    setup_colors
    setup_icons

    print_logo

    step "Detecting platform"
    detect_platform
    ok "$PLATFORM ($ARCH_LABEL)"

    check_prerequisites
    install_euxis
    setup_shell
    verify_installation
    print_success
}

main "$@"
