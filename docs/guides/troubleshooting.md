# (c) 2026 Euxis Fleet. All rights reserved.
# Troubleshooting

Common issues and solutions for Euxis.

## Installation Issues

### Problem: Command not found

**Symptoms:**
```bash
$ euxis
bash: euxis: command not found
```

**Solution:**
1. Verify PATH includes `~/bin`:
   ```bash
   echo $PATH | grep "bin"
   ```
2. Re-add to PATH:
   ```bash
   echo 'export PATH="$HOME/bin:$PATH"' >> ~/.profile
   source ~/.profile
   ```
3. Restart your terminal completely

---

### Problem: Setup script fails

**Symptoms:**
```
Error: Python 3.8+ required
```

**Solution:**
```bash
# Install Python 3.8+
brew install python@3.11

# Verify version
python3 --version
```

---

## Health Check Failures

### Problem: Agent Naming Convention

**Symptoms:**
```
✗ Agent Naming Convention: 2 files don't match registry
```

**Solution:**
```bash
# Check which files are misnamed
euxis-health --verbose

# Files in prompts/ must match registry.json agent IDs
# Example: architect.txt must match "id": "architect" in registry
```

---

### Problem: Documentation Drift

**Symptoms:**
```
✗ Documentation Drift: 3 documents outdated
```

**Solution:**
```bash
# Sync documentation
euxis-sync-docs

# Run certification to verify
euxis-certify
```

---

## Certification Failures

### Problem: Gate 5 Branding Check Fails

**Symptoms:**
```
FAIL: 1 of 5 recent commits missing signature
```

**Solution:**
Add branding signature to commit messages:
```bash
# Amend last commit with signature
git commit --amend -m "$(cat <<'EOF'
Your commit message here

---
🎨 Designed by **[Sebastien Rousseau](https://sebastienrousseau.com/)**
🚀 Engineered with **[Euxis](https://euxis.co/)** — Enterprise Unified eXecution Intelligence System
EOF
)"
```

---

### Problem: Gate 3 Infrastructure Tests Fail

**Symptoms:**
```
INFRASTRUCTURE VALIDATION & TESTS FAILED
Logic test failures: 2
```

**Solution:**
```bash
# Run detailed test output
euxis-test-infra

# Check specific failures
cat /tmp/euxis_test_*.log
```

---

## Agent Execution Issues

### Problem: Unknown Agent

**Symptoms:**
```
[euxis] ERROR: Unknown agent: my-agent
```

**Solution:**
```bash
# List available agents
euxis --help | grep "Available Agents" -A 50

# Verify agent exists in registry
jq '.agents[].id' ~/.euxis/registry.json
```

---

### Problem: Provider Not Available

**Symptoms:**
```
[euxis] ERROR: Provider 'claude' not configured
```

**Solution:**
```bash
# Check available providers
which claude gemini ollama

# Install missing provider
brew install claude-cli  # or your preferred provider

# Use fallback provider
euxis architect "Your task" ollama
```

---

### Problem: Agent Times Out

**Symptoms:**
```
[euxis] Agent timed out after 300s
```

**Solution:**
```bash
# Increase timeout
export EUXIS_API_TIMEOUT=600
euxis architect "Complex task"

# Or use faster local provider
euxis architect "Complex task" ollama
```

---

## Dispatch & Playbook Issues

### Problem: Dispatch Fails Verification

**Symptoms:**
```
1 of 6 missions failed. Check logs in /tmp/euxis_dispatch_*/
```

**Solution:**
```bash
# Check failure logs
cat /tmp/euxis_dispatch_*/*.log | grep -A 10 "ERROR\|FAIL"

# Check verification results
cat /tmp/euxis_dispatch_*/.results/*

# Re-run specific agent
euxis <failed-agent> "Task" claude
```

---

### Problem: Playbook Phase Fails

**Symptoms:**
```
PHASE 2 (Build) FAILED
Playbook aborted at phase 2
```

**Solution:**
```bash
# Check phase logs
euxis-playbook status <session-id>

# Resume from specific phase
euxis-playbook run <playbook> "<goal>" --from-phase 2

# Run in dry-run mode first
euxis-playbook run <playbook> "<goal>" --dry-run
```

---

## Memory & Cortex Issues

### Problem: Cortex Unavailable

**Symptoms:**
```
[WARNING] Cortex unavailable (chromadb not installed)
```

**Solution:**
```bash
# Install ChromaDB
pip install chromadb sentence-transformers

# Initialize cortex
euxis-cortex init

# Verify
euxis-cortex status
```

---

### Problem: Memory Not Persisting

**Symptoms:**
Agents don't remember previous interactions.

**Solution:**
```bash
# Check memory path
ls ~/.euxis/data/projects/*/memory.md

# Verify project name
echo $EUXIS_PROJECT

# Force memory write
euxis-cortex remember "Important fact" "agent-name" --type episodic
```

---

## Performance Issues

### Problem: Slow Agent Responses

**Symptoms:**
Agent takes >60 seconds for simple tasks.

**Solution:**
```bash
# Use local provider for speed
euxis architect "Quick task" ollama

# Check provider health
euxis-health --providers

# Use faster model
export EUXIS_OLLAMA_MODEL=llama2:7b
```

---

### Problem: High Memory Usage

**Symptoms:**
System slows down during fleet dispatch.

**Solution:**
```bash
# Limit parallel agents
export EUXIS_MAX_PARALLEL=3
euxis-dispatch manifest.json

# Use hierarchical mode (sequential)
euxis-squad deploy build "Task" --mode hierarchical
```

---

## Getting More Help

### Verbose Output
```bash
# Enable debug mode
export EUXIS_DEBUG=1
euxis architect "Task"
```

### Log Files
```bash
# Check session logs
ls ~/.euxis/data/projects/*/output/

# Check dispatch logs
ls /tmp/euxis_dispatch_*/
```

### Health Report
```bash
# Full system diagnosis
euxis-health --verbose > health-report.txt
euxis-certify > certification-report.txt
```

---

**Still stuck?** Open an issue at [github.com/sebastienrousseau/euxis/issues](https://github.com/sebastienrousseau/euxis/issues)
