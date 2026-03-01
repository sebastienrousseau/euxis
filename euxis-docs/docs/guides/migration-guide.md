# Migration Guide

This guide provides step-by-step instructions for upgrading between Euxis versions. Each migration section includes breaking changes, new features, configuration updates, and rollback procedures.

---

## Table of Contents

1. [Version History](#version-history)
2. [Migrating from v0.0.6 to v0.0.7](#migrating-from-v006-to-v007)
   - [New Features](#new-features)
   - [Breaking Changes](#breaking-changes)
   - [Deprecated Features](#deprecated-features)
   - [Step-by-Step Upgrade Instructions](#step-by-step-upgrade-instructions)
   - [Configuration Changes](#configuration-changes)
   - [Backward Compatibility Notes](#backward-compatibility-notes)
3. [General Migration Checklist](#general-migration-checklist)
4. [Troubleshooting Common Migration Issues](#troubleshooting-common-migration-issues)
5. [Rollback Procedures](#rollback-procedures)
6. [Testing Your Migration](#testing-your-migration)

---

## Version History

| Version | Release Date | Major Changes |
|---------|--------------|---------------|
| 0.0.7   |-02-09   | TUI (ETX), SQLite registry, 16 language playbooks, performance optimizations |
| 0.0.6   |-01-15   | Initial fleet architecture, JSON registry, core agents |

---

## Migrating from v0.0.6 to v0.0.7

### New Features

#### 1. ETX Terminal User Interface (TUI)

The ETX (Euxis Terminal eXperience) is a full-featured terminal UI built on the Textual framework.

**New screens and capabilities:**
- Metrics dashboard with real-time sparklines
- Squad detail view and fleet monitor
- Playbook browser and cortex screen
- Provider modal for AI provider selection
- Welcome, help, and about screens
- Tool runner for direct agent invocation

**Launch the TUI:**
```bash
python -m tui
```

**New configuration options in `~/.euxis/config/etx-settings.json`:**
```json
{
  "theme": "etx-liquid-glass",
  "default_provider": "claude",
  "show_agent_tags": true,
  "reduced_motion": false,
  "accessible_mode": false,
  "locale": "en",
  "grid_columns": 4,
  "recent_agents": [],
  "recent_commands": []
}
```

#### 2. SQLite Registry

The fleet registry now uses SQLite as the primary storage format with JSON fallback.

**Registry location:**
```
~/.euxis/agents/registry.db      # Primary (SQLite)
~/.euxis/agents/registry.json    # Fallback (JSON)
```

**Query agents directly:**
```bash
sqlite3 ~/.euxis/agents/registry.db "SELECT id, tier, activation FROM agents_complete;"
```

**Expected output:**
```
accountant|fleet|default
ambassador|fleet|on-demand
animator|fleet|default
arbiter|core|core
architect|core|core
...
```

#### 3. New Agents

v0.0.7 introduces enhanced agent capabilities and new specialist agents:

| Agent | Tier | Activation | Purpose |
|-------|------|------------|---------|
| tactician | fleet | default | TUI/CLI terminal design specialist |
| conduit | fleet | specialist | Real-time audio and voice pipeline |
| cryptographer | fleet | specialist | Cryptographic implementation and PQC |
| ledger | fleet | specialist | Payments and ISO 20022 compliance |
| custodian | fleet | specialist | Rust crate publishing and maintenance |

#### 4. Language Playbooks

16 language-specific playbooks with 10-gate enforcement:

```bash
ls ~/.euxis/playbooks/
```

**Expected output:**
```
bash.yaml       go.yaml         python.yaml     swift.yaml
c.yaml          java.yaml       ruby.yaml       typescript.yaml
cpp.yaml        javascript.yaml rust.yaml       zig.yaml
csharp.yaml     kotlin.yaml     scala.yaml      ...
```

#### 5. Voice Pipeline Optimizations

Significant performance improvements for voice/audio features:

| Metric | v0.0.6 | v0.0.7 | Improvement |
|--------|--------|--------|-------------|
| Cold Start | 7,764ms | 2,463ms | 68% faster |
| Whisper Load | 10,216ms | 1,408ms | 86% faster |
| Piper Load | 6,755ms | 1,466ms | 78% faster |
| Short TTS | 489ms | 70ms | 86% faster |
| Medium TTS | 3,614ms | 420ms | 88% faster |

---

### Breaking Changes

#### 1. Registry Format Migration

**Impact:** Scripts that directly parse `agents/registry.json` may need updates.

**Before (v0.0.6):**
```python
import json
with open("~/.euxis/agents/registry.json") as f:
    agents = json.load(f)["agents"]
```

**After (v0.0.7):**
```python
from tui.core.registry import FleetRegistry

registry = FleetRegistry.load()
agents = registry.agents
```

**Migration path:** The system automatically falls back to JSON if SQLite is unavailable. Update scripts to use the registry module for future compatibility.

#### 2. Provider Command Syntax

**Impact:** Provider invocation now requires explicit syntax.

**Before (v0.0.6):**
```bash
euxis architect "Review this code"  # Used default provider
```

**After (v0.0.7):**
```bash
euxis architect "Review this code" claude    # Explicit provider
euxis architect "Review this code" gemini    # Alternative provider
euxis architect "Review this code" ollama    # Local provider
```

#### 3. Removed Providers

The following providers have been removed:

| Removed Provider | Replacement |
|------------------|-------------|
| OpenCode | Claude CLI or Gemini |
| Kilo Code | Claude CLI or Gemini |
| Amazon Q | Kiro CLI |

**Update your scripts:**
```bash
# Before
euxis architect "Task" opencode

# After
euxis architect "Task" claude
```

#### 4. Configuration File Location

**Impact:** Configuration moved from root to dedicated directory.

**Before (v0.0.6):**
```
~/.euxis/capabilities.json
```

**After (v0.0.7):**
```
~/.euxis/config/capabilities.json
~/.euxis/config/etx-settings.json
```

**Migration command:**
```bash
mkdir -p ~/.euxis/config
mv ~/.euxis/capabilities.json ~/.euxis/config/ 2>/dev/null || true
```

---

### Deprecated Features

| Feature | Status | Removal Target | Migration Path |
|---------|--------|----------------|----------------|
| JSON-only registry | Deprecated | v0.0.2 | Use SQLite registry |
| Em-dash formatting in prompts | Deprecated | v0.0.2 | Use colons for natural tone |
| Audit files in git tracking | Removed | N/A | Files now in `.gitignore` |

---

### Step-by-Step Upgrade Instructions

#### Pre-Migration Checklist

```bash
# 1. Backup current installation
cp -r ~/.euxis ~/.euxis.backup.$(date +%Y%m%d)

# 2. Check current version
cat ~/.euxis/agents/registry.json | jq -r '.protocol_version'
# Expected: 0.0.6

# 3. Verify git status
cd ~/.euxis && git status
# Ensure working directory is clean
```

#### Migration Steps

**Step 1: Update the repository**
```bash
cd ~/.euxis
git fetch origin
git checkout feat/v0.0.7-release  # Or: git pull origin main
```

**Step 2: Install TUI dependencies**
```bash
pip install textual[all] --upgrade
```

**Step 3: Migrate configuration**
```bash
# Create config directory
mkdir -p ~/.euxis/config

# Move capabilities file
mv ~/.euxis/capabilities.json ~/.euxis/config/ 2>/dev/null || true

# Initialize ETX settings
python -c "from tui.core.config import ETXConfig; ETXConfig().save()"
```

**Step 4: Initialize SQLite registry**
```bash
# The registry auto-migrates on first load
python -c "from tui.core.registry import FleetRegistry; r = FleetRegistry.load(); print(f'Loaded {len(r.agents)} agents')"
```

**Expected output:**
```
Loaded 40 agents
```

**Step 5: Verify installation**
```bash
euxis-health --verbose
euxis-certify
```

**Expected output:**
```
Euxis Health Check
==================
✓ Registry: 40 agents loaded
✓ Configuration: etx-settings.json valid
✓ Playbooks: 16 language playbooks found
✓ TUI: Textual 0.89+ installed

All checks passed.
```

**Step 6: Run test suite**
```bash
cd ~/.euxis
python -m pytest tests/ -v
```

---

### Configuration Changes

#### New Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `EUXIS_REGISTRY_TYPE` | `sqlite` | Registry storage type (`sqlite`, `json`) |
| `EUXIS_TUI_THEME` | `etx-liquid-glass` | Default TUI theme |
| `EUXIS_DEFAULT_PROVIDER` | `claude` | Default AI provider |

#### ETX Configuration Schema

```json
{
  "theme": "string",           // TUI color theme
  "default_provider": "string", // claude, gemini, ollama, etc.
  "show_agent_tags": "boolean", // Display capability tags
  "reduced_motion": "boolean",  // Accessibility: reduce animations
  "accessible_mode": "boolean", // Full accessibility mode
  "locale": "string",          // Localization (en, es, fr, etc.)
  "grid_columns": "integer",   // Fleet grid column count (1-8)
  "recent_agents": "array",    // Recently used agent IDs
  "recent_commands": "array"   // Recent command history
}
```

#### Registry Schema Changes

**New fields in agent records:**

| Field | Type | Description |
|-------|------|-------------|
| `capability_tags` | array | Standardized capability identifiers |
| `activation` | string | Activation mode: `default`, `on-demand`, `specialist`, `core` |

---

### Backward Compatibility Notes

1. **JSON Registry Fallback:** If `agents/registry.db` is missing or corrupted, the system automatically loads from `agents/registry.json`.

2. **Provider Fallback:** If the specified provider is unavailable, the system attempts providers in this order:
   ```
   claude -> gemini -> ollama -> aider
   ```

3. **Configuration Defaults:** Missing configuration keys use built-in defaults. No manual configuration required for basic usage.

4. **API Compatibility:** All v0.0.6 agent commands work unchanged. Provider must now be specified explicitly.

---

## General Migration Checklist

Use this checklist template for any version migration:

```markdown
## Migration Checklist: v{OLD} to v{NEW}

### Pre-Migration
- [ ] Read changelog for breaking changes
- [ ] Backup current installation: `cp -r ~/.euxis ~/.euxis.backup.$(date +%Y%m%d)`
- [ ] Document current configuration
- [ ] Check disk space: minimum 500MB free
- [ ] Notify team of planned migration

### Migration
- [ ] Pull latest code from repository
- [ ] Install new dependencies
- [ ] Run migration scripts (if any)
- [ ] Update configuration files
- [ ] Initialize new components

### Post-Migration
- [ ] Run health check: `euxis-health`
- [ ] Run certification: `euxis-certify`
- [ ] Execute test suite: `pytest tests/`
- [ ] Test core agent functionality
- [ ] Verify TUI launches correctly
- [ ] Check provider connectivity

### Validation
- [ ] Compare agent count with expected
- [ ] Verify squad configurations
- [ ] Test playbook execution
- [ ] Confirm euxis-runtime/memory/cortex functionality
- [ ] Performance benchmark (optional)

### Documentation
- [ ] Update internal documentation
- [ ] Document any custom configurations
- [ ] Record migration date and issues encountered
```

---

## Troubleshooting Common Migration Issues

### Issue: SQLite Registry Not Found

**Symptoms:**
```
Warning: agents/registry.db not found, falling back to JSON
```

**Solution:**
```bash
# Generate SQLite registry from JSON
python -c "
from tui.core.registry import FleetRegistry
import sqlite3
import json

# Load JSON registry
with open('~/.euxis/agents/registry.json'.replace('~', '$HOME')) as f:
    data = json.load(f)

# The registry module handles creation automatically
registry = FleetRegistry.load()
print(f'Registry loaded with {len(registry.agents)} agents')
"
```

### Issue: TUI Dependencies Missing

**Symptoms:**
```
ModuleNotFoundError: No module named 'textual'
```

**Solution:**
```bash
pip install textual[all] rich
```

### Issue: Configuration Not Loading

**Symptoms:**
```
TypeError: Expected dict in config file, got str
```

**Solution:**
```bash
# Reset configuration to defaults
rm ~/.euxis/config/etx-settings.json
python -c "from tui.core.config import ETXConfig; ETXConfig().save()"
```

### Issue: Provider Command Fails

**Symptoms:**
```
[euxis] ERROR: Provider 'opencode' not found
```

**Solution:**
```bash
# Use supported provider
euxis architect "Your task" claude

# Or check available providers
which claude gemini ollama
```

### Issue: Agent Count Mismatch

**Symptoms:**
```
Expected 40 agents, found 35
```

**Solution:**
```bash
# Verify registry files
jq '.agents | length' ~/.euxis/agents/registry.json
sqlite3 ~/.euxis/agents/registry.db "SELECT COUNT(*) FROM agents_complete;"

# If mismatch, re-sync from JSON
rm ~/.euxis/agents/registry.db
python -c "from tui.core.registry import FleetRegistry; FleetRegistry.load()"
```

### Issue: Playbooks Not Found

**Symptoms:**
```
[euxis] ERROR: Playbook 'python' not found
```

**Solution:**
```bash
# Verify playbook directory
ls ~/.euxis/playbooks/

# Re-download if missing
git checkout -- playbooks/
```

---

## Rollback Procedures

### Quick Rollback (< 24 hours since migration)

```bash
# 1. Stop any running Euxis processes
pkill -f "python.*tui" 2>/dev/null || true

# 2. Restore from backup
rm -rf ~/.euxis
mv ~/.euxis.backup.YYYYMMDD ~/.euxis

# 3. Verify restoration
cat ~/.euxis/agents/registry.json | jq -r '.protocol_version'
```

### Git-Based Rollback

```bash
cd ~/.euxis

# 1. Check available versions
git tag -l

# 2. Checkout previous version
git checkout v0.0.6

# 3. Remove v0.0.7 artifacts
rm -f ~/.euxis/agents/registry.db
rm -rf ~/.euxis/config/etx-settings.json

# 4. Verify rollback
euxis-health
```

### Selective Rollback

If only specific components need rollback:

```bash
# Rollback registry only
git checkout v0.0.6 -- agents/registry.json
rm ~/.euxis/agents/registry.db

# Rollback playbooks only
git checkout v0.0.6 -- playbooks/

# Rollback TUI only (disable without removing)
mv ~/.euxis/tui ~/.euxis/tui.disabled
```

### Post-Rollback Verification

```bash
# 1. Verify version
cat ~/.euxis/agents/registry.json | jq -r '.protocol_version'
# Expected: 0.0.6

# 2. Run health check
euxis-health

# 3. Test agent execution
euxis architect "Test query" claude

# 4. Document rollback
echo "Rolled back to v0.0.6 on $(date)" >> ~/.euxis/migration.log
```

---

## Testing Your Migration

### Automated Test Suite

```bash
cd ~/.euxis

# Run full test suite
python -m pytest tests/ -v --tb=short

# Run TUI-specific tests
python -m pytest tests/test_tui*.py -v

# Run registry tests
python -m pytest tests/test_registry*.py -v
```

**Expected output:**
```
========================= test session starts ==========================
collected 447 items

tests/test_registry.py::test_load_from_sqlite PASSED
tests/test_registry.py::test_json_fallback PASSED
tests/test_tui_config.py::test_config_load PASSED
tests/test_tui_config.py::test_config_save PASSED
...

========================= 447 passed in 45.23s =========================
```

### Manual Verification Tests

#### Test 1: Registry Load

```bash
python -c "
from tui.core.registry import FleetRegistry
r = FleetRegistry.load()
print(f'Agents: {len(r.agents)}')
print(f'Squads: {len(r.squads)}')
print(f'Combos: {len(r.combos)}')
print(f'Version: {r.version}')
"
```

**Expected output:**
```
Agents: 40
Squads: 6
Combos: 8
Version: 0.0.7
```

#### Test 2: TUI Launch

```bash
# Launch TUI in test mode
timeout 5 python -m tui --help || echo "TUI module accessible"
```

#### Test 3: Agent Execution

```bash
# Test core agent
euxis architect "List 3 design patterns" claude

# Test fleet agent
euxis debugger "Explain null pointer exceptions" claude

# Test specialist agent
euxis cryptographer "Explain ECDSA" claude
```

#### Test 4: Configuration Persistence

```bash
python -c "
from tui.core.config import ETXConfig

# Load, modify, save
config = ETXConfig.load()
config.grid_columns = 3
config.save()

# Reload and verify
config2 = ETXConfig.load()
assert config2.grid_columns == 3
print('Configuration persistence: OK')
"
```

#### Test 5: Playbook Execution

```bash
# Dry run a playbook
euxis-playbook run python "Create a hello world script" --dry-run
```

### Performance Benchmarks

```bash
# Run performance verification
python docs/benchmarks/performance-verification.py

# Run voice pipeline benchmark (if voice enabled)
python docs/benchmarks/voice-perf-benchmark.py
```

**Expected output:**
```
PASS Thread Creation: 0.20ms <= 10ms budget
PASS Lock Contention: 5.22ms <= 100ms budget
PASS Health Check: 100.01ms <= 500ms budget
...
All metrics passing: 13/13 (100%)
```

### Integration Test

```bash
# Full integration test
euxis-certify

# Expected output
# Gate 1: ✓ STRUCTURE
# Gate 2: ✓ SECURITY
# Gate 3: ✓ INFRASTRUCTURE
# Gate 4: ✓ DOCUMENTATION
# Gate 5: ✓ BRANDING
# ...
# CERTIFICATION: PASSED
```

---

**Migration support:** If you encounter issues not covered in this guide, check the [Troubleshooting Guide](troubleshooting.md) or open an issue at [github.com/sebastienrousseau/euxis/issues](https://github.com/sebastienrousseau/euxis/issues).
