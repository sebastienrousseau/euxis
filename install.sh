#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com
#
# install.sh — Global CLI installation for Euxis
#
# This script makes Euxis CLI tools available system-wide by:
#   1. Setting EUXIS_HOME in shell profile
#   2. Adding euxis-bin to PATH
#   3. Creating symlinks in ~/.local/bin (optional)
#
# Usage:
#   ./install.sh [--symlinks]
#
# Options:
#   --symlinks    Create symlinks in ~/.local/bin for global access

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo ""
echo -e "${BLUE}╔═══════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║           Euxis v0.0.3 Installation                   ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════╝${NC}"
echo ""

# Detect installation directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EUXIS_HOME="$SCRIPT_DIR"

echo -e "${YELLOW}Installation directory:${NC} $EUXIS_HOME"

# Detect shell
SHELL_NAME=$(basename "$SHELL")
case "$SHELL_NAME" in
    bash)
        PROFILE_FILE="$HOME/.bashrc"
        ;;
    zsh)
        PROFILE_FILE="$HOME/.zshrc"
        ;;
    *)
        PROFILE_FILE="$HOME/.profile"
        ;;
esac

echo -e "${YELLOW}Shell profile:${NC} $PROFILE_FILE"

# Check if already installed
if grep -q "EUXIS_HOME" "$PROFILE_FILE" 2>/dev/null; then
    echo -e "${GREEN}✓${NC} Euxis already configured in $PROFILE_FILE"
else
    echo -e "${YELLOW}Adding Euxis to $PROFILE_FILE...${NC}"

    cat >> "$PROFILE_FILE" << EOF

# Euxis CLI
export EUXIS_HOME="$EUXIS_HOME"
export PATH="\$EUXIS_HOME/euxis-bin:\$PATH"
EOF

    echo -e "${GREEN}✓${NC} Added EUXIS_HOME and PATH to $PROFILE_FILE"
fi

# Create symlinks if requested
if [[ "${1:-}" == "--symlinks" ]]; then
    SYMLINK_DIR="$HOME/.local/bin"
    mkdir -p "$SYMLINK_DIR"

    echo -e "${YELLOW}Creating symlinks in $SYMLINK_DIR...${NC}"

    CLI_COMMANDS=(
        "euxis"
        "euxis-dispatch"
        "euxis-loop"
        "euxis-playbook"
        "euxis-agent-bootstrap"
        "euxis-bench"
    )

    for cmd in "${CLI_COMMANDS[@]}"; do
        src="$EUXIS_HOME/euxis-bin/$cmd"
        dst="$SYMLINK_DIR/$cmd"
        if [[ -f "$src" ]]; then
            ln -sf "$src" "$dst"
            echo -e "  ${GREEN}✓${NC} $cmd"
        fi
    done

    # Ensure ~/.local/bin is in PATH
    if ! echo "$PATH" | grep -q "$SYMLINK_DIR"; then
        echo -e "${YELLOW}Note:${NC} Add $SYMLINK_DIR to your PATH if not already present"
    fi
fi

# Verify installation
echo ""
echo -e "${BLUE}Verifying installation...${NC}"

if [[ -x "$EUXIS_HOME/euxis-bin/euxis-dispatch" ]]; then
    echo -e "  ${GREEN}✓${NC} euxis-dispatch found"
else
    echo -e "  ${RED}✗${NC} euxis-dispatch not found"
fi

if [[ -f "$EUXIS_HOME/euxis-bin/lib/mesh.sh" ]]; then
    echo -e "  ${GREEN}✓${NC} mesh.sh library found"
else
    echo -e "  ${RED}✗${NC} mesh.sh library not found"
fi

if [[ -f "$EUXIS_HOME/euxis-bin/lib/resources.sh" ]]; then
    echo -e "  ${GREEN}✓${NC} resources.sh library found"
else
    echo -e "  ${RED}✗${NC} resources.sh library not found"
fi

if [[ -f "$EUXIS_HOME/euxis-data/agents/registry.json" ]]; then
    AGENT_COUNT=$(jq '.agents | length' "$EUXIS_HOME/euxis-data/agents/registry.json")
    echo -e "  ${GREEN}✓${NC} Agent registry: $AGENT_COUNT agents"
else
    echo -e "  ${RED}✗${NC} Agent registry not found"
fi

echo ""
echo -e "${GREEN}Installation complete!${NC}"
echo ""
echo "To activate, run:"
echo -e "  ${YELLOW}source $PROFILE_FILE${NC}"
echo ""
echo "Or start a new terminal session."
echo ""
echo "Verify with:"
echo -e "  ${YELLOW}euxis-dispatch --resources${NC}"
echo ""
