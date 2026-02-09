#!/usr/bin/env bash
# (c) 2026 Euxis Fleet. All rights reserved.
#
# Euxis Validation Library - Security-focused validation functions
# Part of the Euxis Multi-Provider AI Agent Framework
#
[[ -n "${_EUXIS_LIB_VALIDATION:-}" ]] && return; _EUXIS_LIB_VALIDATION=1

set -euo pipefail

# ============================================================================
# Validation State
# ============================================================================

declare -a VALIDATION_ERRORS=()
declare -a VALIDATION_WARNINGS=()

# ============================================================================
# Core Validation Functions
# ============================================================================

validation_error() {
    local message="$1"
    VALIDATION_ERRORS+=("ERROR: $message")
    echo "  ❌ $message" >&2
}

validation_warning() {
    local message="$1"
    VALIDATION_WARNINGS+=("WARNING: $message")
    echo "  ⚠️  $message" >&2
}

validation_pass() {
    local message="$1"
    echo "  ✅ $message"
}

# ============================================================================
# Security Validation Functions
# ============================================================================

# Validate agent name for security (path traversal prevention)
validate_agent_name() {
    local agent="$1"
    local context="${2:-agent name}"

    # Check for empty input
    if [[ -z "$agent" ]]; then
        validation_error "Empty $context not allowed"
        return 1
    fi

    # Check for path traversal attempts
    if [[ "$agent" == *".."* ]]; then
        validation_error "Invalid $context: '$agent' (path traversal detected)"
        return 1
    fi

    # Check for absolute paths
    if [[ "$agent" == /* ]]; then
        validation_error "Invalid $context: '$agent' (absolute path not allowed)"
        return 1
    fi

    # Check for directory separators
    if [[ "$agent" == *"/"* ]]; then
        validation_error "Invalid $context: '$agent' (directory separators not allowed)"
        return 1
    fi

    # Check for partial files (underscore prefix)
    if [[ "$agent" == _* ]]; then
        validation_error "Invalid $context: '$agent' (partial files not allowed)"
        return 1
    fi

    # Check for valid characters only
    if [[ ! "$agent" =~ ^[a-zA-Z0-9_-]+$ ]]; then
        validation_error "Invalid $context: '$agent' (only alphanumeric, hyphens, and underscores allowed)"
        return 1
    fi

    validation_pass "$context '$agent' is valid"
    return 0
}

# Validate task input for security
validate_task_input() {
    local task="$1"

    # Check for empty input
    if [[ -z "$task" ]]; then
        validation_error "Empty task not allowed"
        return 1
    fi

    # Task length sanity check (prevent extremely long inputs)
    if [[ ${#task} -gt 10000 ]]; then
        validation_error "Task too long (${#task} characters, max 10000)"
        return 1
    fi

    validation_pass "Task input is valid"
    return 0
}

# Validate file exists and is executable
validate_file_executable() {
    local file_path="$1"
    local description="${2:-file}"

    if [[ ! -f "$file_path" ]]; then
        validation_error "$description does not exist: $file_path"
        return 1
    fi

    if [[ ! -x "$file_path" ]]; then
        validation_error "$description is not executable: $file_path"
        return 1
    fi

    validation_pass "$description is accessible: $file_path"
    return 0
}

# Check for required system dependencies
validate_system_dependencies() {
    local missing_deps=()

    # Check for critical dependencies
    if ! command -v jq &>/dev/null; then
        missing_deps+=("jq")
    fi

    if ! command -v gh &>/dev/null; then
        missing_deps+=("gh")
    fi

    if ! command -v curl &>/dev/null; then
        missing_deps+=("curl")
    fi

    if [[ ${#missing_deps[@]} -gt 0 ]]; then
        validation_warning "Missing optional dependencies: ${missing_deps[*]} (some features may be degraded)"
        return 0  # Warning, not error
    fi

    validation_pass "All system dependencies available"
    return 0
}

# Validate Euxis directory structure
validate_euxis_structure() {
    local euxis_home="${EUXIS_HOME:-$HOME/.euxis}"

    # Check critical directories exist
    local required_dirs=(
        "$euxis_home"
        "$euxis_home/bin"
        "$euxis_home/bin/lib"
        "$euxis_home/prompts"
        "$euxis_home/data"
    )

    for dir in "${required_dirs[@]}"; do
        if [[ ! -d "$dir" ]]; then
            validation_error "Required directory missing: $dir"
            return 1
        fi
    done

    # Check registry file exists
    if [[ ! -f "$euxis_home/registry.json" ]]; then
        validation_error "Registry file missing: $euxis_home/registry.json"
        return 1
    fi

    # Check registry is valid JSON
    if ! jq empty "$euxis_home/registry.json" &>/dev/null; then
        validation_error "Registry file is not valid JSON: $euxis_home/registry.json"
        return 1
    fi

    validation_pass "Euxis directory structure is valid"
    return 0
}

# ============================================================================
# Comprehensive Validation Functions
# ============================================================================

# Minimal validation required for operation
validate_minimal() {
    local validation_failed=false

    echo "Running minimal validation..."

    # Reset validation state
    VALIDATION_ERRORS=()
    VALIDATION_WARNINGS=()

    # Core structure validation
    if ! validate_euxis_structure; then
        validation_failed=true
    fi

    # System dependencies check
    validate_system_dependencies  # Always succeeds (warnings only)

    # Report results
    if [[ ${#VALIDATION_WARNINGS[@]} -gt 0 ]]; then
        echo ""
        echo "Validation warnings (${#VALIDATION_WARNINGS[@]}):"
        for warning in "${VALIDATION_WARNINGS[@]}"; do
            echo "  $warning"
        done
    fi

    if [[ ${#VALIDATION_ERRORS[@]} -gt 0 ]]; then
        echo ""
        echo "Validation errors (${#VALIDATION_ERRORS[@]}):"
        for error in "${VALIDATION_ERRORS[@]}"; do
            echo "  $error"
        done
        return 1
    fi

    echo "Minimal validation passed."
    return 0
}

# Full validation (more comprehensive checks)
validate_full() {
    echo "Running full validation..."

    # Reset validation state
    VALIDATION_ERRORS=()
    VALIDATION_WARNINGS=()

    # Run minimal validation first
    if ! validate_minimal; then
        return 1
    fi

    # Additional full validation checks could go here
    # (placeholder for future enhancements)

    echo "Full validation passed."
    return 0
}

# Alias for backward compatibility with euxis-certify
validate_comprehensive() {
    validate_full "$@"
}