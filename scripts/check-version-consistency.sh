#!/bin/bash

# Euxis Version Consistency Checker
# Validates that all version references match the protocol_version in registry.json

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

echo "=== Euxis Version Consistency Check ==="

# Extract the authoritative version from registry.json
if [ ! -f "registry.json" ]; then
    echo "❌ FATAL: registry.json not found"
    exit 1
fi

REGISTRY_VERSION=$(python3 -c "import json; print(json.load(open('registry.json'))['protocol_version'])" 2>/dev/null || echo "")

if [ -z "$REGISTRY_VERSION" ]; then
    echo "❌ FATAL: Could not extract protocol_version from registry.json"
    exit 1
fi

echo "✅ Authoritative version from registry.json: $REGISTRY_VERSION"

violation_count=0

# Check VERSION file consistency
if [ -f "VERSION" ]; then
    VERSION_FILE_CONTENT=$(cat VERSION | tr -d '\n')
    if [ "$VERSION_FILE_CONTENT" != "$REGISTRY_VERSION" ]; then
        echo "❌ VIOLATION: VERSION file ($VERSION_FILE_CONTENT) != registry.json ($REGISTRY_VERSION)"
        violation_count=$((violation_count + 1))
    else
        echo "✅ VERSION file matches registry.json"
    fi
fi

# Check workflow files for hardcoded versions
echo ""
echo "Checking GitHub workflow files..."

# Look for hardcoded version patterns in workflow files
workflow_files=(.github/workflows/*.yml)
for workflow in "${workflow_files[@]}"; do
    if [ -f "$workflow" ]; then
        echo "Checking: $workflow"

        # Check for hardcoded Euxis versions (common patterns)
        hardcoded_patterns=(
            "v[0-9]+\.[0-9]+\.[0-9]+"
            "Euxis v[0-9]"
            "EUXIS v[0-9]"
            "version.*[0-9]+\.[0-9]+\.[0-9]+"
        )

        for pattern in "${hardcoded_patterns[@]}"; do
            matches=$(grep -n "$pattern" "$workflow" 2>/dev/null || true)
            if [ -n "$matches" ]; then
                # Filter out the dynamic version usage we implemented
                filtered_matches=$(echo "$matches" | grep -v "\$EUXIS_VERSION" | grep -v "\${{ github.event.inputs.version }}" || true)
                if [ -n "$filtered_matches" ]; then
                    echo "⚠️  POTENTIAL HARDCODED VERSION in $workflow:"
                    echo "$filtered_matches"
                    # Don't count as violation if it's in a comment or our implemented solution
                    if ! echo "$filtered_matches" | grep -q -E "(#|//|\$EUXIS_VERSION|\$\{\{)"; then
                        violation_count=$((violation_count + 1))
                    fi
                fi
            fi
        done
    fi
done

# Check agent prompts for version consistency
echo ""
echo "Checking agent prompt versions..."

prompt_files=(prompts/core/*.txt prompts/fleet/*.txt)
for prompt_file in "${prompt_files[@]}"; do
    if [ -f "$prompt_file" ]; then
        # Extract version from YAML frontmatter
        prompt_version=$(awk '/^---$/{flag=!flag;next} flag && /^version:/{gsub(/[" ]/, "", $2); print $2}' "$prompt_file" 2>/dev/null || echo "")
        if [ -n "$prompt_version" ] && [ "$prompt_version" != "$REGISTRY_VERSION" ]; then
            echo "❌ VIOLATION: $(basename "$prompt_file") version ($prompt_version) != registry.json ($REGISTRY_VERSION)"
            violation_count=$((violation_count + 1))
        fi
    fi
done

# Summary
echo ""
echo "=== Version Consistency Summary ==="
if [ $violation_count -eq 0 ]; then
    echo "✅ All version references are consistent with registry.json ($REGISTRY_VERSION)"
    echo "✅ No hardcoded versions found in workflows"
    exit 0
else
    echo "❌ $violation_count version consistency violations found"
    echo ""
    echo "Resolution steps:"
    echo "  1. Update registry.json protocol_version if this is a version bump"
    echo "  2. Run 'scripts/sync-versions.sh' to update all references"
    echo "  3. Use \$EUXIS_VERSION environment variable in workflows instead of hardcoding"
    echo "  4. Ensure VERSION file matches registry.json"
    exit 1
fi