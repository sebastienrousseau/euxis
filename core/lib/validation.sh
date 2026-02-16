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

# Validate task input for security (including prompt injection detection)
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

    # Prompt injection detection: block common goal-hijacking patterns
    local task_lower
    task_lower=$(printf '%s' "$task" | tr '[:upper:]' '[:lower:]')
    local -a injection_patterns=(
        "ignore previous"
        "ignore all previous"
        "ignore your instructions"
        "disregard previous"
        "disregard your"
        "forget your instructions"
        "override your"
        "new instructions:"
        "system prompt:"
        "you are now"
        "pretend you are"
        "act as if"
        "bypass approval"
        "skip verification"
        "skip all gates"
        "disable security"
        "dangerously"
    )
    for pattern in "${injection_patterns[@]}"; do
        if [[ "$task_lower" == *"$pattern"* ]]; then
            validation_error "Task input rejected: potential prompt injection detected (pattern: '$pattern')"
            return 1
        fi
    done

    # Block shell metacharacters in task input (prevent command injection via task)
    if [[ "$task" =~ [\`\$\(] && "$task" =~ [\)\'] ]]; then
        validation_warning "Task input contains shell metacharacters — treating as literal text"
    fi

    validation_pass "Task input is valid"
    return 0
}

# Sanitize task input for safe embedding in prompts
# Strips dangerous sequences while preserving legitimate content
sanitize_task_input() {
    local task="$1"
    # Remove null bytes and control characters (except newline/tab)
    task=$(printf '%s' "$task" | tr -d '\000-\010\013\014\016-\037')
    # Escape backticks to prevent shell expansion in prompts
    task="${task//\`/\\\\}"
    printf '%s' "$task"
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
        "$euxis_home/cli/bin"
        "$euxis_home/core/lib"
        "$euxis_home/agents/prompts"
        "$euxis_home/data"
    )

    for dir in "${required_dirs[@]}"; do
        if [[ ! -d "$dir" ]]; then
            validation_error "Required directory missing: $dir"
            return 1
        fi
    done

    # Check registry exists (SQLite or JSON)
    if [[ -f "$euxis_home/agents/registry.db" ]]; then
        # Validate SQLite database
        local agent_count
        # Use Python for SQLite access (more portable than sqlite3 CLI)
        agent_count=$(python3 -c "import sqlite3; conn = sqlite3.connect('$euxis_home/agents/registry.db'); print(conn.execute('SELECT COUNT(*) FROM agents').fetchone()[0])" 2>/dev/null)
        if [[ -n "$agent_count" && "$agent_count" -gt 0 ]]; then
            validation_pass "Registry database valid ($agent_count agents)"
        else
            validation_error "Registry database empty or corrupt: $euxis_home/agents/registry.db"
            return 1
        fi
    elif [[ -f "$euxis_home/agents/registry.json" ]]; then
        # Fall back to JSON validation
        if ! jq empty "$euxis_home/agents/registry.json" &>/dev/null; then
            validation_error "Registry file is not valid JSON: $euxis_home/agents/registry.json"
            return 1
        fi
    else
        validation_error "Registry not found (neither agents/registry.db nor agents/registry.json)"
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
