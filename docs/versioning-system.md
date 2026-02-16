# Euxis Centralized Versioning System

This document describes how version management works across the Euxis system, ensuring consistency and eliminating hardcoded version references.

## Version Authority

The **single source of truth** for the Euxis system version is:

```
registry.json → "protocol_version" field
```

**Current version: 0.0.8**

All other version references MUST derive from this source.

## System Components

### 1. Registry.json (Primary Source)
- **File**: `registry.json`
- **Field**: `protocol_version`
- **Purpose**: Authoritative version source used by all workflows and documentation

### 2. VERSION File (Backup Source)
- **File**: `VERSION`
- **Purpose**: Simple version reference for scripts and tools that can't parse JSON
- **Sync**: Must match `registry.json` at all times

### 3. CI/CD Workflows (Dynamic Usage)
All GitHub workflows now extract version dynamically:

```yaml
- name: Extract version from registry
  run: |
    if [ -n "${{ github.event.inputs.version }}" ]; then
      echo "Using override version: ${{ github.event.inputs.version }}"
      echo "EUXIS_VERSION=${{ github.event.inputs.version }}" >> $GITHUB_ENV
    else
      echo "Extracting version from registry.json..."
      EUXIS_VERSION=$(python3 -c "import json; print(json.load(open('registry.json'))['protocol_version'])")
      echo "Found version: $EUXIS_VERSION"
      echo "EUXIS_VERSION=$EUXIS_VERSION" >> $GITHUB_ENV
    fi
```

### 4. Agent Prompts (YAML Frontmatter)
All agent prompts include version in their YAML frontmatter:

```yaml
---
agent_id: example
version: "0.0.8"
---
```

## Maintenance Scripts

### Version Consistency Checker
```bash
scripts/check-version-consistency.sh
```

**Purpose**: Validates that all version references are consistent
**Usage**: Run before releases and in CI/CD pipelines
**Returns**: Exit code 0 if consistent, non-zero if violations found

### Version Synchronization
```bash
scripts/sync-versions.sh
```

**Purpose**: Updates all version references to match `registry.json`
**Usage**: Run after updating `protocol_version` in `registry.json`
**Updates**:
- `VERSION` file
- All agent prompt YAML frontmatter
- Documentation version references

## Version Bump Process

When releasing a new version:

1. **Update the authoritative source**:
   ```bash
   # Edit registry.json
   vim registry.json
   # Update "protocol_version": "X.Y.Z"
   ```

2. **Synchronize all references**:
   ```bash
   scripts/sync-versions.sh
   ```

3. **Verify consistency**:
   ```bash
   scripts/check-version-consistency.sh
   ```

4. **Commit changes**:
   ```bash
   git add -A
   git commit -m "version: bump to X.Y.Z"
   ```

## CI/CD Integration

### Workflow Version Override
All workflows support manual version override:

```yaml
workflow_dispatch:
  inputs:
    version:
      description: 'Override version (leave empty to use registry.json)'
      required: false
      default: ''
```

### Environment Variable Usage
Workflows expose version as `$EUXIS_VERSION`:

```bash
echo "Building Euxis v$EUXIS_VERSION..."
echo "=== Euxis v$EUXIS_VERSION Platform Report ===" > report.md
```

## Benefits

### ✅ Single Source of Truth
- No version conflicts between files
- One place to update for new releases
- Automatic consistency across all components

### ✅ Eliminates Hardcoding
- No more `v0.0.8` scattered across workflow files
- Dynamic version extraction from authoritative source
- Workflow flexibility with manual override capability

### ✅ Validation & Automation
- Automated consistency checking
- One-command synchronization
- CI/CD integration for version validation

### ✅ Audit Trail
- Clear version history in `registry.json`
- Automated detection of version drift
- Compliance with versioning protocol

## Troubleshooting

### Version Mismatch Detected
1. Check which files are inconsistent:
   ```bash
   scripts/check-version-consistency.sh
   ```

2. Synchronize to registry version:
   ```bash
   scripts/sync-versions.sh
   ```

### Workflow Using Wrong Version
1. Verify `registry.json` has correct `protocol_version`
2. Ensure workflow has version extraction step
3. Check that `$EUXIS_VERSION` is used instead of hardcoded values

### Manual Override Not Working
1. Verify workflow input is properly defined
2. Check that environment variable logic handles override correctly
3. Ensure subsequent steps use `$EUXIS_VERSION` environment variable

## Migration Notes

### Before (Hardcoded)
```yaml
# BAD: Hardcoded version
echo "Euxis v0.0.8 Platform Report" > report.md
default: 'v0.0.8'
```

### After (Dynamic)
```yaml
# GOOD: Dynamic version from registry
echo "Euxis v$EUXIS_VERSION Platform Report" > report.md
EUXIS_VERSION=$(python3 -c "import json; print(json.load(open('registry.json'))['protocol_version'])")
```

This system ensures version consistency across the entire Euxis codebase while providing flexibility for testing and development workflows.