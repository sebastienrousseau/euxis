# Euxis-Superior: Deep-Dive Audit & Architecture Roadmap

**Date:** 2026-03-01
**Author:** Lead AI Architect (Claude Opus 4.6)
**Scope:** Euxis vs. OpenClaw competitive audit + "Euxis-Superior" implementation roadmap

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [2026 Landscape Analysis](#2-2026-landscape-analysis)
3. [Competitive Audit: Euxis vs. OpenClaw](#3-competitive-audit-euxis-vs-openclaw)
4. [Architecture Plan: Euxis-Superior](#4-architecture-plan-euxis-superior)
5. [Validation & Benchmark Suite](#5-validation--benchmark-suite)
6. [Implementation Timeline](#6-implementation-timeline)

---

## 1. Executive Summary

Euxis is a **production-grade, security-first agentic platform** with 18+ packages, 42 specialized agents, WASM sandboxing, and a strict CI/CD pipeline requiring 100% code coverage. OpenClaw is a **viral consumer-facing personal AI agent** (180k+ GitHub stars) with a 3,286-skill marketplace (ClawHub) but catastrophic supply-chain security vulnerabilities (ClawHavoc: 20% malicious skills).

**Euxis's strategic position is strong** -- it already solves OpenClaw's worst problem (security) and has a working bridge layer. The gaps are in **community velocity, protocol adoption (A2A/MCP-I/ERC-8004), and edge inference**. This roadmap closes those gaps to establish Euxis-Superior as the reference implementation for secure, autonomous agent ecosystems.

### Key Findings

| Dimension | Euxis | OpenClaw | Winner |
|-----------|-------|----------|--------|
| Security | Zero-trust WASM + Ed25519 signing + boundary enforcement | Allow-by-default, 20% malicious skills (ClawHavoc) | **Euxis** |
| Skill Ecosystem | 42 built-in agents, 8 squads | 3,286 community skills (ClawHub) | **OpenClaw** (quantity) |
| Portability | Linux/macOS/WSL platform adapters | Any platform with Node.js | **Tie** |
| Protocol Support | MCP native | MCP native | **Tie** |
| A2A Protocol | Not implemented | Not implemented | **Neither** |
| Agent Identity (KYA) | Internal registry only | None | **Neither** |
| Long-Horizon Autonomy | Swarm playbooks, circuit breakers | Heartbeat proactive mode | **Euxis** |
| Performance Governance | P95 budgets, benchmark baselines | None | **Euxis** |
| Community Traction | Private/early-stage | 180k+ stars, 2M visitors/week | **OpenClaw** |
| CI/CD Rigor | 100% coverage, 8 GHA workflows, 76+ make targets | Basic CI | **Euxis** |
| Edge/Local Inference | Not implemented | Not implemented | **Neither** |
| Encrypted Memory | AES-256-GCM (Rust-backed euxis-crypto) | Plaintext | **Euxis** |

---

## 2. 2026 Landscape Analysis

### 2.1 Agent Communication Protocols

Four complementary protocols have emerged under open governance:

| Protocol | Focus | Transport | Governance |
|----------|-------|-----------|------------|
| **MCP** (Anthropic) | Agent-to-tool (vertical) | JSON-RPC 2.0 | Linux Foundation |
| **A2A** (Google) | Agent-to-agent (horizontal) | JSON-RPC / gRPC / HTTP+SSE | Linux Foundation (June 2025) |
| **ACP** (IBM) | Agent-to-agent brokered | REST + multipart MIME | IBM Research |
| **ANP** (Community) | Decentralized agent discovery | W3C DIDs + JSON-LD | Community |

**Euxis implication:** MCP is already supported. A2A is the critical gap -- Agent Cards for capability discovery and task lifecycle management are the standard for multi-agent coordination. ACP and ANP are secondary but worth monitoring.

### 2.2 KYA & Agent Identity

The KYA (Know Your Agent) paradigm has crystallized around three layers:

1. **DIDs** (W3C Decentralized Identifiers) for cryptographic agent identity
2. **Verifiable Credentials** for authorization scoping
3. **ERC-8004** (Ethereum mainnet, Jan 2026) for on-chain identity/reputation/validation

ERC-8004 attracted 30,000+ registrations in its first week. Its three registries (Identity, Reputation, Validation) map directly to Euxis's existing agent registry concept but add trustless, cross-platform verification.

**Euxis implication:** The existing `registry.json` with 42 agents is an internal-only registry. Extending it to support DID-based identity with ERC-8004 compatibility would give Euxis agents verifiable, portable, cross-ecosystem identities.

### 2.3 Long-Horizon Autonomy

METR benchmarks show task-completion time horizons doubling every ~4 months (accelerated from 7-month average). Current frontier: reliable completion of multi-hour tasks; multi-day autonomy remains aspirational.

**Euxis implication:** The swarm playbook system with circuit breakers, retry policies, and FinOps routing is well-positioned. The gaps are: checkpoint/resume for multi-day tasks, context compaction for long horizons, and progressive autonomy escalation.

### 2.4 Sparse Attention & Edge Inference

DeepSeek's NSA (Native Sparse Attention) won ACL 2025 Best Paper. MoE architectures are now standard for frontier models (DeepSeek-V3: 671B total / 37B active). Edge deployment achieves 75-96% of full-model accuracy at 4-10x smaller sizes.

**Euxis implication:** The WASM sandbox architecture is ideal for hosting quantized local models. Integration with GGUF/llama.cpp or similar for local fallback would eliminate cloud dependency for latency-sensitive or offline scenarios.

### 2.5 OpenClaw: Opportunity Window

OpenClaw's creator (Peter Steinberger) joined OpenAI on 2026-02-14. The project is transferring to an open-source foundation. Combined with ClawHavoc (800+ malicious skills discovered), this creates a **trust vacuum and leadership transition** -- the ideal window for Euxis to position as the secure, production-grade alternative.

---

## 3. Competitive Audit: Euxis vs. OpenClaw

### 3.1 Package-by-Package Comparison

#### Core Runtime

| Capability | Euxis Package | OpenClaw Equivalent | Euxis Advantage | Gap |
|------------|---------------|---------------------|-----------------|-----|
| Agent execution | `euxis-core` (Extism WASM) | Node.js runtime | WASM isolation, zero-copy msgpack | None |
| CLI orchestration | `euxis-cli` (76+ targets) | `openclaw` CLI | Richer command set, squad/playbook support | None |
| API gateway | `euxis-gateway` (FastAPI) | None (messaging-only) | HTTP/WS API, session mgmt | None |
| TUI monitoring | `euxis-tui` (Textual) | None | Real-time agent observability | None |
| Metrics/Observability | `euxis-metrics` | None | Delegation pattern analysis, profiling | None |

#### Security

| Capability | Euxis Package | OpenClaw Equivalent | Euxis Advantage | Gap |
|------------|---------------|---------------------|-----------------|-----|
| Encryption | `euxis-crypto` (Rust/PyO3, AES-256-GCM) | None (plaintext) | Native encrypted memory | None |
| Sandboxing | Extism WASM + boundary enforcement | None (unrestricted Node.js) | Zero-trust execution | None |
| Signing | Ed25519 detached signatures | None (GitHub-account-only publishing) | Supply chain integrity | None |
| Policy enforcement | `euxis-security` + `signed_exec.py` | None | Deny-by-default execution | None |
| Input validation | AST-based boundary checks | None | Architecture-level enforcement | None |

**Verdict:** Euxis has a generational security advantage. OpenClaw's ClawHavoc incident (20% malicious skills, RCE CVE-2026-25253) is the clearest evidence.

#### Skill Ecosystem

| Capability | Euxis | OpenClaw | Gap |
|------------|-------|----------|-----|
| Built-in agents | 42 (6 tiers) | Minimal built-in | Euxis leads |
| Community skills | 0 (private) | 3,286 (ClawHub) | **Critical gap** |
| Skill format | Python modules + WASM | `SKILL.md` + JS files | Different paradigm |
| Skill registry | `registry.json` (local) | clawhub.ai (hosted, searchable) | **Critical gap** |
| Skill search | None | Vector embeddings | **Gap** |

**Verdict:** OpenClaw wins on ecosystem breadth. The `euxis-bridge` already imports ClawHub skills, but Euxis needs its own hosted registry with supply-chain verification.

#### Memory & State

| Capability | Euxis | OpenClaw | Euxis Advantage |
|------------|-------|----------|-----------------|
| Memory tiers | 3-tier (hot/relevant/cross-agent) | Single persistent store | Sophistication |
| Encryption | AES-256-GCM | Plaintext | Security |
| Semantic drift | Contradiction detection + resolution | None | Intelligence |
| Pruning | Configurable (500 lines, 100 recent keep) | Manual | Automation |
| Cross-agent | Read-only sibling context (3 matches) | None | Multi-agent |

#### Messaging & Adapters

| Channel | Euxis (`euxis-adapters`) | OpenClaw | Status |
|---------|--------------------------|----------|--------|
| Telegram | Yes | Yes | Parity |
| WhatsApp | Yes | Yes | Parity |
| Discord | Yes | Yes | Parity |
| Slack | Yes | Yes | Parity |
| Signal | Yes (bridge-daemon) | Community skill | Euxis leads |
| HTTP/WS API | Yes (`euxis-gateway`) | No | Euxis leads |

#### Portability

| Platform | Euxis | OpenClaw |
|----------|-------|----------|
| Linux | Native (`platform/linux.py`) | Node.js |
| macOS | Native (`platform/macos.py`) | Node.js |
| WSL2 | Native (`platform/wsl.py`) | Node.js |
| Windows (native) | Not supported | Node.js |
| Docker | Not documented | Community images |
| Mobile | Not supported | Not supported |

### 3.2 Speed Comparison

| Metric | Euxis | OpenClaw | Analysis |
|--------|-------|----------|----------|
| Cold start | Near-zero (WASM pool) | Node.js startup (~200ms) | Euxis leads |
| Serialization | Zero-copy MessagePack | JSON | Euxis leads (3-5x) |
| Skill execution | WASM isolated + Python native | Node.js subprocess | Comparable |
| Bridge overhead | 20s timeout, subprocess spawn | N/A | Euxis bottleneck |
| P95 latency target | 150s (current) -> 45s (Q4 2026) | No governance | Euxis has roadmap |

### 3.3 Summary: Strategic Gaps to Close

| Priority | Gap | Impact | Effort |
|----------|-----|--------|--------|
| P0 | **A2A Protocol support** | Agent interoperability standard | High |
| P0 | **Hosted skill registry** (EuxisHub) | Community growth | High |
| P1 | **KYA/DID agent identity** | Cross-ecosystem trust | Medium |
| P1 | **Local inference fallback** (sparse attention models) | Offline/edge capability | High |
| P1 | **Checkpoint/resume** for long-horizon tasks | Multi-day autonomy | Medium |
| P2 | **ERC-8004 integration** | On-chain reputation | Medium |
| P2 | **Windows native support** | Platform coverage | Low |
| P2 | **Docker/OCI images** | Deployment simplicity | Low |

---

## 4. Architecture Plan: Euxis-Superior

### 4.1 Portability: Native Parity Across macOS, Linux, WSL2

**Current state:** Platform adapters exist in `euxis-cpp/euxis-core-cpp/` (C++23 modules) and `euxis-bridge/platform/` with `linux.py`, `macos.py`, `wsl.py` + factory pattern.

**Roadmap:**

#### Phase 1: Harden Existing Adapters (Q1 2026)
- **Audit all platform-specific code paths** via `check_boundaries.py` to ensure zero platform leakage into core logic
- **Unify service management:** systemd (Linux), launchd (macOS), Windows Service (WSL2 via interop)
- **Cross-platform path resolution:** Replace all hardcoded `/tmp`, `~/.config` with `platformdirs` library
- **Filesystem watcher parity:** inotify (Linux), FSEvents (macOS), ReadDirectoryChangesW (WSL2)

#### Phase 2: Container-First Deployment (Q2 2026)
```
euxis-superior/
  docker/
    Dockerfile.runtime       # Minimal Python 3.14 + Rust runtime
    Dockerfile.gateway       # FastAPI gateway with health checks
    Dockerfile.bridge        # Node.js 22 bridge worker
    docker-compose.yml       # Full stack orchestration
    devcontainer.json        # VS Code / Codespaces support
```
- **Multi-stage builds:** Rust compilation -> Python wheel -> slim runtime image
- **OCI annotations:** Labels for SBOM, signature verification, provenance
- **Rootless containers:** Run as non-root with capability dropping
- **ARM64 parity:** Native builds for Apple Silicon and AWS Graviton

#### Phase 3: Native Windows Support (Q3 2026)
- **`platform/windows.py`** adapter for native Windows (not just WSL)
- **Windows Service integration** for bridge-daemon
- **Named pipe transport** as alternative to Unix sockets
- **PowerShell integration** for euxis-cli

### 4.2 Security: Encrypted Memory + Sandboxed Execution

**Current state:** `euxis-crypto` provides AES-256-GCM + PBKDF2 via Rust/PyO3. Extism WASM provides sandbox isolation. `signed_exec.py` enforces Ed25519 signatures.

**Roadmap to solve OpenClaw's permission risks:**

#### Phase 1: KYA Identity Layer (Q1-Q2 2026)
```python
# New package: euxis-identity
@dataclass(frozen=True)
class AgentIdentity:
    did: str                          # did:euxis:<agent-id>
    public_key: Ed25519PublicKey
    verifiable_credentials: list[VC]  # Authorization scopes
    erc8004_token_id: int | None      # Optional on-chain anchor
    created_at: datetime
    attestations: list[Attestation]   # Third-party trust signals

class IdentityRegistry:
    """W3C DID-compatible agent identity registry."""
    def register(self, agent: AgentIdentity) -> DIDDocument: ...
    def resolve(self, did: str) -> AgentIdentity | None: ...
    def verify_credential(self, vc: VC) -> VerificationResult: ...
    def revoke(self, did: str, reason: str) -> None: ...
```

- **DID method `did:euxis`** for native agent identity
- **Verifiable Credentials** for scoped authorization (spending limits, API access, data permissions)
- **ERC-8004 bridge** for optional on-chain identity anchoring
- **MCP-I compatibility** for cross-ecosystem identity verification

#### Phase 2: Skill Supply Chain Security (Q2 2026)

Directly addresses ClawHavoc:

```python
class SkillVerificationPipeline:
    """Multi-stage skill verification before registry admission."""

    async def verify(self, skill: SkillSubmission) -> VerificationResult:
        checks = await asyncio.gather(
            self.static_analysis(skill),       # AST + semgrep rules
            self.dependency_audit(skill),       # Known vuln DB check
            self.sandbox_execution(skill),      # WASM sandbox dry-run
            self.signature_verification(skill), # Ed25519 author sig
            self.reputation_check(skill.author),# Author trust score
        )
        return VerificationResult(
            admitted=all(c.passed for c in checks),
            checks=checks,
            provenance=self.generate_provenance(skill, checks),
        )
```

- **Mandatory Ed25519 signing** for all skill submissions
- **Static analysis gate** (semgrep + custom rules for AMOS-style payloads)
- **WASM sandbox dry-run** before admission
- **Author reputation scoring** based on history + attestations
- **Provenance chain** with SLSA Level 3 compliance

#### Phase 3: Encrypted Agent Memory (Q2-Q3 2026)

Extend `euxis-crypto` to the full memory pipeline:

```python
class EncryptedMemoryStore:
    """AES-256-GCM encrypted memory with per-agent key isolation."""

    def __init__(self, agent_did: str, master_key: bytes):
        self.agent_key = derive_agent_key(master_key, agent_did)  # PBKDF2

    def store(self, entry: MemoryEntry) -> None:
        encrypted = aes_gcm_encrypt(
            key=self.agent_key,
            plaintext=msgpack.packb(entry),
            aad=entry.tier.encode(),  # Bind tier to ciphertext
        )
        self.backend.write(entry.id, encrypted)

    def recall(self, query: str, tier: MemoryTier) -> list[MemoryEntry]:
        # Decrypt only matching tier for efficiency
        ...
```

- **Per-agent key derivation** from master key + agent DID
- **Authenticated encryption** with tier-bound AAD (Additional Authenticated Data)
- **Memory export** encrypted with recipient's public key for cross-agent sharing
- **Secure deletion** with cryptographic erasure (key destruction)

### 4.3 Performance: Sparse Attention Local Fallback

**Current state:** Euxis routes to cloud LLM providers via the C++23 FinOps router (`euxis-cpp/euxis-core-cpp/`). No local inference capability.

**Roadmap:**

#### Phase 1: Local Model Runtime (Q2 2026)
```python
# New module: euxis-cpp/euxis-core-cpp/local_inference.cppm
class LocalModelRuntime:
    """GGUF-based local inference with sparse attention support."""

    def __init__(self, config: LocalModelConfig):
        self.engine = LlamaCppEngine(
            model_path=config.model_path,
            n_ctx=config.context_length,      # Up to 64k with NSA
            n_gpu_layers=config.gpu_layers,   # Auto-detect via platform
            flash_attn=True,                  # Enable flash attention
            type_k=QuantType.Q4_0,            # KV-cache quantization
        )

    async def generate(self, request: AgentExecutionRequest) -> str:
        """Generate with automatic cloud fallback on quality check failure."""
        local_result = await self.engine.generate(request.prompt)
        if self.quality_gate(local_result, request):
            return local_result
        # Fall through to cloud via FinOps router
        raise LocalQualityBelowThreshold(score=score)
```

- **llama.cpp integration** via `llama-cpp-python` bindings
- **GGUF model management** with automatic download + verification
- **Quality gate** to detect when local model output is insufficient -> cloud fallback
- **NSA-compatible models** (DeepSeek-R1 1.78-bit, Mistral variants) for edge deployment

#### Phase 2: FinOps Router Enhancement (Q3 2026)
```python
class EnhancedFinOpsRouter:
    """Extended router with local-first strategy."""

    def score_providers(self, request: AgentExecutionRequest) -> list[ScoredProvider]:
        providers = [
            self.score_local(request),    # Local GGUF model
            self.score_cloud(request),    # Cloud API providers
        ]
        # Scoring dimensions: cost, latency, quality, privacy, offline_capable
        return sorted(providers, key=lambda p: p.composite_score, reverse=True)
```

- **Local-first scoring** when offline or for privacy-sensitive tasks
- **Cost dimension** heavily favoring local (amortized GPU cost vs per-token cloud)
- **Privacy dimension** automatically routing PII-containing requests to local
- **Offline detection** with graceful degradation to local-only mode

#### Phase 3: Model Optimization Pipeline (Q4 2026)
- **Automated quantization** of fine-tuned models to GGUF
- **Speculative decoding** with small draft model + large verifier
- **KV-cache sharing** across agents in the same swarm session
- **Adaptive context compaction** for long-horizon tasks

### 4.4 Functionality: OpenClaw JS Skill Bridge

**Current state:** `euxis-bridge` imports ClawHub manifests and executes Node.js skills via `NodeSkillBridge` (20s timeout, env allowlist, subprocess isolation).

**Roadmap to wrap all OpenClaw JS skills into Euxis Python runtimes:**

#### Phase 1: Bridge Hardening (Q1 2026)
```python
class HardenedNodeBridge:
    """Enhanced Node.js skill bridge with WASM-level isolation."""

    async def execute(self, skill: BridgedSkill, payload: dict) -> SkillResult:
        # 1. Verify skill signature
        if not self.verify_signature(skill):
            raise UntrustedSkillError(skill.slug)

        # 2. Create isolated execution environment
        sandbox = await self.create_sandbox(
            skill=skill,
            filesystem=FilesystemPolicy.READ_ONLY,  # No writes
            network=NetworkPolicy.DENY_ALL,          # No network by default
            timeout=skill.metadata.get("timeout", 20),
            memory_limit_mb=256,
            env=self.filtered_env(skill),
        )

        # 3. Execute in sandbox
        result = await sandbox.run(payload)

        # 4. Validate output schema
        return self.validate_output(result, skill.output_schema)
```

- **nsjail/bubblewrap integration** for Linux namespace isolation (beyond subprocess)
- **Filesystem policy** defaulting to read-only with explicit write grants
- **Network policy** defaulting to deny-all with explicit allowlist
- **Memory limits** enforced via cgroups
- **Output schema validation** to prevent injection via return values

#### Phase 2: Automatic Skill Transpilation (Q2-Q3 2026)
```python
class SkillTranspiler:
    """Convert OpenClaw JS skills to native Euxis Python skills."""

    def transpile(self, js_skill: BridgedSkill) -> PythonSkill:
        # 1. Analyze JS skill for complexity tier
        tier = self.classify(js_skill)

        if tier == SkillTier.SIMPLE_HTTP:
            # Pure HTTP skills -> httpx equivalent
            return self.transpile_http(js_skill)
        elif tier == SkillTier.BROWSER_AUTOMATION:
            # Playwright JS -> Playwright Python
            return self.transpile_playwright(js_skill)
        elif tier == SkillTier.DATA_PROCESSING:
            # JS data manipulation -> Python equivalent
            return self.transpile_data(js_skill)
        else:
            # Complex skills stay as bridge-wrapped
            return self.wrap_as_bridge(js_skill)
```

- **Tier classification** of JS skills by complexity and dependencies
- **Automatic transpilation** for simple patterns (HTTP calls, data transforms)
- **Playwright JS->Python** mapping for browser automation skills
- **Native wrapping** for complex skills that resist transpilation
- **Performance comparison** benchmarks: bridge vs. native for each skill

#### Phase 3: Bidirectional Bridge (Q3 2026)
```python
class BidirectionalBridge:
    """Allow Euxis Python skills to be consumed by OpenClaw."""

    def export_as_openclaw(self, euxis_skill: EuxisSkill) -> SkillManifest:
        """Generate SKILL.md + wrapper for ClawHub publishing."""
        return SkillManifest(
            skill_md=self.generate_skill_md(euxis_skill),
            wrapper_js=self.generate_node_wrapper(euxis_skill),
            openclaw_json=self.generate_manifest(euxis_skill),
        )
```

- **Export Euxis skills to ClawHub format** for cross-ecosystem reach
- **Node.js wrapper generation** that calls Euxis Python skills via subprocess
- **Metadata preservation** across both directions
- **ClawHub publishing automation** with signature verification

### 4.5 A2A Protocol Integration

**New package: `euxis-a2a`**

```python
# euxis-a2a/src/euxis_a2a/agent_card.py
@dataclass(frozen=True)
class EuxisAgentCard:
    """A2A Agent Card for Euxis agent capability discovery."""
    name: str
    description: str
    capabilities: list[Capability]
    protocols: list[str]           # ["a2a/1.0", "mcp/1.0"]
    endpoint: str                  # Agent's A2A endpoint
    authentication: AuthSchema     # Supported auth methods
    identity: AgentIdentity | None # KYA/DID identity

class A2AServer:
    """A2A protocol server for receiving inter-agent requests."""

    async def handle_task(self, task: A2ATask) -> A2ATask:
        # 1. Verify requesting agent's identity
        # 2. Check authorization via verifiable credentials
        # 3. Route to appropriate Euxis agent/squad
        # 4. Stream progress via SSE
        # 5. Return artifacts
        ...

class A2AClient:
    """A2A protocol client for discovering and invoking remote agents."""

    async def discover(self, endpoint: str) -> EuxisAgentCard:
        """Fetch remote agent's Agent Card."""
        ...

    async def invoke(self, agent: EuxisAgentCard, task: A2ATask) -> A2ATask:
        """Create a task on a remote agent and track to completion."""
        ...
```

- **Agent Card generation** from existing `registry.json` agent definitions
- **A2A server** integrated into `euxis-gateway` for receiving inter-agent requests
- **A2A client** in `euxis-core` for discovering and invoking external agents
- **SSE streaming** for long-running task progress
- **KYA identity binding** for authenticated agent-to-agent communication

---

## 5. Validation & Benchmark Suite

### 5.1 Benchmark Categories

#### Category 1: Security Benchmark (ClawHavoc Prevention)
```python
class SecurityBenchmark:
    """Validates Euxis resists attacks that compromised OpenClaw."""

    tests = [
        # Supply chain attacks
        "malicious_skill_detection_rate",       # Target: >99.5% (vs OpenClaw 0%)
        "rce_payload_blocking",                 # Target: 100%
        "env_exfiltration_prevention",          # Target: 100%
        "amos_stealer_pattern_detection",       # Target: 100%

        # Sandbox escapes
        "wasm_sandbox_escape_resistance",       # Target: 0 escapes in 10k attempts
        "node_bridge_env_leakage",              # Target: 0 vars leaked beyond allowlist
        "filesystem_write_outside_sandbox",     # Target: 0 successful writes
        "network_access_without_grant",         # Target: 0 successful connections

        # Identity & auth
        "unsigned_skill_execution_blocked",     # Target: 100% blocked
        "revoked_credential_rejection",         # Target: 100% rejected
        "cross_agent_memory_isolation",         # Target: 0 cross-reads without grant
    ]
```

#### Category 2: Autonomy Benchmark (METR-Compatible)
```python
class AutonomyBenchmark:
    """Validates long-horizon task completion against 2026 METR standards."""

    tasks = [
        # Tier 1: Minutes (baseline, should be 100%)
        {"description": "File search and summarization", "human_time_minutes": 5},
        {"description": "API endpoint implementation", "human_time_minutes": 15},
        {"description": "Bug diagnosis from stack trace", "human_time_minutes": 10},

        # Tier 2: Hours (target: >80% completion)
        {"description": "Full feature implementation with tests", "human_time_minutes": 120},
        {"description": "Multi-file refactoring", "human_time_minutes": 90},
        {"description": "Security audit of a module", "human_time_minutes": 60},

        # Tier 3: Extended (target: >50% completion, stretch)
        {"description": "Cross-package architectural migration", "human_time_minutes": 480},
        {"description": "Full integration test suite creation", "human_time_minutes": 360},
    ]

    metrics = [
        "completion_rate_by_tier",
        "time_to_completion_vs_human",
        "checkpoint_resume_success_rate",
        "context_compaction_quality",        # Semantic preservation after compaction
        "autonomous_error_recovery_rate",
    ]
```

#### Category 3: Performance Benchmark
```python
class PerformanceBenchmark:
    """Validates latency and throughput against P95 governance targets."""

    scenarios = [
        # Cold start
        {"name": "wasm_cold_start", "target_p95_ms": 50},
        {"name": "python_skill_cold_start", "target_p95_ms": 100},
        {"name": "node_bridge_cold_start", "target_p95_ms": 500},

        # Warm execution
        {"name": "wasm_warm_execution", "target_p95_ms": 10},
        {"name": "python_skill_warm", "target_p95_ms": 50},
        {"name": "node_bridge_warm", "target_p95_ms": 200},

        # Serialization
        {"name": "msgpack_roundtrip_1kb", "target_p95_ms": 0.1},
        {"name": "msgpack_roundtrip_1mb", "target_p95_ms": 5},

        # Memory
        {"name": "encrypted_memory_write", "target_p95_ms": 2},
        {"name": "encrypted_memory_read_hot_tier", "target_p95_ms": 1},
        {"name": "memory_recall_relevant_tier", "target_p95_ms": 10},

        # Local inference
        {"name": "local_inference_7b_q4", "target_tokens_per_sec": 30},
        {"name": "local_inference_14b_q4", "target_tokens_per_sec": 15},
        {"name": "cloud_fallback_latency", "target_p95_ms": 2000},
    ]
```

#### Category 4: Portability Benchmark
```python
class PortabilityBenchmark:
    """Validates identical behavior across Linux, macOS, and WSL2."""

    platforms = ["linux-x86_64", "linux-aarch64", "macos-arm64", "wsl2-x86_64"]

    tests = [
        "platform_adapter_feature_parity",    # All adapters expose same interface
        "filesystem_watcher_consistency",      # Events fire identically
        "service_management_parity",           # Start/stop/status consistent
        "path_resolution_consistency",         # No hardcoded paths
        "crypto_output_determinism",           # Same plaintext -> same ciphertext (given same key+nonce)
        "docker_image_functional_parity",      # Identical behavior in container
    ]
```

#### Category 5: Interoperability Benchmark
```python
class InteroperabilityBenchmark:
    """Validates A2A, MCP, and bridge compatibility."""

    tests = [
        # A2A Protocol
        "agent_card_generation_valid",          # Agent Card passes A2A schema validation
        "task_lifecycle_complete",              # Create -> progress -> complete
        "sse_streaming_reliability",            # No dropped events over 1000 tasks
        "cross_framework_discovery",            # Euxis agent discoverable by LangGraph/CrewAI

        # MCP
        "mcp_tool_registration",               # All Euxis tools visible via MCP
        "mcp_resource_exposure",               # Memory/state accessible via MCP resources
        "mcp_prompt_templates",                # Agent prompts available as MCP prompts

        # Bridge
        "clawhub_import_coverage",             # % of top-100 ClawHub skills importable
        "bridge_execution_success_rate",       # % of imported skills that execute correctly
        "transpilation_accuracy",              # Transpiled skills produce identical output
        "bidirectional_roundtrip",             # Euxis -> ClawHub -> Euxis preserves semantics
    ]
```

### 5.2 Benchmark Infrastructure

```
euxis-ops/benchmarks/
  runner.py                    # Orchestrates all benchmark suites
  reporters/
    json_reporter.py           # Machine-readable results
    markdown_reporter.py       # Human-readable reports
    regression_detector.py     # Flags >20% regressions vs baseline
  baselines/
    2026-q1-baseline.json      # Frozen baseline for comparison
  ci_integration.py            # GitHub Actions integration
```

- **CI gate:** All benchmarks run on every PR; regressions >20% block merge
- **Nightly full suite:** Extended benchmarks including long-horizon autonomy tests
- **Cross-platform matrix:** Linux x86_64, Linux aarch64, macOS arm64, WSL2
- **Public dashboard:** Benchmark results published to `euxis-docs` site

---

## 6. Implementation Timeline

### Q1 2026 (Now - March 31)

| Week | Deliverable | Packages Touched |
|------|-------------|------------------|
| 1-2 | Platform adapter audit + hardening | `euxis-core`, `euxis-bridge` |
| 2-3 | `euxis-identity` package skeleton + DID method | New: `euxis-identity` |
| 3-4 | Bridge hardening (nsjail, filesystem/network policy) | `euxis-bridge` |
| 4-6 | Security benchmark suite (ClawHavoc scenarios) | `euxis-ops` |
| 5-8 | A2A Agent Card generation from registry | New: `euxis-a2a` |
| 6-8 | Portability benchmark suite | `euxis-ops` |

### Q2 2026 (April - June)

| Week | Deliverable | Packages Touched |
|------|-------------|------------------|
| 1-4 | A2A server + client implementation | `euxis-a2a`, `euxis-gateway` |
| 2-5 | Local model runtime (llama.cpp integration) | `euxis-core` |
| 3-6 | Skill supply chain verification pipeline | `euxis-bridge`, `euxis-security` |
| 4-7 | KYA Verifiable Credentials + ERC-8004 bridge | `euxis-identity` |
| 5-8 | Enhanced FinOps router (local-first scoring) | `euxis-core` |
| 6-8 | Autonomy benchmark suite (METR-compatible) | `euxis-ops` |
| 7-8 | Docker/OCI images + devcontainer | New: `docker/` |

### Q3 2026 (July - September)

| Week | Deliverable | Packages Touched |
|------|-------------|------------------|
| 1-4 | Automatic skill transpilation (JS -> Python) | `euxis-bridge` |
| 2-5 | Checkpoint/resume for long-horizon tasks | `euxis-core` |
| 3-6 | Bidirectional bridge (Euxis -> ClawHub export) | `euxis-bridge` |
| 4-7 | EuxisHub hosted skill registry (MVP) | New: `euxis-hub` |
| 5-8 | Windows native platform adapter | `euxis-core` |
| 7-8 | Full interoperability benchmark suite | `euxis-ops` |

### Q4 2026 (October - December)

| Week | Deliverable | Packages Touched |
|------|-------------|------------------|
| 1-4 | Model optimization pipeline (quantization, speculative decoding) | `euxis-core` |
| 2-5 | EuxisHub public launch + skill migration tools | `euxis-hub` |
| 3-6 | Context compaction for multi-day autonomy | `euxis-core` |
| 4-7 | ACP/ANP protocol support (secondary protocols) | `euxis-a2a` |
| 6-8 | Euxis-Superior 1.0 release + public benchmarks | All packages |

### Success Criteria for Euxis-Superior 1.0

| Metric | Target |
|--------|--------|
| Security benchmark score | >99.5% malicious skill detection |
| P95 latency (warm execution) | <45s (per existing Q4 target) |
| Cross-platform test parity | 100% identical results |
| A2A interop | Discoverable by LangGraph, CrewAI, AutoGen |
| ClawHub skill import rate | >80% of top-100 skills |
| METR autonomy tier 2 completion | >80% |
| Local inference throughput (7B Q4) | >30 tok/s |
| EuxisHub skills (launch) | >500 verified skills |

---

## Appendix A: New Package Dependency Graph

```
                    euxis-identity (NEW)
                   /        |         \
                  /         |          \
    euxis-a2a (NEW)    euxis-crypto    euxis-core
         |                  |              |
         |                  |              |
    euxis-gateway      euxis-bridge   euxis-cpp/euxis-core-cpp/local_inference (NEW)
         |                  |
         |                  |
    euxis-hub (NEW)    euxis-security
```

## Appendix B: Risk Register

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| A2A spec instability (still v0.2) | Medium | High | Abstract behind protocol adapter; support both JSON-RPC and gRPC bindings |
| ERC-8004 adoption stalls | Medium | Low | Keep on-chain identity optional; DID-only path always works |
| llama.cpp API breaking changes | High | Medium | Pin to stable release; abstract behind `LocalModelRuntime` interface |
| ClawHub API changes post-foundation-transfer | High | Medium | Bridge already normalizes to internal format; manifest parsing is resilient |
| WASM cold start regression with larger plugins | Low | Medium | Plugin pool warm-up + precompilation cache |
| Cross-platform CI matrix cost | Medium | Low | Run full matrix nightly; PR builds on primary platform only |

## Appendix C: Key External References

- [A2A Protocol Specification](https://a2a-protocol.org/latest/specification/)
- [ERC-8004: Trustless Agents](https://eips.ethereum.org/EIPS/eip-8004)
- [METR Time Horizons](https://metr.org/time-horizons/)
- [DeepSeek NSA Paper](https://arxiv.org/abs/2502.11089)
- [ClawHavoc Disclosure](https://thehackernews.com/2026/02/researchers-find-341-malicious-clawhub.html)
- [Agent Protocol Survey](https://arxiv.org/html/2505.02279v1)
- [MCP Specification](https://spec.modelcontextprotocol.io/)
