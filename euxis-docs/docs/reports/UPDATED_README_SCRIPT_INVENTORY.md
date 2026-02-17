# Script Inventory for README.md

## Core Commands

### Primary Entry Points
- **euxis** - Main CLI entry point (symlink to euxis.sh)
- **euxis.sh** - Multi-Provider AI Agent Framework dispatcher
- **setup.sh** - Install and configure the Euxis Fleet system

### Fleet Management
- **euxis-squad** - Squad management and deployment
- **euxis-playbook** - Phased squad execution via playbooks
- **euxis-combo** - Sequential agent chain execution
- **euxis-dispatch** - Fleet deployment from architect manifests
- **euxis-synthesize** - Dynamic agent composition and dissolution

### System Operations
- **euxis-health** - System health check and diagnostics
- **euxis-verify** - Pre-commit quality gate (5 checks)
- **euxis-certify** - Full certification pipeline (7 gates)
- **euxis-test-infra** - Infrastructure unit tests
- **euxis-lint** - Static analysis (registry, headers, versions)

### Memory & Knowledge
- **euxis-cortex** - Tri-typed vector memory operations
- **euxis-graph** - GraphRAG knowledge graph management
- **euxis-council** - Multi-agent adversarial debate system

### Communication & UI
- **euxis-bus** - Async pub/sub message bus
- **euxis-tui** - Terminal user interface (Python Textual)
- **euxis-ui** - Alternative UI launcher
- **euxis-voice** - Offline voice interface

### Development & Testing
- **euxis-gym** - Agent evaluation and A/B testing
- **euxis-audit-run** - Security audit with probes
- **euxis-bench** - Performance benchmarking
- **euxis-loop** - Autonomous retry loop for agents

### Optimization & Maintenance
- **euxis-kaizen** - Continuous self-improvement cycle
- **euxis-daemon** - Periodic kaizen with fail-safe
- **euxis-optimize** - System-wide tune-up
- **euxis-polish** - Apply Euxis Standard to content

### Documentation & Deployment
- **euxis-sync-docs** - Documentation sync with approval gate
- **euxis-deploy** - Docker Compose enterprise deployment
- **euxis-docs-test** - Test documentation examples
- **euxis-cross-platform-verify** - Cross-platform compatibility testing

### Git & Workflow Integration
- **euxis-git-guard** - Git branch protection and workflow enforcement
- **euxis-hooks** - Git hooks management
- **euxis-setup-shell-hooks** - Install shell hooks for development

### Provider Integration
- **euxis-codex** - OpenAI Codex integration wrapper
- **euxis-slash** - Slash command interface

### Quality Assurance
- **euxis-verify-all** - Comprehensive verification across goals
- **euxis-license-check** - License compliance verification
- **euxis-shell-lint** - Shell script linting and standards

### Utilities
- **check_licenses.py** - Python script for license validation
- **check_package_maintenance.py** - Package maintenance status checker

## Library Modules (bin/lib/)

### Core Libraries
- **common.sh** - Logging, performance tracking, UI elements
- **cli.sh** - Command-line interface and argument parsing
- **dispatch.sh** - Fleet coordination and task distribution

### Specialized Libraries
- **agents.sh** - Agent discovery, validation, and lifecycle
- **providers.sh** - AI provider management and routing
- **memory.sh** - Persistent memory and knowledge storage
- **session.sh** - Session management and state tracking
- **template.sh** - Template processing and variable substitution
- **prompt.sh** - Prompt engineering and formatting
- **skill-detector.sh** - Capability detection and routing
- **validation.sh** - Input validation and security

## Git Hooks
- **prepare-commit-msg** - Automatic commit message formatting

## Total: 47 Scripts
- 38 executable commands
- 9 library modules
- 1 git hook
- 2 Python utilities