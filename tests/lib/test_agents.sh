#!/usr/bin/env bash
# Tests for lib/agents.sh

source "${EUXIS_HOME}/bin/lib/agents.sh"

# Test resolve_agent_path: known core agent
result=$(resolve_agent_path "architect")
assert_eq "resolve architect path" "${EUXIS_HOME}/agents/prompts/core/architect.txt" "${result}"

# Test resolve_agent_path: known fleet agent
result=$(resolve_agent_path "bug-fixer")
assert_eq "resolve bug-fixer path" "${EUXIS_HOME}/agents/prompts/fleet/bug-fixer.txt" "${result}"

# Test resolve_agent_path: unknown agent returns empty
result=$(resolve_agent_path "nonexistent-agent-xyz" 2>/dev/null || true)
assert_eq "unknown agent returns empty" "" "${result}"

# Test list_agents: output contains known agents
agents_output=$(list_agents)
assert_contains "list_agents includes architect" "architect" "${agents_output}"
assert_contains "list_agents includes bug-fixer" "bug-fixer" "${agents_output}"
assert_contains "list_agents includes orchestrator" "orchestrator" "${agents_output}"
