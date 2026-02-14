#!/usr/bin/env bash

# Euxis Version Synchronization Script
# Updates all version references to match registry protocol_version

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

echo "=== Euxis Version Synchronization ==="

# Extract the authoritative version (SQLite-first, JSON fallback)
REGISTRY_VERSION=""
if [ -f "registry.db" ]; then
    REGISTRY_VERSION=$(sqlite3 -init /dev/null "registry.db" "SELECT value FROM registry_metadata WHERE key='protocol_version'" 2>/dev/null || echo "")
fi
if [ -z "$REGISTRY_VERSION" ] && [ -f "registry.json" ]; then
    REGISTRY_VERSION=$(python3 -c "import json; print(json.load(open('registry.json'))['protocol_version'])" 2>/dev/null || echo "")
fi

if [ -z "$REGISTRY_VERSION" ]; then
    echo "❌ FATAL: Could not extract protocol_version from registry"
    exit 1
fi

echo "✅ Synchronizing all versions to: $REGISTRY_VERSION"

updates_made=0

# Update VERSION file
if [ -f "VERSION" ]; then
    echo "$REGISTRY_VERSION" > VERSION
    echo "✅ Updated VERSION file"
    updates_made=$((updates_made + 1))
fi

# Update agent prompt versions
echo ""
echo "Updating agent prompt versions..."

prompt_files=(prompts/core/*.txt prompts/fleet/*.txt prompts/protocols/*.txt)
for prompt_file in "${prompt_files[@]}"; do
    if [ -f "$prompt_file" ]; then
        # Update version in YAML frontmatter using sed
        if grep -q "^version:" "$prompt_file"; then
            sed -i.bak "s/^version:.*/version: \"$REGISTRY_VERSION\"/" "$prompt_file"
            rm -f "$prompt_file.bak"
            echo "✅ Updated $(basename "$prompt_file")"
            updates_made=$((updates_made + 1))
        fi
    fi
done

# Update documentation version references
echo ""
echo "Updating documentation version references..."

doc_files=("README.md" "docs/user-guide.md" "docs/fleet-guide.md")
for doc_file in "${doc_files[@]}"; do
    if [ -f "$doc_file" ]; then
        # Look for version patterns and update them
        if grep -q "v[0-9]\+\.[0-9]\+\.[0-9]\+" "$doc_file"; then
            sed -i.bak "s/v[0-9]\+\.[0-9]\+\.[0-9]\+/v$REGISTRY_VERSION/g" "$doc_file"
            rm -f "$doc_file.bak"
            echo "✅ Updated version references in $(basename "$doc_file")"
            updates_made=$((updates_made + 1))
        fi
    fi
done

echo ""
echo "=== Synchronization Complete ==="
echo "✅ Made $updates_made version updates"
echo "✅ All versions now synchronized to: $REGISTRY_VERSION"
echo ""
echo "Next steps:"
echo "  1. Review the changes: git diff"
echo "  2. Verify consistency: scripts/check-version-consistency.sh"
echo "  3. Test workflows with updated versions"
echo "  4. Commit changes: git add -A && git commit -m 'sync: update all versions to $REGISTRY_VERSION'"