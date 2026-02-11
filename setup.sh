#!/usr/bin/env bash
# (c) 2026 Euxis Fleet. All rights reserved.
#
# setup.sh - Install and configure the Euxis Fleet system
#
# USAGE:
#     setup.sh [OPTIONS]
#     setup.sh --help
#
# DESCRIPTION:
#     Installs the Euxis Fleet by creating symlinks in ~/bin for all
#     executable scripts and performing initial system verification.
#     This is the primary installation script for new Euxis deployments.
#
# OPTIONS:
#     -h, --help          Show this help message and exit
#     -v, --verbose       Enable verbose output during installation
#     -q, --quiet         Suppress non-error output
#     --dry-run           Show what would be installed without executing
#     --force             Force reinstallation over existing symlinks
#
# ENVIRONMENT VARIABLES:
#     HOME                User home directory (required)
#     EUXIS_HOME          Euxis installation directory (default: ~/.euxis)
#     PATH                Will be modified to include ~/bin
#
# DEPENDENCIES:
#     - bash 4.0+
#     - ln (for creating symlinks)
#     - euxis-health (created during installation)
#
# EXAMPLES:
#     # Standard installation
#     ./setup.sh
#
#     # Verbose installation
#     ./setup.sh --verbose
#
#     # Preview installation without executing
#     ./setup.sh --dry-run
#
# EXIT CODES:
#     0    Installation successful
#     1    Installation failed
#     2    Usage error
#     3    Missing dependencies

set -euo pipefail

# Script metadata
readonly SCRIPT_NAME="$(basename "$0")"
readonly SCRIPT_VERSION="0.0.7"

# Configuration
VERBOSE=false
QUIET=false
DRY_RUN=false
FORCE=false

# Usage function
usage() {
    cat <<EOF
${SCRIPT_NAME} - Install and configure the Euxis Fleet system

USAGE:
    ${SCRIPT_NAME} [OPTIONS]
    ${SCRIPT_NAME} --help

DESCRIPTION:
    Installs the Euxis Fleet by creating symlinks in ~/bin for all
    executable scripts and performing initial system verification.

OPTIONS:
    -h, --help          Show this help message and exit
    -v, --verbose       Enable verbose output during installation
    -q, --quiet         Suppress non-error output
    --dry-run           Show what would be installed without executing
    --force             Force reinstallation over existing symlinks

EXAMPLES:
    # Standard installation
    ${SCRIPT_NAME}

    # Verbose installation
    ${SCRIPT_NAME} --verbose

    # Preview installation
    ${SCRIPT_NAME} --dry-run

EXIT CODES:
    0    Installation successful
    1    Installation failed
    2    Usage error
    3    Missing dependencies
EOF
}

# Argument parsing
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                usage
                exit 0
                ;;
            -v|--verbose)
                VERBOSE=true
                shift
                ;;
            -q|--quiet)
                QUIET=true
                shift
                ;;
            --dry-run)
                DRY_RUN=true
                shift
                ;;
            --force)
                FORCE=true
                shift
                ;;
            -*)
                echo "Error: Unknown option '$1'" >&2
                echo "Try '${SCRIPT_NAME} --help' for more information." >&2
                exit 2
                ;;
            *)
                echo "Error: Unexpected argument '$1'" >&2
                echo "Try '${SCRIPT_NAME} --help' for more information." >&2
                exit 2
                ;;
        esac
    done
}

# Logging functions
log_info() {
    [[ "$QUIET" == "true" ]] || echo "$@"
}

log_verbose() {
    [[ "$VERBOSE" == "true" ]] && printf '  %s\n' "$*" || true
}

log_error() {
    printf 'Error: %s\n' "$*" >&2
}

# Main installation function
main() {
    parse_args "$@"

    log_info "Installing Euxis Fleet..."

    # Create bin directory
    if [[ "$DRY_RUN" == "true" ]]; then
        log_info "[DRY RUN] Would create directory: ${HOME}/bin"
    else
        mkdir -p "${HOME}/bin"
        log_verbose "Created directory: ${HOME}/bin"
    fi

    # Link tools to ~/bin
    log_info "Linking tools to ~/bin..."
    local linked_count=0

    for script in "${HOME}/.euxis/bin/"*; do
        if [[ -x "${script}" ]]; then
            local name="$(basename "${script}" .sh)"

            # Skip if .sh version exists alongside non-.sh version
            if [[ "${script}" == *.sh && -x "${HOME}/.euxis/bin/${name}" ]]; then
                log_verbose "Skipping ${name}.sh (non-.sh version exists)"
                continue
            fi

            # Check if link already exists
            if [[ -L "${HOME}/bin/${name}" ]] && [[ "$FORCE" != "true" ]]; then
                log_verbose "Skipping ${name} (already linked)"
                continue
            fi

            if [[ "$DRY_RUN" == "true" ]]; then
                log_info "[DRY RUN] Would link: ${name}"
            else
                ln -sf "${script}" "${HOME}/bin/${name}"
                log_info "  - ${name}"
                ((++linked_count))
            fi
        fi
    done

    log_verbose "Linked ${linked_count} scripts"

    # Verify installation
    log_info "Verifying protocol headers..."
    if [[ "$DRY_RUN" == "true" ]]; then
        log_info "[DRY RUN] Would run health check"
    else
        if "${HOME}/bin/euxis-health" --silent 2>/dev/null; then
            log_verbose "Health check passed"
        else
            log_info "Health check flagged issues. Run 'euxis-health' for details."
        fi
    fi

    # Installation complete
    log_info ""
    log_info "Euxis is ready."
    log_info "   Try:  euxis butler \"Introduce yourself briefly.\""
    log_info "   Or:   euxis verify"
    log_info ""
    log_info "Restart your terminal to refresh PATH."
}

# Run main function with all arguments
main "$@"
