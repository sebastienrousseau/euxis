# Changelog

All notable changes to Euxis will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Documentation audit with AI-ready `llms.txt` file
- Gateway protocol documentation and quickstart
- Gateway auth and config references
- Gateway JSON Schemas for request/response/event frames
- Gateway protocol validation harness
- Gateway CLI reference and adapter design notes

---

## [0.0.7] - 2026-02-09

### Added
- **ETX Terminal Experience**: Full-featured TUI built on Python Textual framework with dark/light/contrast themes
- **Metrics Dashboard**: Real-time performance sparklines and fleet monitoring with visual indicators
- **Squad Detail View**: Comprehensive squad management interface with member status and coordination
- **Playbook Browser**: Visual playbook selection and execution with 16 language playbooks
- **Cortex Screen**: Memory browser for episodic, semantic, and procedural knowledge
- **Provider Modal**: Interactive AI provider selection with capability indicators
- **Welcome Screen**: Onboarding experience for new users with guided setup
- **Help and About Screens**: In-app documentation and version information
- **Tool Runner Screen**: Direct tool execution interface with output streaming
- **Command Palette**: Quick actions via `Ctrl+K` with fuzzy search
- **16 Language Playbooks**: Go, Rust, Python, TypeScript, Java, Kotlin, Swift, C++, Ruby, PHP, Scala, Elixir, Haskell, Clojure, C#, and Dart with consistency-audited 10-gate enforcement
- **91 TUI Tests**: Comprehensive test suite (51 + 40 tests) ensuring UI reliability
- **SQLite Registry**: Migrated from JSON to SQLite for improved performance across 15 files
- **Agent Escalation Protocols**: Automated escalation paths for complex tasks
- **Shell Test Infrastructure**: Comprehensive testing framework for shell libraries
- **Python Unit Tests**: 95%+ code coverage with comprehensive test suite
- **Weekly Cache Cleanup**: Automated CI workflow for cache management

### Changed
- **Architecture Cleanup**: Lean cross-platform structure with improved modularity
- **CI Optimization**: GitHub Actions workflows optimized for $0 budget operation
- **Provider Clarity**: Made provider usage explicit with command syntax documentation
- **Documentation Style**: Apple-style progressive disclosure throughout all guides
- **Branding Signatures**: Standardized across all outputs and documentation
- **Version Format**: Updated for certification compliance requirements
- **Agent Descriptions**: Expanded with clearer role definitions and capabilities
- **Tone Consistency**: Replaced em dashes with colons for natural, conversational tone
- **Provider Naming**: Renamed Amazon Q to Kiro CLI for clarity
- **Removed Providers**: OpenCode and Kilo Code removed from supported providers

### Deprecated
- **JSON Registry Format**: SQLite is now the primary storage; JSON support will be removed in v0.1.0

### Removed
- **OpenCode Provider**: Discontinued due to lack of active development
- **Kilo Code Provider**: Merged functionality into other providers
- **Gitignored Audit Files**: Removed from version control for cleaner repository

### Fixed
- **Test Failures**: Resolved 3 test failures from standards enforcement
- **Playbook Standards**: Addressed 14 playbook findings for consistency
- **TUI Security**: Hardened input validation and sandboxing
- **Dependency Pinning**: All dependencies now pinned to specific versions
- **Broken Documentation Links**: Fixed cross-references throughout docs
- **Gemini Provider Config**: Removed invalid `--search-web` flag
- **Security Vulnerabilities**: Resolved all identified security findings with mandatory enforcement gate
- **Dispatch Output**: Fixed "Verify: null" display issue in dispatch functions
- **Fleet Monitor Layout**: Fixed grid layout taking incorrect space allocation
- **Bash Compatibility**: Resolved `set -e` compatibility issues in setup.sh and euxis-health
- **CI Workflow Bugs**: Fixed issues across enforced-ci, merge-gate, and cross-platform pipelines
- **Subprocess Forks**: Eliminated unnecessary process spawning
- **Shell Portability**: Hardened for BSD/macOS compatibility
- **Copyright Headers**: Added missing headers across all source files
- **Input Validation**: Strengthened validation throughout the codebase

### Security
- **Input Validation Library**: New centralized validation for all user inputs
- **Timeout Handling**: Graceful degradation for long-running operations
- **Sandbox Mode**: TUI operations run in isolated environment
- **Dependency Audit**: All dependencies scanned and vulnerabilities resolved

### Breaking Changes

#### Command Syntax
- Provider selection now uses explicit syntax: `euxis <agent> "<task>" <provider>`
- Previous implicit provider detection is no longer supported

#### Migration Steps
1. Update any scripts using implicit provider selection
2. Review playbooks for deprecated JSON registry references
3. Update automation using removed providers (OpenCode, Kilo Code)

### Migration Notes
- Run `euxis-health` after upgrade to verify installation
- Run `euxis-certify` to validate configuration compliance
- Existing JSON configurations will auto-migrate to SQLite on first run

---

## [0.0.6] - 2026-01-15

### Added
- **Voice Pipeline**: Sub-3-second response latency with 100% compliance (13/13 metrics)
- **Cross-Platform CI**: Comprehensive workflows for macOS, Linux, and WSL
- **Verify-All Pipeline**: Unified verification across all quality gates
- **UI/UX Fleet Agent Prompts**: Enhanced visual feedback during operations
- **Concurrency Tests**: Performance benchmarks for parallel agent execution
- **Fleet Constitution**: Formal governance document for agent coordination
- **6 Missing Protocols**: Complete protocol coverage for all agent types
- **Branding Signature System**: Git hooks and PR template enforcement
- **Codex Prompt Template Library**: 4 battle-tested templates for common tasks
- **Security-Lead Core Agent**: Dedicated security oversight role
- **3 Core Agent Promotions**: reviewer, product-manager, compliance-officer elevated to core tier

### Changed
- **Agent Classification**: Reclassified into Core/Default/On-Demand tiers
- **Fleet Freeze Rule**: Established stable agent roster with activation modes
- **README Rewrite**: Apple-style documentation with progressive disclosure
- **Provider Models Update**: Claude Sonnet 4 and Gemini 2.0 Flash support
- **Capability Registry**: Enhanced with squad and playbook engine integration

### Fixed
- **Multi-line PR Bodies**: Proper handling in branding compliance check
- **macOS CI Timeout**: Removed unavailable timeout command
- **Cross-Platform YAML**: Resolved parsing errors in CI workflows
- **eval Injection**: Removed sensitive file vulnerabilities
- **Weak Language**: Eliminated throughout documentation

### Security
- **Gate 5 Enforcement**: Branding compliance verification
- **Gate 6 Enforcement**: Documentation governance checks
- **Cryptographic Signing**: Enforced commit.gpgsign in Gate 5
- **Sensitive File Removal**: Purged credentials and secrets from history

---

## [0.0.5] - 2026-01-01

### Added
- **Protocol v6.0**: Message bus architecture with GraphRAG memory
- **Dynamic Agent Synthesis**: Runtime agent creation for specialized tasks
- **Hybrid Dispatch**: Combined sync/async execution patterns
- **Tri-Typed Memory**: Episodic, semantic, and procedural memory stores
- **3-Layer Self-Correction**: Automated error detection and recovery
- **Conflict Resolution Protocol**: Multi-agent disagreement handling
- **Squads System**: Pre-configured agent groups for common workflows
- **Playbooks System**: Repeatable multi-step automation sequences
- **Combos System**: Sequential agent chains with output passing
- **Progress Monitor**: Visual dispatch progress during execution
- **QA-Coordinator Agent**: Quality assurance orchestration
- **Incident-Commander Agent**: Emergency response coordination

### Changed
- **Framework Architecture**: Migrated from shell-gpt to Codex CLI
- **Dispatch System**: Added progress monitoring and live status tables
- **Documentation**: Brand-evangelist polish pass on all content
- **Enterprise Rebrand**: "Enterprise Unified eXecution Intelligence System"

### Fixed
- **Fleet Roster Injection**: Fixed unbound EUXIS_DIR variable
- **JSON Extraction**: Proper handling when stdout is redirected
- **Security Vulnerabilities**: Hardened fleet scripts per architect review
- **Certify Pipeline**: Added timeout guard and deduplicated symlinks
- **Kaizen Findings**: Eliminated weak language, documented CLI tools

---

## [0.0.4] - 2025-12-15

### Added
- **Protocol v5.0**: Hybrid dispatch with conflict resolution
- **Protocol v4.5**: Intelligence tiering taxonomy with clean architecture
- **Cortex Semantic Memory**: Long-term knowledge storage and retrieval
- **Kaizen Self-Improvement Loop**: Continuous system optimization
- **Safe Daemon Mode**: Background operation with file-hash checks
- **Certification Suite**: Comprehensive validation pipeline
- **Golden Tests**: Baseline comparison for regression detection
- **User Guide**: Complete CLI reference documentation
- **Governance Loop**: Audit runner, bench, sync-docs integration
- **Librarian v4.5**: Enhanced knowledge management

### Changed
- **Architecture**: Restructured to clean architecture patterns
- **Agent Versions**: Synchronized all agents to v4.5
- **Provider Models**: Updated to claude-sonnet-4 and gemini-2.0-flash
- **Bash Scripts**: Hardened with strict error handling

### Fixed
- **Health Check**: Improved provider detection accuracy
- **Security Probe**: Fixed false positive in shell injection test
- **euxis-sync-docs**: Added ~/bin to PATH for resolution
- **Linux Compatibility**: Fixed stat flag order in euxis-lint

---

## [0.0.3] - 2025-12-01

### Added
- **Protocol v4.0**: Cortex semantic memory integration
- **Protocol v3.0**: ReAct loop with tiered memory and DAG orchestration
- **Reflexion Protocol**: Debate rounds for improved reasoning
- **Agent S Components**: Self-correction loop and memory distillation
- **SOTA Components**: Gym evals, Council protocol, Tool Belt
- **Marketing Division**: 3 new marketing-focused agents
- **Unit-Tester Agent**: Dedicated testing specialist
- **Compliance Agent**: Regulatory oversight
- **Globalization Agent**: Internationalization support
- **Observability Agent**: System monitoring and telemetry

### Changed
- **Quality Scores**: Achieved 100/100 on all quality criteria
- **euxis.sh Refactor**: Split monolithic function into discrete phases
- **Protocol Footer**: Upgraded to v2.0 format
- **Agent Prompts**: Enterprise-grade quality improvements

### Fixed
- **Protocol Version**: Synchronized across all components
- **Registry Sync**: Dynamic scoring alignment
- **Architect Audit**: Protocol v2.1 with section standardization

---

## [0.0.2] - 2025-11-15

### Added
- **4 Specialist Agents**: Domain-specific expertise roles
- **Protocol v2.0**: Enhanced agent communication format
- **Dynamic Discovery**: Automatic agent capability detection
- **Shared Protocol**: Unified communication layer
- **Delegation System**: Task routing between agents
- **README**: Initial documentation with architecture overview
- **Security Hardening**: Initial vulnerability mitigations

### Changed
- **Agent Prompts**: Upgraded to enterprise-grade quality
- **Framework Structure**: Improved organization and modularity

---

## [0.0.1] - 2025-11-01

### Added
- **Initial Framework**: Core Euxis infrastructure
- **Agent Discovery**: Dynamic agent loading system
- **Dispatch System**: Basic task routing to agents
- **Provider Integration**: Claude, Gemini, Ollama support
- **Configuration System**: YAML-based agent definitions
- **CLI Interface**: Basic command-line operations
- **Health Check**: System validation tooling

---

## Version Comparison Links

[Unreleased]: https://github.com/sebastienrousseau/euxis/compare/v0.0.7...HEAD
[0.0.7]: https://github.com/sebastienrousseau/euxis/compare/v0.0.6...v0.0.7
[0.0.6]: https://github.com/sebastienrousseau/euxis/compare/v0.0.5...v0.0.6
[0.0.5]: https://github.com/sebastienrousseau/euxis/compare/v0.0.4...v0.0.5
[0.0.4]: https://github.com/sebastienrousseau/euxis/compare/v0.0.3...v0.0.4
[0.0.3]: https://github.com/sebastienrousseau/euxis/compare/v0.0.2...v0.0.3
[0.0.2]: https://github.com/sebastienrousseau/euxis/compare/v0.0.1...v0.0.2
[0.0.1]: https://github.com/sebastienrousseau/euxis/releases/tag/v0.0.1
