# Troubleshooting Guide


This comprehensive troubleshooting guide covers common issues, diagnostic commands, and solutions for the Euxis Fleet system.

---

## Quick Diagnostics

Before diving into specific issues, run these diagnostic commands to assess your system state.

### euxis-health Command

The `euxis-health` command performs 10 automated checks on your fleet:

```bash
euxis-health
```

**Expected output (healthy system):**

```
==================================================
  EUXIS HEALTH CHECK
==================================================

[1/10] Naming Consistency...
  All agent filenames match their agent_id.

[2/10] Script Hardening...
  All bash scripts have hardening flags.

[3/10] Orphan Detection...
  All agents are registered. No orphans or ghosts.

[4/10] Header Schema Validation...
  All agents have complete headers (agent_id, role, version, tags, last_updated).

[5/10] Documentation Drift...
  docs/guides/user-guide.md is current (updated within 24h).

[6/10] Certification Readiness...
  euxis-certify is present and executable.
  euxis-lint is present and executable.
  euxis-test-infra is present and executable.

[7/10] Provider Connectivity...
  FOUND: claude
  FOUND: gemini
  NOT FOUND: ollama (optional -- install to enable this provider)
  3 of 9 provider CLIs available.

[8/10] Codex Integrity...
  Codex manifest valid. 12 templates verified.

[9/10] Bus Pipe Activity...
  3 pipe(s), 45 message(s) total.
  All pipes active.

[10/10] Cortex Connectivity...
  Cortex is reachable.
  128 memories stored.

==================================================
  HEALTH CHECK PASSED with 2 warning(s).
==================================================
```

**Interpreting Health Check Output:**

| Status | Meaning |
|--------|---------|
| `HEALTH CHECK PASSED` | All critical checks passed |
| `PASSED with N warning(s)` | Functional but some optional features degraded |
| `HEALTH CHECK FAILED` | Critical issues require attention |

**Command Options:**

```bash
# Exit code only (for scripts/CI)
euxis-health --silent

# JSON output for programmatic parsing
euxis-health --json
```

**JSON output example:**

```json
{
  "status": "healthy",
  "failures": 0,
  "warnings": 2,
  "agents": 41,
  "providers": 3,
  "bus_pipes": 3,
  "bus_messages": 45,
  "timestamp": "2026-02-14T10:30:00Z"
}
```

### euxis-certify Command

The `euxis-certify` command runs the complete 6-gate certification pipeline:

```bash
euxis-certify
```

**Expected output (certified system):**

```
==================================================
  EUXIS CERTIFICATION PIPELINE
==================================================

[GATE 0] Environment Pre-Validation (Fail-Fast)...
--------------------------------------------------
  Running full validation...
  Running minimal validation...
  Registry database valid (50 agents)
  Euxis directory structure is valid
  All system dependencies available
  Full validation passed.

[GATE 0a] Certification Prerequisites...
--------------------------------------------------
  Static analysis tool is accessible
  Infrastructure testing is accessible
  Main Euxis script is accessible
  Branding signature exists
  Main README exists
  User guide exists
  Fleet guide exists
  Temporary file write access verified
  Current version detected: 0.0.8
  All certification prerequisites met

[GATE 0b] Security & Quality Enforcement...
--------------------------------------------------
  PASS: euxis-bus uses jq for safe JSON generation
  PASS: Provider model name validation present
  PASS: Task input sanitization in dispatch pipeline
  PASS: PII sanitization in logging functions
  PASS: Project license declared in pyproject.toml
  PASS: Prompt injection detection in task validation
  PASS: Message bus sender validation present
  PASS: Message bus retention policy (cleanup) present
  PASS: TUI deployment confirmation gates present
  All mandatory security controls verified

[GATE 1+2] Static Analysis (Lint)...
--------------------------------------------------
  All agents passed lint checks

[GATE 3] Infrastructure Tests...
--------------------------------------------------
  All infrastructure tests passed

[GATE 4] Semantic Verification...
--------------------------------------------------
  Golden dataset found. Running sample eval...
  Testing architect agent (timeout: 180s)...
  PASS: Architect produced output (2847 chars).

[GATE 5] Branding Compliance...
--------------------------------------------------
  PASS: config/branding/signature.txt exists.
  PASS: PR template carries branding signature.
  PASS: README.md carries author attribution.
  PASS: prepare-commit-msg hook is present and executable.
  PASS: commit.gpgsign is globally enabled.
  PASS: Signing key is configured (format: ssh).
  PASS: Last 5 commits carry branding signature.
  PASS: Last 5 commits are cryptographically signed.
  PASS: Signature URL points to euxis.co.
  PASS: GitHub Actions branding-check workflow exists.

[GATE 6] Documentation Governance...
--------------------------------------------------
  Documentation directory accessible
  jq available for JSON validation
  PASS: euxis-sync-docs is present and executable.
  PASS: README.md exists.
  PASS: docs/guides/user-guide.md exists.
  PASS: docs/guides/fleet-guide.md exists.
  PASS: All documentation is current (updated within 7 days).
  PASS: README.md references version 0.0.8.
  Validating JSON manifest files...
  PASS: agents/registry.json is valid JSON.
  PASS: agents/squads.json is valid JSON.
  PASS: config/capabilities.json is valid JSON.
  PASS: config/codex/codex.json is valid JSON.

==================================================
  EUXIS IS CERTIFIED.
  All toll gates passed.
==================================================
```

### Common Health Check Failures

| Check | Failure Message | Quick Fix |
|-------|-----------------|-----------|
| Naming Consistency | `MISMATCH: agent.txt` | Rename file to match `agent_id` in header |
| Script Hardening | `MISSING HARDENING` | Add `set -euo pipefail` to script |
| Orphan Detection | `ORPHAN AGENTS` | Add agent to registry or remove file |
| Orphan Detection | `GHOST ENTRIES` | Remove registry entry or create prompt file |
| Header Schema | `MISSING FIELD` | Add required field to agent frontmatter |
| Documentation Drift | `STALE` | Run `euxis-sync-docs` |
| Provider Connectivity | `NOT FOUND` | Install provider CLI or use available provider |
| Codex Integrity | `MISSING TEMPLATE` | Add template file or update codex.json |
| Bus Pipe Activity | `stale pipes` | Run `euxis-bus cleanup` |
| Cortex Connectivity | `unreachable` | Run `euxis-cortex init` |

---

## Installation Issues

### PATH Not Configured Correctly

**Symptoms:**

```bash
$ euxis
bash: euxis: command not found
```

**Diagnosis:**

```bash
# Check if ~/cli/bin is in PATH
echo $PATH | tr ':' '\n' | grep -E "(home|~).*bin"

# Check if euxis symlink exists
ls -la ~/cli/bin/euxis
```

**Solution:**

1. **Add ~/cli/bin to PATH** (choose your shell):

   **For Bash (~/.bashrc or ~/.bash_profile):**
   ```bash
   echo 'export PATH="$HOME/cli/bin:$PATH"' >> ~/.bashrc
   source ~/.bashrc
   ```

   **For Zsh (~/.zshrc):**
   ```bash
   echo 'export PATH="$HOME/cli/bin:$PATH"' >> ~/.zshrc
   source ~/.zshrc
   ```

   **For Fish (~/.config/fish/config.fish):**
   ```fish
   set -Ux fish_user_paths $HOME/cli/bin $fish_user_paths
   ```

2. **Re-run setup if symlinks are missing:**
   ```bash
   ~/.euxis/setup.sh --force
   ```

3. **Verify installation:**
   ```bash
   which euxis
   # Expected: /home/username/cli/bin/euxis
   ```

### Missing Dependencies

**Bash 4.0+ Required:**

```bash
# Check Bash version
bash --version

# If version < 4.0, install newer Bash
# macOS:
brew install bash

# Ubuntu/Debian:
sudo apt update && sudo apt install bash
```

**Python 3.8+ Required:**

```bash
# Check Python version
python3 --version

# If version < 3.8, install Python 3.12+
# macOS:
brew install python@3.12

# Ubuntu/Debian:
sudo apt update && sudo apt install python3.12 python3.12-venv
```

**Required Python venv:**

```bash
# If venv doesn't exist, create it
python3 -m venv ~/.euxis/.venv

# Install dependencies
~/.euxis/.venv/cli/bin/pip install -r ~/.euxis/requirements.txt
```

**Optional but recommended tools:**

```bash
# jq - JSON processing (highly recommended)
brew install jq      # macOS
apt install jq       # Ubuntu/Debian

# gh - GitHub CLI (for PR operations)
brew install gh      # macOS
apt install gh       # Ubuntu/Debian

# timeout/gtimeout - Command timeouts
brew install coreutils  # macOS (provides gtimeout)
```

### Permission Issues

**Symptoms:**

```
Permission denied: ~/.euxis/cli/bin/euxis
```

**Solution:**

```bash
# Fix executable permissions on all scripts
chmod +x ~/.euxis/cli/bin/*

# Fix directory permissions
chmod 755 ~/.euxis
chmod 755 ~/.euxis/cli/bin
chmod 755 ~/.euxis/core/lib

# Verify
ls -la ~/.euxis/cli/bin/euxis
# Should show: -rwxr-xr-x
```

### Shell Compatibility

**Zsh Users:**

```bash
# Add to ~/.zshrc
export PATH="$HOME/cli/bin:$PATH"
export EUXIS_HOME="$HOME/.euxis"

# Reload
source ~/.zshrc
```

**Bash Users:**

```bash
# Add to ~/.bashrc (or ~/.bash_profile on macOS)
export PATH="$HOME/cli/bin:$PATH"
export EUXIS_HOME="$HOME/.euxis"

# Reload
source ~/.bashrc
```

**Fish Users:**

```fish
# Run these commands
set -Ux PATH $HOME/cli/bin $PATH
set -Ux EUXIS_HOME $HOME/.euxis
```

---

## Provider Issues

### Provider Not Responding

**Symptoms:**

```
[euxis] ERROR: Provider command timed out after 300s
```

**Diagnosis:**

```bash
# Test provider directly
which claude && claude --version
which gemini && gemini --version
which ollama && ollama --version

# Check if API key is set (example for Claude)
echo $ANTHROPIC_API_KEY | head -c 10
```

**Solutions:**

1. **Increase timeout:**
   ```bash
   export EUXIS_API_TIMEOUT=600  # 10 minutes
   euxis architect "Your task"
   ```

2. **Use a different provider:**
   ```bash
   # Use local provider (faster, no API calls)
   euxis architect "Your task" ollama

   # Use Gemini instead
   euxis architect "Your task" gemini
   ```

3. **Check provider service status:**
   ```bash
   # For Ollama (local)
   ollama list

   # For Claude
   claude doctor
   ```

### API Key Configuration

**Claude (Anthropic):**

```bash
# Set API key
export ANTHROPIC_API_KEY="sk-ant-..."

# Add to shell profile for persistence
echo 'export ANTHROPIC_API_KEY="sk-ant-..."' >> ~/.bashrc
```

**Gemini (Google):**

```bash
# Set API key
export GOOGLE_API_KEY="AIza..."

# Or use gcloud authentication
gcloud auth application-default login
```

**OpenAI:**

```bash
# Set API key
export OPENAI_API_KEY="sk-..."
```

**Ollama (Local):**

```bash
# No API key needed - ensure service is running
ollama serve &

# Pull required model
ollama pull llama3.2
```

### Rate Limiting

**Symptoms:**

```
[euxis] ERROR: Rate limit exceeded
[euxis] ERROR: 429 Too Many Requests
```

**Solutions:**

1. **Wait and retry:**
   ```bash
   sleep 60 && euxis architect "Your task"
   ```

2. **Use a different provider:**
   ```bash
   # Fall back to local provider
   euxis architect "Your task" ollama
   ```

3. **Reduce concurrent agents:**
   ```bash
   export EUXIS_MAX_PARALLEL=2
   euxis-dispatch manifest.json
   ```

### Timeout Errors

**Symptoms:**

```
[euxis] ERROR: Provider command timed out after 300s: claude API
[euxis] Consider increasing EUXIS_API_TIMEOUT or using a different provider
```

**Solutions:**

```bash
# Increase timeout
export EUXIS_API_TIMEOUT=600  # 10 minutes

# Use faster provider for simple tasks
euxis butler "Quick question" ollama

# Check if timeout command is available
which timeout gtimeout

# Install if missing (macOS)
brew install coreutils
```

### Provider Fallback Not Working

**Symptoms:**

Agent fails instead of falling back to alternate provider.

**Diagnosis:**

```bash
# List available providers
euxis-health 2>&1 | grep -A 15 "Provider Connectivity"
```

**Solution:**

Provider fallback is automatic via the tiering system. Manually specify a provider:

```bash
# Force specific provider
euxis architect "Your task" claude
euxis architect "Your task" gemini
euxis architect "Your task" ollama
```

**Tiering map (default providers by agent):**

| Agent Type | Default Provider | Fallback |
|------------|------------------|----------|
| orchestrator, architect | claude | gemini |
| researcher, deep-researcher | gemini | claude |
| debugger, tester | goose | claude |
| butler, librarian | ollama | claude |
| optimizer | qwen | claude |

---

## Agent Issues

### Agent Not Found

**Symptoms:**

```
[euxis] ERROR: Unknown agent: my-agent
```

**Diagnosis:**

```bash
# List all registered agents
euxis --help 2>&1 | grep -A 100 "Available Agents"

# Check registry (SQLite)
python3 -c "import sqlite3; conn = sqlite3.connect('$HOME/.euxis/agents/registry.db'); print('\n'.join([r[0] for r in conn.execute('SELECT id FROM agents ORDER BY id')]))"

# Check registry (JSON fallback)
jq -r '.agents[].id' ~/.euxis/agents/registry.json | sort
```

**Solutions:**

1. **Verify agent name spelling:**
   ```bash
   # Common agents
   euxis architect "Task"     # Not 'architekt'
   euxis debugger "Task"     # Not 'bugfixer'
   euxis tester "Task"   # Not 'unittester'
   ```

2. **Check if prompt file exists:**
   ```bash
   ls ~/.euxis/agents/prompts/core/*.txt
   ls ~/.euxis/agents/prompts/fleet/*.txt
   ```

3. **Re-sync registry:**
   ```bash
   euxis-health  # Will report orphans/ghosts
   ```

### Agent Stuck in Active State

**Symptoms:**

Agent appears to be running but never completes.

**Diagnosis:**

```bash
# Check lifecycle state
cat ~/.euxis/runtime/data/lifecycle/*.state

# List active agents
for f in ~/.euxis/runtime/data/lifecycle/*.state; do
    agent=$(basename "$f" .state)
    state=$(cat "$f")
    [[ "$state" == "active" ]] && echo "$agent: $state"
done

# Check transition log
tail -20 ~/.euxis/runtime/data/lifecycle/transitions.jsonl
```

**Solutions:**

1. **Clean stale agents (30 minute timeout):**
   ```bash
   # Manual cleanup via the agents library
   source ~/.euxis/core/lib/agents.sh
   cleanup_stale_agents 1800  # 30 minutes
   ```

2. **Force state reset:**
   ```bash
   # Reset specific agent
   echo "idle" > ~/.euxis/runtime/data/lifecycle/architect.state

   # Reset all agents
   for f in ~/.euxis/runtime/data/lifecycle/*.state; do
       echo "idle" > "$f"
   done
   ```

3. **Check for hung processes:**
   ```bash
   ps aux | grep -E "(claude|gemini|ollama)" | grep -v grep
   # Kill if necessary
   pkill -f "claude.*--print"
   ```

### Agent Returning Empty Response

**Symptoms:**

```
[euxis] ERROR: Architect returned empty output
```
or
```
CERTIFICATION FAILED at Gate 4 (Semantic).
```

**Diagnosis:**

```bash
# Enable debug mode
export EUXIS_DEBUG=1
euxis architect "Test task"

# Check provider directly
echo "Hello" | claude --print --model claude-sonnet-4-20250514
```

**Solutions:**

1. **Check API key validity:**
   ```bash
   # Test provider directly
   claude --version
   claude "Say hello"
   ```

2. **Check prompt file integrity:**
   ```bash
   # Validate prompt has required sections
   head -50 ~/.euxis/agents/prompts/core/architect.txt

   # Check frontmatter
   grep -E "^(agent_id|role|version):" ~/.euxis/agents/prompts/core/architect.txt
   ```

3. **Check model availability:**
   ```bash
   # For Ollama, verify model is pulled
   ollama list

   # Pull if missing
   ollama pull llama3.2
   ```

### Agent Lifecycle Errors

**Symptoms:**

```
[euxis] ERROR: Agent lifecycle transition failed
```

**Solutions:**

```bash
# Ensure lifecycle directory exists
mkdir -p ~/.euxis/runtime/data/lifecycle

# Check permissions
chmod 755 ~/.euxis/runtime/data/lifecycle

# Clear corrupted state files
rm ~/.euxis/runtime/data/lifecycle/*.state
```

---

## Memory Issues

### Memory File Corruption

**Symptoms:**

```
[euxis] ERROR: Path traversal rejected
```
or garbled content in memory.md files.

**Diagnosis:**

```bash
# Check memory file validity
cat ~/.euxis/runtime/data/projects/*/*/memory.md | head -20

# Look for binary or corrupted content
file ~/.euxis/runtime/data/projects/*/*/memory.md
```

**Solutions:**

1. **Reset memory for specific agent:**
   ```bash
   PROJECT="your-project"
   AGENT="architect"

   # Backup
   cp ~/.euxis/runtime/data/projects/$PROJECT/$AGENT/memory.md{,.bak}

   # Reset
   echo "# Memory: $AGENT" > ~/.euxis/runtime/data/projects/$PROJECT/$AGENT/memory.md
   ```

2. **Restore from backup:**
   ```bash
   cp ~/.euxis/runtime/data/projects/$PROJECT/$AGENT/memory.md.bak \
      ~/.euxis/runtime/data/projects/$PROJECT/$AGENT/memory.md
   ```

### Memory Pruning Not Working

**Symptoms:**

Memory files grow unbounded (>500 lines).

**Diagnosis:**

```bash
# Check memory file sizes
wc -l ~/.euxis/runtime/data/projects/*/*/memory.md
```

**Solutions:**

1. **Run manual pruning:**
   ```bash
   source ~/.euxis/core/lib/memory.sh

   # Prune specific memory file
   prune_memory ~/.euxis/runtime/data/projects/myproject/architect/memory.md

   # Prune all memory in a project
   prune_project_memory ~/.euxis/runtime/data/projects/myproject
   ```

2. **Configure pruning thresholds:**
   ```bash
   # In your shell profile
   export EUXIS_MEMORY_MAX_LINES=500      # Trigger pruning above this
   export EUXIS_MEMORY_RECENT_KEEP=100    # Keep this many recent entries
   ```

3. **Run Kaizen maintenance:**
   ```bash
   euxis-kaizen prune
   ```

### Semantic Drift Warnings

**Symptoms:**

```
[DRIFT] New fact negates existing: Uses JWT authentication
[DRIFT] Value change detected -- old: 'jwt', new: 'oauth'
```

**Understanding Drift:**

Semantic drift occurs when new memories contradict existing knowledge. This is a feature, not a bug -- it helps maintain knowledge consistency.

**Solutions:**

1. **Review and resolve contradictions:**
   ```bash
   # View drift warnings
   grep -r "\[DRIFT\]" ~/.euxis/runtime/data/projects/*/*/memory.md
   ```

2. **Manually resolve:**
   ```bash
   source ~/.euxis/core/lib/memory.sh

   # Supersede old knowledge with new
   resolve_memory_contradiction \
       ~/.euxis/runtime/data/projects/myproject/architect/memory.md \
       "Project uses OAuth 2.0 for authentication" \
       "architect" \
       "supersede"
   ```

3. **Keep both versions:**
   ```bash
   resolve_memory_contradiction \
       ~/.euxis/runtime/data/projects/myproject/architect/memory.md \
       "New authentication approach" \
       "architect" \
       "keep_both"
   ```

### Cross-Agent Memory Not Accessible

**Symptoms:**

Agents cannot access sibling agent memories.

**Diagnosis:**

```bash
# Check project structure
ls -la ~/.euxis/runtime/data/projects/myproject/

# Verify memory files exist for other agents
ls ~/.euxis/runtime/data/projects/myproject/*/memory.md
```

**Solutions:**

1. **Ensure correct project context:**
   ```bash
   export EUXIS_PROJECT="myproject"
   euxis architect "Your task"
   ```

2. **Check directory structure:**
   ```bash
   # Memories should be at: ~/.euxis/runtime/data/projects/{project}/{agent}/memory.md
   mkdir -p ~/.euxis/runtime/data/projects/myproject/architect
   touch ~/.euxis/runtime/data/projects/myproject/architect/memory.md
   ```

---

## TUI Issues

### TUI Not Launching

**Symptoms:**

```
[euxis] ERROR: Python venv not found at ~/.euxis/.venv
```
or
```
ModuleNotFoundError: No module named 'textual'
```

**Solutions:**

1. **Create/recreate venv:**
   ```bash
   python3 -m venv ~/.euxis/.venv
   ~/.euxis/.venv/cli/bin/pip install -r ~/.euxis/requirements.txt
   ```

2. **Install Textual manually:**
   ```bash
   ~/.euxis/.venv/cli/bin/pip install textual textual-dev
   ```

3. **Verify installation:**
   ```bash
   ~/.euxis/.venv/cli/bin/python -c "import textual; print(textual.__version__)"
   ```

### Display Rendering Issues

**Symptoms:**

- Garbled characters
- Missing borders
- Overlapping text

**Solutions:**

1. **Check terminal compatibility:**
   ```bash
   # Verify terminal supports 256 colors
   echo $TERM
   # Should be: xterm-256color, screen-256color, etc.

   # Set if missing
   export TERM=xterm-256color
   ```

2. **Check terminal size:**
   ```bash
   # Minimum recommended: 80x24
   tput cols
   tput lines

   # Resize terminal window if needed
   ```

3. **Try different terminal emulator:**
   - **Recommended:** iTerm2 (macOS), Alacritty, Kitty, WezTerm
   - **Avoid:** Basic Terminal.app (limited Unicode support)

4. **Reset terminal:**
   ```bash
   reset
   euxis-tui
   ```

### Keyboard Shortcuts Not Working

**Symptoms:**

Ctrl+K, Ctrl+T, etc. don't respond.

**Solutions:**

1. **Check for conflicting terminal shortcuts:**
   - macOS Terminal: Preferences > Keyboard
   - iTerm2: Preferences > Keys
   - Disable any Ctrl+ shortcuts that conflict

2. **Try alternative shortcuts:**
   ```
   F1          - Help
   F2          - Welcome screen
   F5          - Refresh
   ?           - Keyboard reference
   Escape      - Go back
   ```

3. **List all bindings:**
   Press `F1` or `?` in the TUI to see the full keyboard reference.

### Theme Not Applying

**Symptoms:**

TUI starts with wrong colors or default theme.

**Solutions:**

1. **Cycle themes manually:**
   Press `Ctrl+T` in the TUI to cycle through themes.

2. **Reset theme config:**
   ```bash
   # Check current config
   cat ~/.euxis/config/etx.json

   # Reset to default
   echo '{"theme": "etx-liquid-glass", "locale": "en"}' > ~/.euxis/config/etx.json
   ```

3. **Available themes:**
   - `etx-liquid-glass` (default)
   - `etx-midnight`
   - `etx-sunrise`
   - Custom themes via CSS

---

## Performance Issues

### Slow Agent Dispatch

**Symptoms:**

Agent takes >5 seconds just to start.

**Diagnosis:**

```bash
# Enable performance logging
export EUXIS_DEBUG=1
euxis architect "Test"

## Metrics Location

Performance metrics are stored at `~/.euxis/metrics/events.jsonl`.

# Check performance metrics
tail -20 ~/.euxis/metrics/events.jsonl
```

**Solutions:**

1. **Use SQLite registry (faster):**
   ```bash
   # Check if using SQLite
   ls ~/.euxis/agents/registry.db

   # If not, migrate
   python3 ~/.euxis/scripts/migrate-registry-to-sqlite.py
   ```

2. **Disable performance tracking (if not needed):**
   ```bash
   export EUXIS_PERF_DISABLE=1
   ```

3. **Use local provider for speed:**
   ```bash
   euxis architect "Quick task" ollama
   ```

### High Latency

**Symptoms:**

```
[euxis] WARN: LATENCY BUDGET EXCEEDED: provider_execution took 8500ms (budget: 5000ms)
```

**Solutions:**

1. **Adjust performance budgets:**
   ```bash
   export EUXIS_PROMPT_BUDGET_MS=1000
   export EUXIS_API_TIMEOUT=600
   ```

2. **Use tiered providers:**
   - Complex tasks: claude (slower, higher quality)
   - Simple tasks: ollama (faster, local)

3. **Check network:**
   ```bash
   # Test API latency
   time curl -s https://api.anthropic.com/v1/health
   ```

### Memory Usage

**Symptoms:**

System slows down during fleet dispatch.

**Solutions:**

1. **Limit parallel agents:**
   ```bash
   export EUXIS_MAX_PARALLEL=3
   euxis-dispatch manifest.json
   ```

2. **Use hierarchical mode:**
   ```bash
   euxis-squad deploy build "Task" --mode hierarchical
   ```

3. **Monitor resource usage:**
   ```bash
   # Watch memory during dispatch
   watch -n 1 'ps aux | grep -E "(python|claude|ollama)" | awk "{sum+=\$4} END {print sum\"%\"}"'
   ```

### Token Budget Exceeded

**Symptoms:**

```
[euxis] ERROR: Token budget exceeded
[euxis] WARN: Response truncated
```

**Solutions:**

1. **Reduce task complexity:**
   Break large tasks into smaller subtasks.

2. **Use summarization:**
   ```bash
   euxis librarian "Summarize the following: [long text]"
   ```

3. **Increase max turns:**
   ```bash
   export EUXIS_MAX_TURNS=50
   euxis architect "Complex task"
   ```

---

## Common Error Messages

| Error Message | Cause | Solution |
|---------------|-------|----------|
| `Unknown agent: X` | Agent ID doesn't exist in registry | Check spelling, run `euxis --help` for list |
| `Provider 'X' not configured` | Provider CLI not installed | Install provider: `brew install X` |
| `Python venv not found` | Venv not created | Run `python3 -m venv ~/.euxis/.venv` |
| `Rate limit exceeded` | API rate limiting | Wait 60s or use different provider |
| `Provider command timed out` | API too slow / network issue | Increase `EUXIS_API_TIMEOUT` |
| `Path traversal rejected` | Security: Invalid file path | Use valid paths within `~/.euxis` |
| `Invalid model name` | Malformed model identifier | Check `EUXIS_*_MODEL` env vars |
| `Registry not found` | Missing agents/registry.db/json | Run `~/.euxis/setup.sh` |
| `FATAL: Cannot load validation library` | Missing lib/validation.sh | Re-clone repository |
| `Task input rejected: prompt injection` | Security: Dangerous input | Remove injection patterns from task |
| `chromadb not installed` | Missing Cortex dependencies | Run `pip install chromadb` |
| `jq not installed` | Missing JSON processor | Run `brew install jq` |
| `timeout command not available` | Missing coreutils | Run `brew install coreutils` |
| `Module not found: textual` | TUI dependencies missing | Run `pip install textual` |
| `CERTIFICATION FAILED at Gate X` | Certification check failed | See gate output for specific issue |
| `MISMATCH: filename vs agent_id` | Prompt file name != agent_id | Rename file to match agent_id |
| `ORPHAN AGENTS` | Agent file not in registry | Add to registry or remove file |
| `GHOST ENTRIES` | Registry entry has no file | Create prompt file or remove entry |

---

## Getting Help

### Debug Mode

Enable comprehensive debug logging:

```bash
# Enable debug mode
export EUXIS_DEBUG=1

# Run command
euxis architect "Your task"

# Debug output shows:
# - Prompt assembly timing
# - Provider selection
# - Memory retrieval
# - Performance metrics
```

### Log Locations

| Log Type | Location |
|----------|----------|
| Agent output | `~/.euxis/runtime/data/projects/{project}/{agent}/output/*.md` |
| Agent audit | `~/.euxis/runtime/data/projects/{project}/{agent}/audit.md` |
| Agent memory | `~/.euxis/runtime/data/projects/{project}/{agent}/memory.md` |
| Performance metrics | `~/.euxis/metrics/events.jsonl` |
| Lifecycle transitions | `~/.euxis/runtime/data/lifecycle/transitions.jsonl` |
| Dispatch logs | `/tmp/euxis_dispatch_*/*.log` |
| Certification logs | `/tmp/euxis_cert_*.log` |
| Bus messages | `~/.euxis/runtime/data/bus/pipes/*/*.msg` |

### Generating a Bug Report

```bash
# Generate comprehensive system report
{
    echo "=== Euxis Bug Report ==="
    echo "Date: $(date)"
    echo ""

    echo "=== System Info ==="
    uname -a
    echo "Bash: $(bash --version | head -1)"
    echo "Python: $(python3 --version)"
    echo ""

    echo "=== Health Check ==="
    euxis-health 2>&1
    echo ""

    echo "=== Recent Errors ==="
    grep -r "ERROR" ~/.euxis/runtime/data/lifecycle/transitions.jsonl 2>/dev/null | tail -20
    echo ""

    echo "=== Provider Status ==="
    for p in claude gemini ollama goose qwen; do
        which $p 2>/dev/null && echo "$p: $(which $p)" || echo "$p: not found"
    done
} > ~/euxis-bug-report.txt

echo "Report saved to ~/euxis-bug-report.txt"
```

### Reporting Issues

1. **GitHub Issues:** [github.com/sebastienrousseau/euxis/issues](https://github.com/sebastienrousseau/euxis/issues)

2. **Include in your report:**
   - Bug report output (see above)
   - Steps to reproduce
   - Expected vs actual behavior
   - Relevant log snippets

3. **Security issues:** Email security@euxis.co (do not post publicly)

---

**Still stuck?** Run `euxis-health --json` and open an issue with the output attached.
