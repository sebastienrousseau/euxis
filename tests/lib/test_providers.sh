#!/usr/bin/env bash
# Tests for lib/providers.sh

source "${EUXIS_HOME}/core/lib/providers.sh"

# Test resolve_tiered_provider: S-Tier agents → claude
assert_eq "architect → claude" "claude" "$(resolve_tiered_provider architect)"
assert_eq "orchestrator → claude" "claude" "$(resolve_tiered_provider orchestrator)"
assert_eq "reviewer → claude" "claude" "$(resolve_tiered_provider reviewer)"
assert_eq "product-manager → claude" "claude" "$(resolve_tiered_provider product-manager)"

# Test resolve_tiered_provider: A-Tier agents
assert_eq "deep-researcher → gemini" "gemini" "$(resolve_tiered_provider deep-researcher)"
assert_eq "compliance-officer → gemini" "gemini" "$(resolve_tiered_provider compliance-officer)"
assert_eq "incident-commander → gemini" "gemini" "$(resolve_tiered_provider incident-commander)"

# Test resolve_tiered_provider: B-Tier agents
assert_eq "bug-fixer → goose" "goose" "$(resolve_tiered_provider bug-fixer)"
assert_eq "unit-tester → goose" "goose" "$(resolve_tiered_provider unit-tester)"
assert_eq "legacy-maintainer → goose" "goose" "$(resolve_tiered_provider legacy-maintainer)"
assert_eq "perf-optimizer → qwen" "qwen" "$(resolve_tiered_provider perf-optimizer)"

# Test resolve_tiered_provider: C-Tier agents
assert_eq "butler → ollama" "ollama" "$(resolve_tiered_provider butler)"
assert_eq "librarian → ollama" "ollama" "$(resolve_tiered_provider librarian)"
assert_eq "tech-writer → ollama" "ollama" "$(resolve_tiered_provider tech-writer)"

# Test resolve_tiered_provider: Default fallback
assert_eq "unknown-agent → claude (default)" "claude" "$(resolve_tiered_provider unknown-agent)"

# Test P0 override: always claude regardless of agent
assert_eq "butler P0 → claude" "claude" "$(resolve_tiered_provider butler P0)"
assert_eq "deep-researcher P0 → claude" "claude" "$(resolve_tiered_provider deep-researcher P0)"

# Test resolve_provider_config populates PROVIDER_MODEL
resolve_provider_config "claude"
assert_eq "claude model" "claude-sonnet-4-20250514" "${PROVIDER_MODEL}"

resolve_provider_config "gemini"
assert_eq "gemini model" "gemini-2.0-flash" "${PROVIDER_MODEL}"

resolve_provider_config "ollama"
assert_eq "ollama default model" "llama3.2" "${PROVIDER_MODEL}"
