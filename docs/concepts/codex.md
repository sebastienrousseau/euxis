# Codex

## What It Is

The Codex is a template library for common agent tasks. It provides pre-built prompts and reasoning frameworks that agents can reference for consistent, high-quality outputs.

## When to Use

- **Standardized workflows**: Use codex templates for repeatable tasks
- **Quality consistency**: Ensure agents follow proven patterns
- **Knowledge reuse**: Capture successful approaches for future use

## Quick Example

```bash
# List available codex templates
ls ~/.euxis/config/codex/

# View a specific template
cat ~/.euxis/config/codex/architect/xray.txt
```

## Codex Structure

```
config/codex/
├── codex.json           # Master registry of all templates
├── architect/           # Architecture analysis templates
│   └── xray.txt         # Deep system analysis framework
├── code/                # Code operation templates
│   └── surgical-fix.txt # Minimal-change bug fixing
├── combo/               # Multi-agent chain templates
│   └── feature-factory.txt # Feature development workflow
└── research/            # Research templates
    └── decision-matrix.txt # Decision framework
```

## Available Templates

### Architect Templates

| Template | Purpose |
|----------|---------|
| `xray.txt` | Deep system analysis with architecture mapping |

### Code Templates

| Template | Purpose |
|----------|---------|
| `surgical-fix.txt` | Minimal-change approach to bug fixing |

### Combo Templates

| Template | Purpose |
|----------|---------|
| `feature-factory.txt` | End-to-end feature development workflow |

### Research Templates

| Template | Purpose |
|----------|---------|
| `decision-matrix.txt` | Structured decision-making framework |

## Template Format

Each template follows a consistent structure:

```text
# Template Name
## Purpose
[What this template accomplishes]

## When to Use
[Appropriate scenarios]

## Framework
[Step-by-step approach]

## Output Format
[Expected deliverables]
```

## Using Templates

### Direct Reference

Agents can reference templates in their prompts:

```bash
euxis architect "Analyze the payment system using the xray framework"
```

### Programmatic Access

```bash
# Get template content
cat ~/.euxis/config/codex/architect/xray.txt

# List all templates
jq '.templates' ~/.euxis/config/codex/codex.json
```

## Create Your Own

### Add a New Template

1. Create the template file:
```bash
cat > ~/.euxis/config/codex/research/competitive-analysis.txt << 'EOF'
# Competitive Analysis Framework

## Purpose
Systematic comparison of competing solutions.

## Framework
1. Identify key competitors
2. Define comparison criteria
3. Gather evidence for each
4. Create comparison matrix
5. Derive recommendations

## Output Format
- Comparison table
- Strengths/weaknesses summary
- Strategic recommendations
EOF
```

2. Register in codex.json:
```bash
jq '.templates += [{"id": "competitive-analysis", "category": "research", "path": "research/competitive-analysis.txt"}]' \
  ~/.euxis/config/codex/codex.json > tmp && mv tmp ~/.euxis/config/codex/codex.json
```

## Best Practices

1. **Keep templates focused** — One purpose per template
2. **Include examples** — Show expected outputs
3. **Version templates** — Track changes over time
4. **Test with agents** — Validate templates produce quality results

**See Also**: [Combos](combos.md), [Agents](agents.md)
