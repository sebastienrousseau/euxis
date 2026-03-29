# Changelog

All notable changes to Euxis will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **CLI Surface Layer**: Verb-first Core command group (`check`, `triage`, `review`, `compare`, `stats`, `policy`) as the primary user-facing interface
- **Certification Readiness** (`certify-readiness`): 18-domain certification assessment with framework overlays (general, SOC2, ISO 27001), 5 hard gates, quality risk analysis, and structured JSON artifacts
- **Provider Strategy Routing**: Semantic task classification with 11 task classes routing to optimal providers (OpenAI for research, Claude for coding, Gemini for security, Ollama for local). Configuration via `data/config/provider_strategy.json`
- **Forensic Mode Routing**: `route_forensic()` with per-agent overrides — opus for critical agents (architect, strategist, investigator, sentinel, reviewer), sonnet for support agents, gemini-pro for research agents
- **Benchmark Suite**: Deterministic `--stats` and `--compare` test fixtures with synthetic verdict history (12 entries, 5 compare tests, 7 stats tests)
- **Command Aliases**: `quick`→triage, `deep`→review, `diag`→doctor, `metrics`→stats, `pb`→playbook
- **Shell Completions**: Tab completion for Bash, Zsh, and Fish (`data/config/completions/`)
- **Help Smoke Tests**: 9 snapshot-like tests verifying help output stability for all Core commands
- **Policy Command**: Standalone policy inspection (`show`), validation (`validate`), and enforcement (`check`) without requiring a full playbook run
- **Lifecycle Commands**: `install`, `update`, `upgrade`, `uninstall`, `self` for installation management

### Changed
- **Directory Restructuring**: `cmd/`→`apps/`, `pkg/`→`libs/`, `internal/platform/`→`libs/platform/`, `build/cmake/`→`cmake/`
- **Agent Tier Promotions**: sentinel, strategist, and investigator promoted from fleet to core tier for Reason-class model routing
- **Version**: Bumped to v0.0.10 across CMakeLists.txt, engine, lifecycle, and TUI
- **Help Output**: "Core" group now appears first in `euxis --help`, before System/Fleet/Knowledge groups
- **README**: Rewritten with accurate C++23 build instructions, provider strategy overview, and complete Core commands table
- **CONTRIBUTING.md**: Updated for C++23 workflow (CMake/GCC prerequisites, `make cpp-test`, AGPL-3.0 license, commit signing required)
- **Documentation**: User guide, quick start, and CLI reference updated to use Core commands as primary examples; all stale `cmd/`/`pkg/` path references updated
- **Test Suite**: 1115 tests (up from 961), all passing
- **CI Workflows**: Fixed stale artifact paths, added `cpp-bench` and `cpp-clang-tidy` Makefile targets, bumped release refs to v0.0.10
- **TEMPLATE_CONFORMANCE**: Updated all 16 test path references from `euxis-cpp/` to `apps/`/`libs/`
- **Data Config Versions**: Bumped `registry.json`, `squads.json`, `router.json`, `capabilities.json`, `codex.json` from v0.0.3 to v0.0.10
- **Documentation**: Updated ~40 doc files from v0.0.4 footers/badges to v0.0.10, fixed `euxis-cpp/` monorepo paths to `apps/`/`libs/`

### Security
- **CWE-78 Mitigation**: `shell_interactive()` rejects shell metacharacters (`$(`, `` ` ``, `;`, `&&`, `||`, `|`, `>>`, `<<`) with 12 new tests
- **SOUP Register**: Formal Software of Unknown Provenance classification for all 9 dependencies per IEC 62304 (`data/docs/compliance/soup-classification.md`)
- **Software Validation Procedure**: ISO 13485 SVP with traceability matrix, release checklist, and non-conformance handling (`data/docs/compliance/software-validation-procedure.md`)
- **SAST**: `.clang-tidy` config with security-focused checks (bugprone-*, cert-*, concurrency-*, use-after-move as error)
- **Credential Exclusion**: `.gitignore` expanded with `.env*`, `*.key`, `*.pem`, `credentials*`, `*.p12`, `*.pfx`, `*.keystore`

### Fixed
- **Security Hardening**: Auth profile store bounds checks, path traversal guards, command injection prevention, bare `catch(...)` elimination
- **Circular Dependency**: Broke crypto↔identity circular include
- **Dependency Vulnerabilities**: Patched flatted (CVE-2026-32141), PyJWT (CVE-2026-32597), removed `/tmp` fallback paths
- **Custodian Task Class**: Fixed routing hint from "security" to "coding" (observability agent)
- **PolicyViolationBadVerdict Test**: Fixed false pass caused by mock execution producing TRUSTED verdict that satisfied the `min_verdict` gate

---

## [0.0.9] - 2026-02-16

### Changed
- **Dependency Security**: Updated `cryptography` to address Dependabot-reported vulnerabilities

### Fixed
- **Release Hygiene**: Documented issue closure status and release summary artifacts

### Security
- **Dependency Patching**: Resolved 3 high and 2 moderate vulnerability alerts in the default branch

---

## [0.0.8] - 2026-02-17

### Added
- **Multi-Tier Model Routing**: Intelligent cost optimization with capability-to-model mapping (`router.sh`)
- **ClawRouter Task Analysis**: Automatic task complexity classification for optimal model selection
- **Local Model Fallback**: Ollama integration for zero-cost routine operations (heartbeats, status checks)
- **Prompt Caching Support**: Cache control hints for 90% API cost reduction on long prompts
- **Benchmark Tool**: `euxis-bench` for cost estimation and multi-tier performance validation
- **TUI Router Transparency**: Color-coded cost tier indicators on agent cards ($=Routine, $$$$=Reason)
- **TUI Throttle UX**: Visual "Throttled" and "Cooling" states when Resource Guard triggers
- **TUI Audit Mode**: `python -m tui --audit` for performance benchmarking and cross-platform checks
- **TUI Router Shortcut**: `Ctrl+R` shows router status in notification
- **A2A Mesh Communication Protocol**: Peer-to-peer agent discovery, shared state sync, and hand-off with deadlock detection (`mesh.sh`)
- **Hardware-Aware Resource Throttling**: Automatic CPU/RAM/load detection with thermal-aware staggered agent launches (`resources.sh`)
- **Agent Bootstrap Wrapper**: Mesh-mode agent launcher with capability registration and cleanup handling
- **Concurrent Write Safety**: flock-based locking for state.json preventing data corruption under parallel writes
- **Process Priority Management**: `nice -n 15` scheduling for UI-friendly background agent execution
- **Orphan Process Cleanup**: SIGTERM→SIGKILL cascade on Ctrl+C with PID tracking
- **Runtime Peer Discovery**: `mesh_discover_runtime()` finds active agents by capability tag
- **Shared Research Log**: Append-only log for cross-agent collaboration without context bloat
- **Gateway Protocol Architecture**: Complete gateway server implementation with WebSocket support and multi-adapter framework
- **Voice Streaming Pipeline**: Real-time STT/TTS hooks with streaming WebSocket delivery and webhook payloads
- **Gateway Authentication System**: Hardened auth with token validation, rate limiting, and secure session management
- **Voice Command Safety**: Command filtering, audio validation, and safety hooks for voice interactions
- **Gateway Canvas UI**: Interactive canvas system with gesture support and real-time collaboration
- **Gateway Adapters**: Slack and Telegram adapter implementations with session management
- **Gateway CLI Tools**: Complete CLI toolkit for gateway management and testing
- **Gateway JSON Schemas**: Comprehensive schema validation for request/response/event frames
- **Gateway Protocol Validation**: Test harness and smoke testing suite for protocol compliance
- **Documentation Overhaul**: AI-ready `llms.txt` file and comprehensive gateway documentation
- **New Core Agents**: `guard`, `pair`, `route` for specialized coordination and safety
- **New Fleet Agents**: `bridge`, `deep-researcher`, `distill`, `govern`, `heal`, `trace` for enhanced capabilities
- **Crypto Library Enhancements**: Improved error handling, key management, and performance optimizations
- **TUI Performance Improvements**: Enhanced fleet monitoring, tips system, and configuration management
- **Benchmark Infrastructure**: TUI performance benchmarking and comprehensive test coverage

### Changed
- **Repository Reorganization**: Modular `euxis-*` directory structure for distribution readiness
- **Dispatch Mode Integration**: Mesh mode now uses bootstrap wrapper instead of prompt-only coordination
- **Agent Bootstrap Enhancement**: Added router integration for automatic model selection before agent launch
- **Cost Tier Configuration**: Added `router.json` for customizable model mappings and cost estimates
- **Dynamic Load Thresholds**: Changed from static per-core limits to `nproc × 0.8` formula
- **Memory Threshold**: Lowered to 75% for macOS swap safety (was 85%)
- **Version Alignment**: All 102 files synchronized to registry.json as single source of truth
- **Voice Retention System**: Enhanced audio processing with configurable retention policies
- **Gateway Event Handling**: Streamlined event processing with improved error recovery
- **Authentication Flow**: Simplified handshake process with better security controls
- **Documentation Architecture**: Restructured gateway docs with progressive disclosure
- **Agent Escalation Paths**: Enhanced inter-agent communication and conflict resolution
- **Performance Metrics**: Improved collectors and analyzers for better observability
- **Test Infrastructure**: Expanded test coverage with property-based and edge case testing

### Fixed
- **Concurrent State Corruption**: flock locking prevents race conditions in mesh state writes
- **euxis-loop Not Found**: Added PATH setup to dispatch and playbook scripts
- **Hyphenated Agent IDs**: jq `setpath/getpath` handles keys with special characters
- **Zombie Processes**: Proper cleanup on interrupt with PID tracking and process group termination
- **Gateway Auth Vulnerabilities**: Hardened token validation and session security
- **Voice Command Processing**: Fixed audio stream handling and command safety
- **TUI Layout Issues**: Resolved fleet monitor grid spacing and responsive design
- **Crypto Operations**: Fixed error handling in key management and encryption operations
- **Performance Bottlenecks**: Optimized collectors and reduced memory overhead
- **Documentation Links**: Fixed cross-references throughout gateway documentation
- **Test Reliability**: Stabilized flaky tests and improved test isolation

### Security
- **Gateway Security Hardening**: Input validation, rate limiting, and secure defaults
- **Voice Command Safety**: Audio validation and command filtering to prevent abuse
- **Authentication Improvements**: Token expiration, secure session handling, and audit logging
- **Crypto Library Security**: Enhanced key management and secure random generation

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
- **JSON Registry Format**: SQLite is now the primary storage; JSON support will be removed in a future release

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

[Unreleased]: https://github.com/sebastienrousseau/euxis/compare/v0.0.9...HEAD
[0.0.9]: https://github.com/sebastienrousseau/euxis/compare/v0.0.8...v0.0.9
[0.0.8]: https://github.com/sebastienrousseau/euxis/compare/v0.0.7...v0.0.8
[0.0.7]: https://github.com/sebastienrousseau/euxis/compare/v0.0.6...v0.0.7
[0.0.6]: https://github.com/sebastienrousseau/euxis/compare/v0.0.5...v0.0.6
[0.0.5]: https://github.com/sebastienrousseau/euxis/compare/v0.0.4...v0.0.5
[0.0.4]: https://github.com/sebastienrousseau/euxis/compare/v0.0.3...v0.0.4
[0.0.3]: https://github.com/sebastienrousseau/euxis/compare/v0.0.2...v0.0.3
[0.0.2]: https://github.com/sebastienrousseau/euxis/compare/v0.0.1...v0.0.2
[0.0.1]: https://github.com/sebastienrousseau/euxis/releases/tag/v0.0.1
