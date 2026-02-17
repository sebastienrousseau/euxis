# Fish completion for euxis
# Install: cp euxis.fish ~/.config/fish/completions/

# Disable file completion by default
complete -c euxis -f

# Commands
complete -c euxis -n "__fish_use_subcommand" -a "squad" -d "Manage agent squads"
complete -c euxis -n "__fish_use_subcommand" -a "combo" -d "Chain agents sequentially"
complete -c euxis -n "__fish_use_subcommand" -a "playbook" -d "Phased execution with gates"
complete -c euxis -n "__fish_use_subcommand" -a "dispatch" -d "Deploy fleet from manifest"
complete -c euxis -n "__fish_use_subcommand" -a "council" -d "Multi-agent debate"
complete -c euxis -n "__fish_use_subcommand" -a "loop" -d "Autonomous retry loop"
complete -c euxis -n "__fish_use_subcommand" -a "verify" -d "Pre-commit quality gates"
complete -c euxis -n "__fish_use_subcommand" -a "health" -d "System health check"
complete -c euxis -n "__fish_use_subcommand" -a "certify" -d "Certification pipeline"
complete -c euxis -n "__fish_use_subcommand" -a "lint" -d "Static analysis"
complete -c euxis -n "__fish_use_subcommand" -a "test" -d "Unit tests"
complete -c euxis -n "__fish_use_subcommand" -a "audit" -d "Security audit"
complete -c euxis -n "__fish_use_subcommand" -a "bench" -d "Performance benchmarks"
complete -c euxis -n "__fish_use_subcommand" -a "cortex" -d "Vector memory"
complete -c euxis -n "__fish_use_subcommand" -a "graph" -d "Knowledge graph"
complete -c euxis -n "__fish_use_subcommand" -a "bus" -d "Message bus"
complete -c euxis -n "__fish_use_subcommand" -a "gateway" -d "Gateway control"
complete -c euxis -n "__fish_use_subcommand" -a "deploy" -d "Docker deployment"
complete -c euxis -n "__fish_use_subcommand" -a "daemon" -d "Kaizen daemon"
complete -c euxis -n "__fish_use_subcommand" -a "gym" -d "Agent evaluation"
complete -c euxis -n "__fish_use_subcommand" -a "kaizen" -d "Self-improvement"
complete -c euxis -n "__fish_use_subcommand" -a "agents" -d "List all agents"
complete -c euxis -n "__fish_use_subcommand" -a "search" -d "Find agents by capability"
complete -c euxis -n "__fish_use_subcommand" -a "doctor" -d "Verify installation"
complete -c euxis -n "__fish_use_subcommand" -a "replay" -d "Replay recorded traces"

# Core Agents
complete -c euxis -n "__fish_use_subcommand" -a "arbiter" -d "Resolves conflicts"
complete -c euxis -n "__fish_use_subcommand" -a "architect" -d "Designs systems"
complete -c euxis -n "__fish_use_subcommand" -a "auditor" -d "Code quality review"
complete -c euxis -n "__fish_use_subcommand" -a "critic" -d "Constructive feedback"
complete -c euxis -n "__fish_use_subcommand" -a "guard" -d "Security gatekeeper"
complete -c euxis -n "__fish_use_subcommand" -a "historian" -d "Project history"
complete -c euxis -n "__fish_use_subcommand" -a "librarian" -d "Documentation"
complete -c euxis -n "__fish_use_subcommand" -a "orchestrator" -d "Workflow coordination"
complete -c euxis -n "__fish_use_subcommand" -a "pair" -d "Coding partner"
complete -c euxis -n "__fish_use_subcommand" -a "planner" -d "Project plans"
complete -c euxis -n "__fish_use_subcommand" -a "reviewer" -d "Code review"
complete -c euxis -n "__fish_use_subcommand" -a "route" -d "Task routing"

# Fleet Agents - Development
complete -c euxis -n "__fish_use_subcommand" -a "debugger" -d "Bug fixing"
complete -c euxis -n "__fish_use_subcommand" -a "tester" -d "Test writing"
complete -c euxis -n "__fish_use_subcommand" -a "repairer" -d "Code repair"
complete -c euxis -n "__fish_use_subcommand" -a "maintainer" -d "Maintenance"
complete -c euxis -n "__fish_use_subcommand" -a "optimizer" -d "Performance"

# Fleet Agents - Research
complete -c euxis -n "__fish_use_subcommand" -a "researcher" -d "Information gathering"
complete -c euxis -n "__fish_use_subcommand" -a "deep-researcher" -d "Deep analysis"
complete -c euxis -n "__fish_use_subcommand" -a "investigator" -d "Root cause"
complete -c euxis -n "__fish_use_subcommand" -a "inspector" -d "Code inspection"

# Fleet Agents - Security
complete -c euxis -n "__fish_use_subcommand" -a "pentester" -d "Penetration testing"
complete -c euxis -n "__fish_use_subcommand" -a "sentinel" -d "Threat detection"
complete -c euxis -n "__fish_use_subcommand" -a "watchdog" -d "Monitoring"
complete -c euxis -n "__fish_use_subcommand" -a "gatekeeper" -d "Access control"
complete -c euxis -n "__fish_use_subcommand" -a "cryptographer" -d "Cryptography"

# Squad subcommands
complete -c euxis -n "__fish_seen_subcommand_from squad" -a "list" -d "List squads"
complete -c euxis -n "__fish_seen_subcommand_from squad" -a "info" -d "Squad details"
complete -c euxis -n "__fish_seen_subcommand_from squad" -a "deploy" -d "Deploy squad"

# Squad names for deploy/info
complete -c euxis -n "__fish_seen_subcommand_from squad; and __fish_seen_subcommand_from deploy info" -a "vision" -d "Strategy & Architecture"
complete -c euxis -n "__fish_seen_subcommand_from squad; and __fish_seen_subcommand_from deploy info" -a "build" -d "Engineering"
complete -c euxis -n "__fish_seen_subcommand_from squad; and __fish_seen_subcommand_from deploy info" -a "quality" -d "Assurance & Security"
complete -c euxis -n "__fish_seen_subcommand_from squad; and __fish_seen_subcommand_from deploy info" -a "growth" -d "Branding & Docs"
complete -c euxis -n "__fish_seen_subcommand_from squad; and __fish_seen_subcommand_from deploy info" -a "experience" -d "UI Excellence"
complete -c euxis -n "__fish_seen_subcommand_from squad; and __fish_seen_subcommand_from deploy info" -a "specialist" -d "Domain-Specific"

# Combo subcommands
complete -c euxis -n "__fish_seen_subcommand_from combo" -a "list" -d "List combos"
complete -c euxis -n "__fish_seen_subcommand_from combo" -a "info" -d "Combo details"
complete -c euxis -n "__fish_seen_subcommand_from combo" -a "run" -d "Run combo"

# Combo names
complete -c euxis -n "__fish_seen_subcommand_from combo; and __fish_seen_subcommand_from run info" -a "envision" -d "Research to review"
complete -c euxis -n "__fish_seen_subcommand_from combo; and __fish_seen_subcommand_from run info" -a "protect" -d "Security hardening"
complete -c euxis -n "__fish_seen_subcommand_from combo; and __fish_seen_subcommand_from run info" -a "craft" -d "Content creation"
complete -c euxis -n "__fish_seen_subcommand_from combo; and __fish_seen_subcommand_from run info" -a "refine" -d "Design iteration"
complete -c euxis -n "__fish_seen_subcommand_from combo; and __fish_seen_subcommand_from run info" -a "seal" -d "Crypto audit"

# Playbook subcommands
complete -c euxis -n "__fish_seen_subcommand_from playbook" -a "list" -d "List playbooks"
complete -c euxis -n "__fish_seen_subcommand_from playbook" -a "info" -d "Playbook details"
complete -c euxis -n "__fish_seen_subcommand_from playbook" -a "run" -d "Run playbook"

# Cortex subcommands
complete -c euxis -n "__fish_seen_subcommand_from cortex" -a "search" -d "Search memories"
complete -c euxis -n "__fish_seen_subcommand_from cortex" -a "prune" -d "Clean old"
complete -c euxis -n "__fish_seen_subcommand_from cortex" -a "sync" -d "Sync remote"
complete -c euxis -n "__fish_seen_subcommand_from cortex" -a "stats" -d "Statistics"

# Search categories
complete -c euxis -n "__fish_seen_subcommand_from search" -a "security" -d "Security agents"
complete -c euxis -n "__fish_seen_subcommand_from search" -a "testing" -d "Testing agents"
complete -c euxis -n "__fish_seen_subcommand_from search" -a "design" -d "Design agents"
complete -c euxis -n "__fish_seen_subcommand_from search" -a "research" -d "Research agents"
complete -c euxis -n "__fish_seen_subcommand_from search" -a "operations" -d "Operations agents"

# Global options
complete -c euxis -l provider -d "Override AI provider" -xa "claude gemini openai ollama qwen crush kiro-cli goose"
complete -c euxis -l json -d "Output as JSON"
complete -c euxis -l verbose -d "Verbose logging"
complete -c euxis -l no-color -d "Disable colors"
complete -c euxis -l help -d "Show help"
complete -c euxis -l version -d "Show version"
