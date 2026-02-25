#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com
#
# test_mesh_concurrent.sh — Stress test for concurrent mesh state writes
#
# Verifies that flock-based locking prevents data corruption when
# multiple agents write to state.json simultaneously.
#
# Usage:
#   ./test_mesh_concurrent.sh [num_writers] [writes_per_agent]

set -u

EUXIS_HOME="${EUXIS_HOME:-$HOME/.euxis}"
source "${EUXIS_HOME}/euxis-core/lib/mesh.sh"

NUM_WRITERS="${1:-5}"
WRITES_PER_AGENT="${2:-10}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo ""
echo "═══════════════════════════════════════════════════════"
echo "  MESH CONCURRENT WRITE STRESS TEST"
echo "═══════════════════════════════════════════════════════"
echo ""
echo "  Writers:         $NUM_WRITERS"
echo "  Writes per agent: $WRITES_PER_AGENT"
echo "  Total writes:    $((NUM_WRITERS * WRITES_PER_AGENT))"
echo ""

# Setup
rm -rf "${EUXIS_HOME}/euxis-runtime/mesh"
mkdir -p "${EUXIS_HOME}/euxis-runtime/mesh/inbox"
mesh_init "stress-test-main" "stress-$$"

echo -e "${BLUE}Starting concurrent writers...${NC}"

# Spawn concurrent writers
PIDS=()
for i in $(seq 1 "$NUM_WRITERS"); do
    (
        mesh_register "stress-agent-$i" "stress-testing" "stress-$$"
        for j in $(seq 1 "$WRITES_PER_AGENT"); do
            mesh_log "Agent $i write $j"
            mesh_state_set "shared.counters.agent-$i" "$j"
        done
    ) &
    PIDS+=($!)
done

echo -e "${BLUE}Waiting for $NUM_WRITERS writers to complete...${NC}"

# Wait for all writers
FAILED=0
for pid in "${PIDS[@]}"; do
    if ! wait "$pid"; then
        ((FAILED++))
    fi
done

echo ""

# Verify results
echo -e "${YELLOW}Verifying state integrity...${NC}"

# Check 1: State file is valid JSON
if jq -e . "$MESH_STATE" >/dev/null 2>&1; then
    echo -e "${GREEN}[PASS]${NC} State file is valid JSON"
else
    echo -e "${RED}[FAIL]${NC} State file corrupted (invalid JSON)"
    cat "$MESH_STATE" | head -20
    exit 1
fi

# Check 2: All agents registered
REGISTERED=$(jq '.agents | keys | length' "$MESH_STATE")
EXPECTED=$((NUM_WRITERS + 1))  # +1 for stress-test-main
if [[ "$REGISTERED" -ge "$EXPECTED" ]]; then
    echo -e "${GREEN}[PASS]${NC} All $REGISTERED agents registered (expected >= $EXPECTED)"
else
    echo -e "${RED}[FAIL]${NC} Only $REGISTERED agents registered (expected $EXPECTED)"
fi

# Check 3: Log entries (should be NUM_WRITERS * WRITES_PER_AGENT)
LOG_COUNT=$(jq '.shared.log | length' "$MESH_STATE")
EXPECTED_LOGS=$((NUM_WRITERS * WRITES_PER_AGENT))
if [[ "$LOG_COUNT" -eq "$EXPECTED_LOGS" ]]; then
    echo -e "${GREEN}[PASS]${NC} All $LOG_COUNT log entries recorded (expected $EXPECTED_LOGS)"
else
    echo -e "${YELLOW}[WARN]${NC} $LOG_COUNT log entries recorded (expected $EXPECTED_LOGS)"
    # This could happen with race conditions, but shouldn't with locking
fi

# Check 4: Counter values (each agent should have final value = WRITES_PER_AGENT)
COUNTER_ERRORS=0
for i in $(seq 1 "$NUM_WRITERS"); do
    VAL=$(jq -r ".shared.counters[\"agent-$i\"] // \"missing\"" "$MESH_STATE")
    if [[ "$VAL" != "$WRITES_PER_AGENT" ]]; then
        echo -e "${RED}  Counter agent-$i = $VAL (expected $WRITES_PER_AGENT)${NC}"
        ((COUNTER_ERRORS++))
    fi
done
if [[ $COUNTER_ERRORS -eq 0 ]]; then
    echo -e "${GREEN}[PASS]${NC} All counter values correct"
else
    echo -e "${RED}[FAIL]${NC} $COUNTER_ERRORS counters have wrong values"
fi

# Check 5: No write failures
if [[ $FAILED -eq 0 ]]; then
    echo -e "${GREEN}[PASS]${NC} All writers completed successfully"
else
    echo -e "${RED}[FAIL]${NC} $FAILED writers failed"
fi

# Summary
echo ""
echo "═══════════════════════════════════════════════════════"
STATE_SIZE=$(stat -c%s "$MESH_STATE" 2>/dev/null || stat -f%z "$MESH_STATE")
echo "  State file size: $STATE_SIZE bytes"
echo "  Lock mechanism:  $(command -v flock &>/dev/null && echo 'flock' || echo 'mkdir fallback')"

if [[ $FAILED -eq 0 ]] && [[ $COUNTER_ERRORS -eq 0 ]]; then
    echo -e "  Result: ${GREEN}PASSED${NC}"
    echo "═══════════════════════════════════════════════════════"
    exit 0
else
    echo -e "  Result: ${RED}FAILED${NC}"
    echo "═══════════════════════════════════════════════════════"
    exit 1
fi
