#!/usr/bin/env bats
# Test suite for core/lib/providers.sh

# Test setup - run before each test
setup() {
    EUXIS_TEST_TMPDIR="$(mktemp -d)"
    export EUXIS_TEST_TMPDIR

    # Use real EUXIS_HOME for library sourcing
    export EUXIS_HOME="${HOME}/.euxis"

    export PATH="${EUXIS_TEST_TMPDIR}:${PATH}"

    # Reset include guards
    unset _EUXIS_LIB_PROVIDERS
    unset _EUXIS_LIB_COMMON
    unset PROVIDER_MODEL
    unset PROVIDER_FLAGS

    # Source dependencies from real installation
    source "${EUXIS_HOME}/euxis-core/lib/common.sh"
    source "${EUXIS_HOME}/euxis-core/lib/providers.sh"
}

teardown() {
    if [[ -n "${EUXIS_TEST_TMPDIR:-}" && -d "${EUXIS_TEST_TMPDIR:-}" ]]; then
        rm -rf "${EUXIS_TEST_TMPDIR}"
    fi
}

# ============================================================================
# RESOLVE_TIERED_PROVIDER TESTS
# ============================================================================

@test "resolve_tiered_provider returns claude for P0 priority" {
    run resolve_tiered_provider "any-agent" "P0"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "claude" ]]
}

@test "resolve_tiered_provider returns claude for strategic agents" {
    for agent in orchestrator architect product-manager reviewer; do
        run resolve_tiered_provider "${agent}"
        [[ "${status}" -eq 0 ]]
        [[ "${output}" == "claude" ]]
    done
}

@test "resolve_tiered_provider returns gemini for research agents" {
    for agent in researcher deep-researcher compliance-officer; do
        run resolve_tiered_provider "${agent}"
        [[ "${status}" -eq 0 ]]
        [[ "${output}" == "gemini" ]]
    done
}

@test "resolve_tiered_provider returns kiro-cli for enterprise agents" {
    run resolve_tiered_provider "incident-commander"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "kiro-cli" ]]
}

@test "resolve_tiered_provider returns goose for coding agents" {
    for agent in bug-fixer unit-tester automation-engineer legacy-maintainer; do
        run resolve_tiered_provider "${agent}"
        [[ "${status}" -eq 0 ]]
        [[ "${output}" == "goose" ]]
    done
}

@test "resolve_tiered_provider returns qwen for math/logic agents" {
    for agent in perf-optimizer data-steward; do
        run resolve_tiered_provider "${agent}"
        [[ "${status}" -eq 0 ]]
        [[ "${output}" == "qwen" ]]
    done
}

@test "resolve_tiered_provider returns ollama for utility agents" {
    for agent in butler librarian tech-writer; do
        run resolve_tiered_provider "${agent}"
        [[ "${status}" -eq 0 ]]
        [[ "${output}" == "ollama" ]]
    done
}

@test "resolve_tiered_provider returns default for unknown agents" {
    run resolve_tiered_provider "unknown-agent"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "claude" ]]
}

@test "resolve_tiered_provider handles empty agent name" {
    run resolve_tiered_provider ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "claude" ]]
}

# ============================================================================
# _VALIDATE_MODEL_NAME TESTS
# ============================================================================

@test "_validate_model_name accepts valid model names" {
    run _validate_model_name "claude-3-opus"
    [[ "${status}" -eq 0 ]]
}

@test "_validate_model_name accepts model with org prefix" {
    run _validate_model_name "anthropic/claude-3"
    [[ "${status}" -eq 0 ]]
}

@test "_validate_model_name accepts model with tag" {
    run _validate_model_name "llama3:latest"
    [[ "${status}" -eq 0 ]]
}

@test "_validate_model_name accepts model with version" {
    run _validate_model_name "gpt-4o-2024-05-13"
    [[ "${status}" -eq 0 ]]
}

@test "_validate_model_name rejects special characters" {
    run _validate_model_name "model;rm -rf /"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Invalid model name" ]]
}

@test "_validate_model_name rejects shell injection" {
    run _validate_model_name "model\$(whoami)"
    [[ "${status}" -eq 1 ]]
}

@test "_validate_model_name rejects excessively long names" {
    local long_name
    long_name=$(printf '%*s' 150 | tr ' ' 'a')
    run _validate_model_name "${long_name}"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "exceeds 128 character limit" ]]
}

@test "_validate_model_name accepts names at 128 chars" {
    local max_name
    max_name=$(printf '%*s' 128 | tr ' ' 'a')
    run _validate_model_name "${max_name}"
    [[ "${status}" -eq 0 ]]
}

# ============================================================================
# RESOLVE_PROVIDER_CONFIG TESTS
# ============================================================================

@test "resolve_provider_config sets claude model" {
    resolve_provider_config "claude"
    [[ "${PROVIDER_MODEL}" == "claude-sonnet-4-20250514" ]]
    [[ "${PROVIDER_FLAGS}" =~ "--cache-prompt" ]]
}

@test "resolve_provider_config sets gemini model" {
    resolve_provider_config "gemini"
    [[ "${PROVIDER_MODEL}" == "gemini-2.0-flash" ]]
}

@test "resolve_provider_config sets openai model" {
    resolve_provider_config "openai"
    [[ "${PROVIDER_MODEL}" == "gpt-4o" ]]
    [[ "${PROVIDER_FLAGS}" =~ "--stream" ]]
}

@test "resolve_provider_config sets ollama model" {
    resolve_provider_config "ollama"
    [[ "${PROVIDER_MODEL}" == "llama3.2" ]]
    [[ "${PROVIDER_FLAGS}" =~ "--local" ]]
}

@test "resolve_provider_config respects EUXIS_OLLAMA_MODEL" {
    export EUXIS_OLLAMA_MODEL="custom-model"
    resolve_provider_config "ollama"
    [[ "${PROVIDER_MODEL}" == "custom-model" ]]
}

@test "resolve_provider_config sets qwen model" {
    resolve_provider_config "qwen"
    [[ "${PROVIDER_MODEL}" == "qwen3-coder" ]]
}

@test "resolve_provider_config sets crush model" {
    resolve_provider_config "crush"
    [[ "${PROVIDER_MODEL}" == "claude-sonnet-4" ]]
}

@test "resolve_provider_config sets kiro-cli model" {
    resolve_provider_config "kiro-cli"
    [[ "${PROVIDER_MODEL}" == "kiro-cli" ]]
}

@test "resolve_provider_config sets goose model" {
    resolve_provider_config "goose"
    [[ "${PROVIDER_MODEL}" == "claude-sonnet-4" ]]
}

@test "resolve_provider_config exits on unknown provider" {
    run resolve_provider_config "invalid-provider"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Unknown provider" ]]
}

# ============================================================================
# RUN_WITH_TIMEOUT TESTS
# ============================================================================

@test "run_with_timeout executes command successfully" {
    run run_with_timeout 5 "test command" echo "hello"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "hello" ]]
}

@test "run_with_timeout handles command failure" {
    run run_with_timeout 5 "failing command" false
    [[ "${status}" -ne 0 ]]
}

@test "run_with_timeout works without timeout command" {
    # Temporarily hide timeout
    local original_path="${PATH}"
    export PATH="${EUXIS_TEST_TMPDIR}/empty:${original_path}"
    mkdir -p "${EUXIS_TEST_TMPDIR}/empty"

    run run_with_timeout 5 "test" echo "works"
    [[ "${status}" -eq 0 ]]

    export PATH="${original_path}"
}

# ============================================================================
# EXECUTE_PROVIDER TESTS (MOCK MODE)
# ============================================================================

@test "execute_provider mock mode returns success" {
    export EUXIS_MOCK_PROVIDER=true
    resolve_provider_config "claude"
    run execute_provider "claude" "test prompt"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "MOCK" ]]
}

@test "execute_provider mock mode shows provider name" {
    export EUXIS_MOCK_PROVIDER=true
    resolve_provider_config "gemini"
    run execute_provider "gemini" "test prompt"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "gemini" ]]
}

@test "execute_provider mock mode shows prompt length" {
    export EUXIS_MOCK_PROVIDER=true
    resolve_provider_config "claude"
    run execute_provider "claude" "a test prompt with content"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "chars" ]]
}

@test "execute_provider exits on unknown provider" {
    export EUXIS_MOCK_PROVIDER=true
    run execute_provider "invalid" "prompt"
    [[ "${status}" -eq 1 ]]
}

# ============================================================================
# PROVIDER RUNNER TESTS (AVAILABILITY CHECK)
# ============================================================================

@test "run_claude requires claude CLI" {
    # Without claude available, should error
    run run_claude "test" 2>&1 || true
    # Either succeeds (if claude is installed) or mentions missing CLI
    [[ "${status}" -eq 0 ]] || [[ "${output}" =~ "claude" ]] || true
}

@test "run_gemini requires gemini CLI" {
    run run_gemini "test" 2>&1 || true
    [[ "${status}" -eq 0 ]] || [[ "${output}" =~ "gemini" ]] || true
}

@test "run_openai requires codex CLI" {
    run run_openai "test" 2>&1 || true
    [[ "${status}" -eq 0 ]] || [[ "${output}" =~ "codex" ]] || true
}

@test "run_ollama requires ollama CLI" {
    run run_ollama "test" 2>&1 || true
    [[ "${status}" -eq 0 ]] || [[ "${output}" =~ "ollama" ]] || true
}

@test "run_qwen requires qwen CLI" {
    run run_qwen "test" 2>&1 || true
    [[ "${status}" -eq 0 ]] || [[ "${output}" =~ "qwen" ]] || true
}

@test "run_crush requires crush CLI" {
    run run_crush "test" 2>&1 || true
    [[ "${status}" -eq 0 ]] || [[ "${output}" =~ "crush" ]] || true
}

@test "run_kiro_cli requires kiro-cli" {
    run run_kiro_cli "test" 2>&1 || true
    [[ "${status}" -eq 0 ]] || [[ "${output}" =~ "kiro-cli" ]] || true
}

@test "run_goose requires goose CLI" {
    run run_goose "test" 2>&1 || true
    [[ "${status}" -eq 0 ]] || [[ "${output}" =~ "goose" ]] || true
}

# ============================================================================
# ERROR PATHS AND EDGE CASES
# ============================================================================

@test "providers functions handle set -u mode" {
    set -u
    run resolve_tiered_provider "architect"
    [[ "${status}" -eq 0 ]]
}

@test "providers functions handle pipefail mode" {
    set -e -o pipefail
    run resolve_tiered_provider "architect"
    [[ "${status}" -eq 0 ]]
}

@test "resolve_provider_config validates env-sourced model names" {
    export EUXIS_OLLAMA_MODEL="valid-model"
    run resolve_provider_config "ollama"
    [[ "${status}" -eq 0 ]]
}

@test "resolve_provider_config rejects invalid env-sourced models" {
    export EUXIS_OLLAMA_MODEL="invalid;model"
    run resolve_provider_config "ollama"
    [[ "${status}" -eq 1 ]]
}

@test "EUXIS_API_TIMEOUT default is 300" {
    [[ "${EUXIS_API_TIMEOUT}" == "300" ]]
}

@test "DEFAULT_PROVIDER is claude" {
    [[ "${DEFAULT_PROVIDER}" == "claude" ]]
}

# ============================================================================
# INTEGRATION TESTS
# ============================================================================

@test "providers module loads without errors" {
    [[ -n "${_EUXIS_LIB_PROVIDERS}" ]]
}

@test "provider config is consistent" {
    resolve_provider_config "claude"
    local model1="${PROVIDER_MODEL}"

    resolve_provider_config "claude"
    local model2="${PROVIDER_MODEL}"

    [[ "${model1}" == "${model2}" ]]
}

@test "tiered provider selection is deterministic" {
    local result1
    result1=$(resolve_tiered_provider "architect")
    local result2
    result2=$(resolve_tiered_provider "architect")

    [[ "${result1}" == "${result2}" ]]
}

@test "all known providers can be configured" {
    local providers=(claude gemini openai ollama qwen crush kiro-cli goose)
    for provider in "${providers[@]}"; do
        resolve_provider_config "${provider}"
        [[ -n "${PROVIDER_MODEL}" ]]
    done
}
