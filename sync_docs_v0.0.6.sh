#!/bin/bash
# Documentation Sync Script for v0.0.6 Release
# (c) 2026 Euxis Fleet. All rights reserved.

set -e

PROTOCOL_VERSION="0.0.6"
TOTAL_AGENTS=31
CORE_AGENTS=7
DEFAULT_AGENTS=17
ONDEMAND_AGENTS=7

echo "🔄 Syncing documentation for v${PROTOCOL_VERSION}..."

# Verify all key files have correct version headers
echo "✅ Verifying version consistency..."

# Check registry.json protocol_version
REGISTRY_VERSION=$(grep '"protocol_version"' registry.json | sed 's/.*"\([^"]*\)".*/\1/')
if [ "$REGISTRY_VERSION" != "$PROTOCOL_VERSION" ]; then
    echo "❌ ERROR: registry.json protocol_version is $REGISTRY_VERSION, expected $PROTOCOL_VERSION"
    exit 1
fi

# Check capabilities.json version
CAPABILITIES_VERSION=$(grep '"version"' capabilities.json | head -1 | sed 's/.*"\([^"]*\)".*/\1/')
if [ "$CAPABILITIES_VERSION" != "$PROTOCOL_VERSION" ]; then
    echo "❌ ERROR: capabilities.json version is $CAPABILITIES_VERSION, expected $PROTOCOL_VERSION"
    exit 1
fi

# Verify USER_GUIDE.md fleet counts are accurate
echo "✅ Verifying fleet counts in USER_GUIDE.md..."

# Update the fleet summary line to be accurate
sed -i "s/| \*\*Fleet\*\* | [0-9]* agents: [0-9]* core + [0-9]* default + [0-9]* on-demand |/| **Fleet** | $TOTAL_AGENTS agents: $CORE_AGENTS core + $DEFAULT_AGENTS default + $ONDEMAND_AGENTS on-demand |/" USER_GUIDE.md

# Verify and update the on-demand fleet section count
sed -i "s/### On-Demand ([0-9]*)/### On-Demand ($ONDEMAND_AGENTS)/" USER_GUIDE.md

# Verify version number in USER_GUIDE.md header
sed -i "s/^Version [0-9]\+\.[0-9]\+\.[0-9]\+/Version $PROTOCOL_VERSION/" USER_GUIDE.md

echo "✅ Documentation sync completed successfully"
echo "📊 Fleet: $TOTAL_AGENTS agents ($CORE_AGENTS core + $DEFAULT_AGENTS default + $ONDEMAND_AGENTS on-demand)"
echo "🏷️  Protocol version: $PROTOCOL_VERSION"
echo ""
echo "🔍 Verification commands:"
echo "  euxis-lint      # Registry integrity check"
echo "  euxis-health    # Fleet health verification"
echo "  euxis-certify   # Full certification"