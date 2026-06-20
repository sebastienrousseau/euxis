# Euxis Fleet Guide

**Enterprise Unified eXecution Intelligence System**

Version v0.1.3

---

## Fleet Overview

53 agents organized into two tiers (core and fleet) with distinct authority and operational rules. Each agent has a defined scope, optimal provider routing, and clear escalation paths.

### Core (12): Authority-bearing, always present

Twelve core agents define direction and may block progress. If one is missing, the system is incomplete.

| Agent | What It Does | Model Tier |
|-------|-------------|------------|
| `orchestrator` | Breaks down ambitious goals. Routes work to specialists. Delivers complete solutions. | Strategic |
| `architect` | Designs systems that scale. Creates patterns that last. | Strategic |
| `planner` | Defines intent, scope, and prioritization. Ships what matters. | Strategic |
| `reviewer` | Truth & quality gate. Guarantees correctness and completeness. | Strategic |
| `librarian` | Keeps knowledge sharp. Documentation governance. | Utility |
| `auditor` | Legal, privacy, and regulatory authority. Ships with confidence. | Strategic |
| `critic` | Challenges assumptions. Surfaces hidden risks. Pre-mortems. | Strategic |
| `arbiter` | Conflict resolution. Final decisions when agents disagree. | Strategic |
| `historian` | Long-term memory. Temporal patterns. Institutional knowledge. | Utility |
| `route` | Session routing. Maps inbound events to agents and squads. | Strategic |
| `pair` | Channel onboarding. Auth flows and device pairing. | Utility |
| `guard` | Execution approvals and audit trail enforcement. | Strategic |

### Fleet (41): Task-triggered specialists

All non-core agents live in the fleet tier. The groupings below are usage guidance only and not enforced by the registry.

| Agent | What It Does | When to Use It |
|-------|--------------|----------------|
| `accountant` | Analyzes cloud costs, finds waste, recommends right-sizing. Tracks spend across services and suggests budget allocations. | "Why did our AWS bill spike?" or "Optimize our infrastructure costs" |
| `animator` | Creates smooth transitions, designs color systems, implements dark mode. Brings interfaces to life with purposeful motion. | "Add page transitions" or "Create a cohesive color palette" |
| `automaton` | Builds CI/CD pipelines, writes Dockerfiles, configures Terraform. Automates everything between commit and production. | "Set up GitHub Actions" or "Create a deployment pipeline" |
| `automator` | Automates repetitive ops and build workflows. Eliminates manual toil at scale. | "Automate our nightly validation" or "Generate release artifacts" |
| `bridge` | Translates message formats across channels. Preserves intent and media. | "Route Slack threads into Telegram" or "Translate Discord blocks to SMS" |
| `compliance` | Policy, audit, and regulatory compliance checks. Ensures evidence and control alignment. | "Check SOC2 controls" or "Audit our data handling" |
| `debugger` | Traces bugs to root cause. Reads stack traces, reproduces issues, and delivers surgical fixes with minimal side effects. | "Fix the null pointer in auth.py" or "Why does login fail on Safari?" |
| `designer` | Creates responsive layouts, builds component libraries, ensures visual consistency. Thinks in design systems, not one-off pages. | "Design a settings page" or "Create a reusable card component" |
| `gatekeeper` | Manages releases end-to-end. Writes changelogs, bumps versions, coordinates deployments, and ensures nothing ships broken. | "Prepare the v2.1 release" or "What changed since last release?" |
| `heal` | Detects failures and recovers automatically. Keeps agents and channels alive. | "Auto-recover from agent timeouts" or "Handle dropped channel connections" |
| `inspector` | Runs comprehensive E2E test suites. Validates user flows work correctly across browsers and devices before shipping. | "Verify the checkout flow" or "Run full regression suite" |
| `interactor` | Implements keyboard shortcuts, focus management, and input handling. Makes interfaces feel responsive and accessible. | "Add keyboard navigation" or "Implement vim-style bindings" |
| `investigator` | Performs post-mortem analysis on crashes and outages. Reconstructs timelines, identifies contributing factors, prevents recurrence. | "Why did production crash at 3am?" or "Analyze this crash dump" |
| `maintainer` | Upgrades dependencies, migrates APIs, modernizes legacy code. Makes breaking changes without breaking things. | "Upgrade to React 19" or "Migrate from REST to GraphQL" |
| `optimizer` | Profiles performance bottlenecks. Reduces latency, improves throughput, cuts memory usage. Makes slow code fast. | "Speed up the dashboard" or "Reduce API response time" |
| `pentester` | Finds vulnerabilities before attackers do. Tests for injection, XSS, CSRF, and authentication bypasses. | "Security audit the login flow" or "Test for SQL injection" |
| `polyglot` | Enforces language-specific best practices. Catches Go-isms in Python, ensures idiomatic Rust, validates TypeScript patterns. | "Review this Go code" or "Is this idiomatic Python?" |
| `repairer` | Applies known fixes automatically. Patches common issues, updates deprecated APIs, fixes linter warnings at scale. | "Fix all ESLint errors" or "Update deprecated function calls" |
| `responder` | Handles production incidents. Triages alerts, coordinates response, communicates status, drives resolution. | "Production is down" or "Handle this PagerDuty alert" |
| `sentinel` | Defines and enforces security policies. Reviews access controls, validates compliance, governs threat response. | "Review our security policies" or "Audit IAM permissions" |
| `tactician` | Designs terminal interfaces. Creates intuitive TUI layouts with proper keyboard navigation and visual hierarchy. | "Design a CLI dashboard" or "Improve the TUI experience" |
| `telemetrist` | Implements observability. Adds structured logging, configures metrics, builds dashboards, sets up alerting. | "Add logging to the API" or "Create a Grafana dashboard" |
| `tester` | Writes and maintains tests. Ensures coverage, prevents regressions, catches bugs before they ship. | "Add tests for the auth module" or "Improve test coverage" |
| `trace` | Captures end-to-end agent decisions, timings, and outcomes. | "Trace a multi-agent workflow" or "Debug routing latency" |
| `watchdog` | Performs exhaustive pre-merge analysis. Catches regressions, validates behavior changes, blocks risky merges. | "Review this PR thoroughly" or "Check for regressions" |
| `writer` | Creates documentation that developers actually read. Writes tutorials, API references, and guides that answer real questions. | "Document the API" or "Write a getting started guide" |

### Fleet usage group: On-demand (guidance only)

These agents handle specialized growth and communication tasks. They add capability: not safety gates. Call them when you need their specific expertise.

| Agent | What It Does | When to Use It |
|-------|--------------|----------------|
| `ambassador` | Builds developer community. Creates tutorials, writes blog posts, prepares conference talks, engages on forums. | "Write a tutorial for our API" or "Prepare a conference talk" |
| `butler` | Summarizes for voice and audio. Converts technical output into clear, spoken English optimized for TTS systems. | "Summarize this for voice output" or "Create an audio briefing" |
| `distill` | Compresses long context without losing critical decisions. | "Summarize this 200-message thread" |
| `evangelist` | Crafts brand voice and visual identity. Tells compelling stories about your product. Makes technical work resonate. | "Write launch announcement copy" or "Define our brand voice" |
| `govern` | Defines and enforces agent capability boundaries and escalation rules. | "Define escalation policies for security work" |
| `localizer` | Adapts products for global markets. Handles i18n, l10n, RTL layouts, Unicode validation, and cultural adaptation. | "Localize for Japanese market" or "Add RTL support" |
| `marketer` | Drives growth through SEO, funnel optimization, and go-to-market strategy. Turns features into user acquisition. | "Improve our SEO" or "Plan the product launch" |
| `deep-researcher` | Produces CTO-grade strategic briefs. Synthesizes academic, industry, and standards evidence with citations. | "Produce a strategic brief on AI system modularity" |
| `researcher` | Conducts deep research with cross-validation. Compares options, benchmarks alternatives, synthesizes findings. Uses Gemini's large context. | "Compare React vs Vue vs Svelte" or "Research PDF parsing libraries" |
| `strategist` | Plans content calendars, manages social presence, builds community engagement. Amplifies your technical work. | "Plan Q1 content strategy" or "Grow our Twitter presence" |
| `social` | Community and social channel operations. Creates concise updates and engagement-ready posts. | "Draft a release tweet" or "Summarize this update for Discord" |

### Fleet usage group: Specialist (guidance only)

These agents have deep expertise in specialized domains. Activate them only when working within their specific area: they know things general agents don't.

| Agent | What It Does | When to Use It |
|-------|--------------|----------------|
| `cryptographer` | Audits cryptographic implementations. Validates key management, reviews hashing schemes, ensures PQC readiness, catches subtle crypto bugs that break security. | "Review our encryption implementation" or "Audit the key derivation" |
| `ledger` | Handles financial systems and payments. Validates ISO 20022 compliance, reviews payment schemas, ensures regulatory correctness for money movement. | "Validate SWIFT message format" or "Review payment flow compliance" |
| `conduit` | Optimizes real-time audio pipelines. Manages latency budgets, implements DSP algorithms, builds voice processing chains that work under strict timing constraints. | "Reduce audio latency" or "Implement noise cancellation" |
| `custodian` | Publishes Rust crates correctly. Manages MSRV policies, writes idiomatic Rust docs, ensures crate ecosystem best practices and semver compliance. | "Prepare crate for publishing" or "Review Rust API design" |

**For the authoritative agent registry, governance rules, and authority model, see [CONSTITUTION.md](../CONSTITUTION.md).**

---

## Dispatch Modes

Scale your team. Coordinate your way.

### Hierarchical

Central command for complex coordination.

```bash
euxis-dispatch manifest.json
```

Perfect for intricate projects requiring tight synchronization.

### Mesh

Specialists coordinate directly.

```bash
euxis-dispatch --mode mesh manifest.json
```

Dispatch-enabled: `architect`, `inspector`, `responder`, `gatekeeper`

Best when domain experts know their workflows better than central command.

### Federated

Autonomous execution across boundaries.

```bash
euxis-dispatch --mode federated manifest.json
```

For cross-project work requiring independent operation.

### Dispatch Manifest

Every dispatch starts with a manifest. Here is the format:

```json
{
  "project": "auth-service",
  "mode": "hierarchical",
  "dispatches": [
    {
      "agent": "pentester",
      "priority": "P0",
      "task": "Scan for injection vulnerabilities",
      "verify_cmd": "security-scan --critical-only"
    }
  ]
}
```

---

## Memory-Driven Operation

Agents use the Cortex for persistent, tri-typed memory across sessions. Before starting complex tasks, agents recall proven patterns and contraindications to avoid repeating failed approaches.

**For complete memory system documentation, commands, and type classification, see [User Guide](user-guide.md#tri-typed-memory-system).**

---

## Intelligence Routing

Euxis automatically routes agents to optimal AI providers based on task complexity and domain requirements. Critical (P0) tasks always use strategic-tier providers for maximum reliability.

**For the complete provider matrix, intelligence tiering, and explicit override syntax, see [User Guide](user-guide.md#ai-provider-matrix).**

---

## Quality Gates

Every agent applies three verification layers before delivering output.

### Layer 1: Internal Consistency
Every claim backed by evidence. No contradictions.

### Layer 2: Cross-Reference
Validate against stored knowledge. Surface conflicts.

### Layer 3: Review Checkpoint
The `reviewer` validates all multi-agent output.

### Learn from Failure
```
REFLEXION: Root cause analysis
EVIDENCE: Specific failure indicator
STRATEGY: Different approach for retry
CONTRAINDICATION: Never repeat this mistake
```

Store as procedural memory. Get better every time.

---

## Conflict Resolution

When specialists disagree, Euxis resolves it systematically.

### Resolution Order
1. **Domain Priority**: Security beats performance. Correctness beats speed.
2. **Evidence Weight**: Verified data beats inference.
3. **Negotiation**: One round. Both agents state positions.
4. **Human Escalation**: Present evidence. Let you decide.

### Conflict Response Format
```json
{
  "agent": "pentester",
  "position": "Reject this change",
  "evidence": "SQL injection in line 42",
  "confidence": "HIGH",
  "concession": "Fix injection, then approve"
}
```

One round. No endless debate.

---

## Delegation Patterns

Route work to the right specialist every time.

| Task | Lead | Support |
|------|------|---------|
| Security audit | `sentinel` | `pentester`, `auditor` |
| Bug investigation | `debugger` | `telemetrist` |
| Architecture design | `architect` | `planner` |
| Release coordination | `gatekeeper` | `inspector`, `writer` |
| Performance optimization | `optimizer` | `architect` |
| Documentation | `writer` | `librarian` |
| Research | `researcher` | `architect` |
| Incident response | `responder` | `debugger`, `pentester` |
| Test strategy | `inspector` | `tester`, `pentester` |
| Cryptographic audit | `cryptographer` | `pentester`, `reviewer` |
| Payments integration | `ledger` | `auditor`, `tester` |
| Audio pipeline | `conduit` | `optimizer`, `automaton` |
| Rust crate release | `custodian` | `tester`, `pentester` |

---

## Common Workflows

### Security Pipeline
```
sentinel -> pentester -> debugger -> tester -> inspector -> reviewer
```

### Release Pipeline
```
inspector -> gatekeeper -> writer -> reviewer
```

### Incident Response
```
responder -> debugger + pentester -> architect -> inspector
```

### Research to Implementation
```
researcher -> architect -> planner -> debugger -> reviewer
```

---

## Squads

Squads are cross-functional teams designed for specific types of work. Each squad has a lead agent (runs at P0 priority) and supporting members (P1 priority). Deploy a squad when you need coordinated expertise rather than a single perspective.

### When to Use Squads

Use a squad when your task needs multiple viewpoints working together. A single agent gives you depth in one area. A squad gives you breadth across a domain.

| Squad | What It's For | Example Tasks |
|-------|---------------|---------------|
| **Vision** | Strategic planning and direction | "Plan the product roadmap" · "Design system architecture" · "Research market opportunity" |
| **Build** | Implementation and maintenance | "Fix all critical bugs" · "Upgrade the codebase to v2" · "Set up CI/CD from scratch" |
| **Quality** | Assurance and verification | "Full security audit" · "Pre-launch checklist" · "Code review the entire module" |
| **Growth** | Marketing and community | "Launch announcement campaign" · "Create developer docs" · "Build community presence" |
| **Experience** | User interface excellence | "Redesign the dashboard" · "Implement design system" · "Improve accessibility" |
| **Specialist** | Domain-specific expertise | "Cryptographic audit" · "Payments integration" · "Audio pipeline optimization" |

### Squad Composition

| Squad | Lead | Members | Total |
|-------|------|---------|-------|
| Vision | `orchestrator` | architect, planner, researcher, historian, accountant | 6 |
| Build | `debugger` | maintainer, automaton, tester, investigator, repairer | 6 |
| Quality | `reviewer` | inspector, pentester, auditor, optimizer, watchdog, polyglot, arbiter | 8 |
| Growth | `writer` | evangelist, strategist, ambassador, marketer, localizer | 6 |
| Experience | `designer` | tactician, animator, interactor | 4 |
| Specialist | `cryptographer` | ledger, conduit, custodian | 4 |

### Squad Commands

```bash
euxis-squad list                                # See all squads
euxis-squad info build                          # View squad details and members
euxis-squad deploy quality "Audit auth module"  # Deploy the entire squad
euxis-squad members vision                      # List members with their roles
euxis-squad validate                            # Verify all squad members exist in registry
```

**How it works:** When you deploy a squad, Euxis generates a dispatch manifest with all squad members and executes them in parallel. The lead agent coordinates the work and synthesizes the final output.

---

## Playbooks

Playbooks are multi-phase workflows that coordinate squads in sequence. Each phase completes before the next begins. Checkpoints gate progression. If a critical phase fails, the playbook stops.

### When to Use Playbooks

Use a playbook when your work has distinct phases that must happen in order. Playbooks are ideal for repeatable processes where you want consistent execution every time.

| Playbook | What It Does | When to Use It |
|----------|--------------|----------------|
| **Zero to One** | Takes a feature from concept to shipped. Vision defines it, Build implements it, Quality verifies it, Growth announces it. | Launching a new product or major feature |
| **Legacy Overhaul** | Modernizes existing systems safely. Build upgrades the code, Quality ensures nothing broke, Vision validates alignment with goals. | Upgrading frameworks, migrating databases |
| **Red Alert** | Emergency response protocol. Quality triages the issue, Build fixes it, Vision ensures the fix aligns with architecture. | Production outages, critical security issues |
| **Verify Everything** | Comprehensive 6-gate verification pipeline. Runs every check in sequence with no shortcuts. | Pre-release validation, compliance audits |
| **Crypto Audit** | Deep cryptographic security review. Models threats, audits implementations, verifies correctness. | Before launching payment features or handling sensitive data |
| **Payments Integration** | ISO 20022 compliance pipeline. Validates schemas, tests integration, coordinates release. | Adding payment processing, SWIFT integration |
| **Rust Release** | Crate publishing pipeline. Audits code, runs security checks, prepares for crates.io publication. | Publishing or updating Rust crates |
| **Audio Pipeline** | Real-time audio optimization. Audits latency, optimizes DSP, verifies timing constraints. | Voice features, audio processing systems |

### How Playbooks Work

```
Phase 1: Vision Squad → generates manifest → executes → checkpoint ✓
    ↓
Phase 2: Build Squad → generates manifest → executes → checkpoint ✓
    ↓
Phase 3: Quality Squad → generates manifest → executes → checkpoint ✓
    ↓
Complete
```

Each phase:
1. Generates a dispatch manifest with the relevant squad
2. Executes all agents in parallel
3. Validates output at a checkpoint
4. Proceeds to the next phase (or halts on failure)

### Playbook Commands

```bash
euxis-playbook list                                             # See available playbooks
euxis-playbook info zero-to-one                                 # View phase breakdown
euxis-playbook run zero-to-one "Launch auth service" --dry-run  # Preview without executing
euxis-playbook run red-alert "Production DB outage"             # Execute emergency response
euxis-playbook status                                           # View session history
```

**Dry run first:** Always preview with `--dry-run` to see exactly what will execute before committing to a full playbook run.

---

## Combos

Combos are lightweight agent chains. No manifests. No phases. Just agents running in sequence, each building on the previous output.

### When to Use Combos

Use a combo when you want a focused pipeline with a predictable flow. Combos are faster than playbooks and simpler than squads: perfect for well-defined tasks that benefit from multiple perspectives in sequence.

### How Combos Work

```
Task: "Design onboarding flow"
    ↓
Agent 1 (deep-researcher): Establishes evidence-backed context → output
    ↓
Agent 2 (planner): Defines scope and requirements → output
    ↓
Agent 3 (architect): Designs the structure → output
    ↓
Agent 4 (evangelist): Polishes the narrative → output
    ↓
Agent 5 (reviewer): Validates and approves → final output
```

Each agent receives:
- Your original task
- The previous agent's output (up to 4000 characters)

### Available Combos

| Combo | What It Does | The Chain | Best For |
|-------|--------------|-----------|----------|
| **Envision** | Product narrative aligned with architecture and ready for launch review. | deep-researcher → planner → architect → evangelist → reviewer | Product strategy, launch briefs, roadmap narratives |
| **Protect** | Security posture hardened with evidence-backed assurance. | sentinel → pentester → auditor → inspector → reviewer | Security-critical features, audit readiness |
| **Craft** | End-to-end content creation with consistent voice. | writer → strategist → evangelist → reviewer | Blog posts, documentation, messaging |
| **Refine** | Interface refined from system to motion and interaction. | designer → animator → interactor → reviewer | UI components, design systems, UX polish |
| **Seal** | Deep cryptographic audit with policy-backed validation. | sentinel → cryptographer → pentester → reviewer | Encryption features, key management |
| **Settle** | Payments integration validation with compliance and tests. | ledger → auditor → tester → reviewer | Payment processing, ISO 20022 |
| **Inspect** | Exhaustive pre-merge analysis and regression hardening. | researcher → polyglot → watchdog → pentester → reviewer | High-risk PRs, critical merges |
| **Discover** | Evidence-backed architecture roadmap and decisions. | deep-researcher → architect → reviewer | Platform decisions, modernization plans |

### Combo Commands

```bash
euxis combo list                                              # See available combos
euxis combo info protect                                    # View the chain
euxis combo run envision "Design onboarding flow"           # Execute the combo
euxis combo run protect "Audit payment module" --provider claude  # Override provider for all agents
```

### Squads vs. Playbooks vs. Combos

| Pattern | Agents | Execution | Best For |
|---------|--------|-----------|----------|
| **Squad** | Multiple agents | Parallel | Broad coverage, multiple perspectives at once |
| **Playbook** | Multiple squads | Sequential phases | Complex multi-stage workflows |
| **Combo** | Few agents | Sequential chain | Focused pipelines, iterative refinement |

---

## Usage

### Single Agent
```bash
euxis <agent> "<task>"
```

### Orchestrated Work
```bash
euxis orchestrator "<complex goal>"
```

### Parallel Dispatch
```bash
euxis-dispatch manifest.json
```

### Adversarial Review
```bash
euxis-council "<decision>"
```

### Autonomous Retry
```bash
euxis-loop <agent> "<task>" "<verify_cmd>"
```

### Codex Templates
```bash
euxis-codex list                                                    # Browse templates
euxis-codex run xray CODEX_CODEBASE_PATH=./src CODEX_ENTRY_POINT=main.py  # Execute
```

### Git Hooks
```bash
euxis-hooks install                     # Brand every commit automatically
euxis-hooks status                      # Check installation
```

---

*Euxis v0.1.3 · Build something that matters.*