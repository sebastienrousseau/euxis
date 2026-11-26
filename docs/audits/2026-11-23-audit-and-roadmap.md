<!-- SPDX-License-Identifier: AGPL-3.0-only -->

# Euxis: audit and roadmap, 2026-11-23

- **Scope:** `main` at `efbc5c7`, about 114k lines of C++ (28 libs, 4 apps).
- **Method:** four read-only reviews running in parallel: architecture, performance, security, and product/DX/market. Critical findings were then spot-checked against the source.
- **Not done:** nothing was built, run, profiled or fuzzed. Each finding records how confident we are in it.

**Confidence tags.** Each finding carries one of these:

| Tag | Meaning |
|---|---|
| **V** | We re-read the code ourselves. |
| **H** | The reviewer read the whole code path. |
| **M** | Likely, but it depends on runtime behaviour. |
| **L** | Plausible, but needs confirming. |

One reviewer claim turned out to be false: that `Process::run` waits on the child twice. We dropped it.

---

## Summary

Euxis has an unusual and defensible position: a single local binary that both finds security problems and *proves* compliance, using evidence packs an auditor can verify offline. No large competitor offers that. Every one of them keeps the evidence in its own cloud.

Today, though, several things the product promises don't work:

1. **Offline verification (critical).** `euxis verify` trusts the public key embedded in the bundle itself (`apps/cli/src/cmd/attest.cpp:378`, V). Anyone can sign a forged bundle with their own key and get "signature OK" and exit code 0. Verification is the product's core promise.
2. **The CI contract.**
   - **Exit codes.** `check`, `triage` and `review` go through `cmd_playbook`, which ignores the documented exit-code scheme (`fleet.cpp:2757-2777`, V).
   - **Flags.** Unknown flags are silently dropped (`surface.cpp:35-68`, V).
   - **Integrations.** The shipped GitHub Action passes flags that don't exist (`action.yml:132-135`, V). The GitLab template and two of the four pre-commit hooks are broken in the same way.
3. **Scanning runs the scanned repo's code.** Euxis auto-runs `cargo test`, `npm test` and `go test`, or the repo's own `.euxis.yaml` `verify:` commands (`fleet.cpp:525+`, V). There is no opt-out. Running a scanner on fork PRs is therefore arbitrary code execution.
4. **Detection quality.** The rule engine and taint analysis do substring matching on every node in the code graph (CPG). So one hit is reported on every enclosing node, flows are over- and under-reported, sink locations are whole functions, and fingerprints are unstable (`rule_engine.cpp:176-184`, V).
5. **Claims ahead of the code.** The README promises:
   - "SOC 2, ISO 27001, NIST … fourteen other frameworks". Only the CWE Top 25 and OWASP Top 10 taxonomies ship.
   - "Sigstore-signed". Bundles are self-signed Ed25519, with no Fulcio certificate or Rekor transparency log.
   - SARIF and `--fix` on `check`. Neither works.

**Recommendation:** before adding features, spend one milestone, about 4–6 weeks, on trust. Make verification and the CI contract honest and correct, stop executing scanned code by default, and fix the matching semantics. Every commercial pillar depends on a verifier that verifies, and that is a security product's credibility. After that, the moat is the evidence pipeline, not the analysis engine. Chasing Semgrep or CodeQL on detection depth is a poor return on effort. Becoming the offline, signed evidence layer that sits on top of any scanner, theirs included, is a good one.

---

## Phase 1: Context and the 2026–2027 horizon

### 1.1 What euxis is

| | |
|---|---|
| **What it does** | Runs in CI. It scans code (static analysis, taint flows, dependencies, SBOM generation, hallucinated-package detection), optionally uses LLM agents and a model router that picks cheap or expensive models by task, and emits a signed evidence pack (in-toto/SLSA attestation, DSSE signature, SARIF, CycloneDX/SPDX SBOMs, OpenVEX) mapped to compliance frameworks. |
| **Audience** | Platform, AppSec and GRC teams that need continuous, auditor-verifiable compliance evidence. EU-regulated buyers (CRA, DORA) are a strong fit. Also embedders, through the C++23 SDK. |
| **Stack** | C++23, CMake/FetchContent, tree-sitter (8 grammars), libsodium/OpenSSL, nlohmann/json, SQLite, cpp-httplib, IXWebSocket, spdlog, Qt (apps/etx). Shipped as static musl Linux binaries plus macOS, .deb/.rpm, Docker and Homebrew. AGPL-3.0. |

### 1.2 Trends shaping 2026–2027

Sources are listed in Appendix B; items marked "unverified" are uncertain.

- **Regulation is turning into deadlines.**
  - EU CRA Article 14 reporting (24-hour early warning on actively exploited vulnerabilities) has applied since 11 Sep 2026, through ENISA's Single Reporting Platform.
  - Full CRA obligations apply from 11 Dec 2027.
  - CISA's 2026 SBOM Minimum Elements (29 Jul 2026) add component hash, licence, tool name and generation context.
  - In the US, EO 14306 (Jun 2025) removed the requirement to submit secure-development (SSDF) attestations to CISA. Whether the OMB memos still bind agencies is unverified.
- **Signed provenance is now the default in package ecosystems.**
  - SLSA v1.2 added a Source Track.
  - PyPI attestations are generally available, Maven Central validates `.sigstore.json`, and npm trusted publishing emits provenance.
  - Buyers will expect Fulcio identities and Rekor inclusion proofs, not bare keys.
- **Scanner supply-chain attacks are real.** The March 2026 Trivy compromise (CVE-2026-33634) involved force-pushed action tags and poisoned images. Pinning by SHA and hermetic, signed releases now matter to buyers.
- **Agentic AppSec.** Semgrep Assistant, Snyk Evo, Copilot Autofix and Checkmarx Assist all triage and fix with LLMs. The differentiators are explainable findings, reachability, and fixes as PRs.
- **AI-generated code risks.** Slopsquatting, where an attacker registers a package name that LLMs hallucinate, is measured: 19.7% of 2.23M LLM package suggestions were hallucinated, and 43% of those repeat across runs.
- **Agent protocols.** A2A moved to the Linux Foundation and reached v1.0 around March 2026, with breaking changes and an `A2A-Version` header. Euxis implements v0.2. MCP is also a Linux Foundation project.

### 1.3 Competitive baseline: what best-in-class requires

| Capability | Who sets the bar | Euxis today |
|---|---|---|
| Structural rules (metavariables, taint mode) and a large rule library | Semgrep / Opengrep | Substring literals; 5 example rules |
| Interprocedural, cross-file dataflow | CodeQL | Per file only; shallow CPG |
| Reachability-aware dependency scanning | Endor Labs, Socket (Coana) | `libs/reach` exists; 12 tests; not wired into `vulndb` |
| Malicious-package threat feed | Socket | Slopsquatting heuristics only |
| Diff/PR scanning, baselines, suppressions | All of them | None |
| SARIF fidelity (codeFlows, security-severity, suppressions) | GHAS | Partial and wrong in places |
| IDE/LSP | Snyk, Semgrep, GHAS | One-shot JSON only |
| Auto-fix PRs | Copilot Autofix, Snyk, Semgrep | `--fix` advertised, not wired |
| Continuous compliance evidence (HR, cloud, ticketing) | Vanta, Drata | Code-side only |
| **Offline-verifiable signed evidence** | **Nobody** | **Designed, but verification is broken** |
| **Single local binary, local LLMs (Ollama)** | **Nobody at this scope** | **Yes** |

**Where euxis can win:** offline signed evidence for regulated and EU buyers, local-first operation and EU sovereignty (Aikido showed an EU AppSec company can reach a $1B valuation), and hermetic signed releases. **Where it shouldn't compete head-on:** detection-engine depth against CodeQL and Semgrep. Ingest their SARIF instead (see Phase 3, M3).

---

## Phase 2: Codebase audit

### 2.1 Architecture and feature completeness

#### Structure

- **[Medium, H] Several libraries ship in no product.** `a2a`, `a2a-types`, `identity`, `inference`, `memory` and `adapters` are not included by the cli, gateway or publisher apps. `apps/etx` links four of them but includes none of their headers. Most of `libs/core` (SwarmOrchestrator, Supervisor, delegation) is used only by tests. The README's claim that "the same libraries the CLI uses ship as a public SDK" is false for these.
- **[Medium, H] Apps link other apps.** `euxis-cli-lib` links `euxis-gateway-cpp` (`apps/cli/CMakeLists.txt:71-91`), and etx links the CLI library. Shared logic belongs in libs.
- **[Medium, M] `libs/runtime` is a god module**: agent loop, sessions, config, lifecycle, tool registry and platform. `libs/platform` depends on it only for a header, so there are two platform abstractions.
- **[Low, H] Layering inversion.** `libs/network` (transport) depends on `a2a-types` (protocol).
- **[Medium, H] Duplicated code.**
  - SHA-256 is implemented 6 times, even though `libs/crypto` exists.
  - The same hash fingerprinting is copied 3 times.
  - Language detection exists twice.
  - There are two routers (FinOpsRouter and ProviderRouter) and two publishers.
- **[High, H] One function holds most of the CLI.** `cmd_playbook` (`fleet.cpp`) is 1,566 lines with cyclomatic complexity 438. check, triage and review all go through it, and it is where the CI-contract bugs live (section 2.4).

#### Correctness and detection quality

- **[High, V] The rule engine reports one hit on every enclosing node.** Each rule is tested as a substring against every CPG node's text (`rule_engine.cpp:176-184`). One `AKIA` becomes about 5 findings (identifier, string, statement, function, root), with no deduplication. There are no metavariables, structural matching or regex.
- **[High, H] Taint analysis does the same.** Every enclosing node of a sink or sanitizer string is tagged, and the ancestor walk (`analyzer.cpp:117-125`) reports a flow whenever a source sits in a function that contains a sink anywhere. One sanitizer string anywhere suppresses the flow. The result is large false-positive and false-negative rates, and the sink location is a whole function.
- **[High, V] Deterministic taint findings are labelled `RuleSource::LlmInferred`** (`analyzer.cpp:254`), so SARIF provenance is wrong.
- **[High, H] Unstable fingerprints.**
  - Taint passes `start_byte` as the "line", so any edit above a finding changes its fingerprint.
  - Rule fingerprints use `node.name`, which is empty for non-identifier nodes, so separate hits collide.
  - Baselines are therefore impossible.
- **[High, H] Rule loading fails open.**
  - An unknown rule language (e.g. `csharp`) is dropped, which leaves the language list empty, which means the rule runs on every language.
  - A malformed pattern is swallowed by `catch(...)` and becomes an empty literal.
  - An empty `pattern-not` evaluates to true.
- **[High, H] The scan cache changes results.**
  - A cache hit round-trips only some fields (`scan.cpp:259-285`), losing taint paths, related locations, fixes and reachability annotations. Cold and warm runs differ, which contradicts "same bytes, same digest".
  - A corrupt cache entry returns empty findings and is counted as a cache hit.
  - The ruleset hash ignores nested patterns, severity and languages, so editing those doesn't invalidate the cache.
- **[Medium, V] Hard-coded versions.** "0.0.3" appears in the cache key and the SARIF driver (`scan.cpp:413,476,723`), "0.1.0" in the SLSA builder version, and "0.0.3" in etx About. The project is at 0.0.16. The v0.0.16 bump (#197) was a text replace that missed all of these.
- **[High, H] `SwarmOrchestrator` is a stub.** "localhost" anywhere in the URL turns on simulation mode. Real mode connects, sends nothing, and records "accepted" as completed. Retry and circuit-breaker objects are built but never used.
- **[Medium, H] Release builds lose their bounds checks.** 46 `assert`s in shipped code (swarm, the AES-GCM bound) are compiled out by NDEBUG.
- **[Low, H] Mixed error handling.** About 237 `std::expected`, 37 `throw`, 26 `catch(...)`, and 109 `getenv` calls used as implicit global config.

#### Missing functionality, and docs against reality

| Claim or need | Reality | Severity |
|---|---|---|
| 18+ frameworks (README:25) | CWE Top 25 and OWASP Top 10 taxonomies only | High (V) |
| "Sigstore-signed … verifiable by any sigstore tool" | Self-signed Ed25519; no Fulcio/Rekor (`bundle.hpp:15`: "Rekor is a follow-up") | High |
| Pack "bundled with SARIF + SBOMs + OpenVEX" | `attest` signs one file, with a random invocationId; nothing binds the parts; not reproducible | Medium |
| Languages | C, C++, Rust, Go, Python, JS/JSX, TS, Java. **`.tsx` silently skipped** (V). No C#, PHP, Ruby, Kotlin, Swift, IaC or Dockerfile | High for the market |
| Baselines and suppressions | None (`--baseline`, inline ignore) | High |
| Diff/PR scanning | None (content-hash cache only) | High |
| SARIF fidelity | Taint hops in `locations` rather than `codeFlows`; no `security-severity`, `suppressions`, `baselineState`, relative URIs or UTF-16 columns | High |
| LSP | One-shot JSON; ranges line-only | Medium |
| Policy as code | SecurityPolicy is a fixed set of booleans | Medium |
| Secrets | Literal token match in parsed source only; no entropy; no config or dotfiles | Medium |
| A2A | v0.2 with non-spec method names; the spec is at v1.0 | Low |
| Memory-store migration | `migrate_legacy_entries()` is never called | Medium |

#### Recent PRs (#192–#204)

| PR | Risk | Notes |
|---|---|---|
| #192 remove toolchain PPA | Low | CI only |
| #193 portable static Linux + CA-bundle probe | Low | Well tested across 5 distros under QEMU. `SSL_CERT_FILE` leaks to child CLIs; no warning when no bundle is found |
| #194 XChaCha20 fix | Medium | AEAD is correct (random 24-byte nonce, AAD = tier, buffers wiped). Migration path never called; AES-GCM bounds are assert-only |
| #195 hermetic skips | Low | Fork-isolated seccomp tests are good. Every sandbox test can skip silently and no CI job checks that they ran |
| #196 ixwebsocket musl DNS | Low | Patch is idempotent. Dependency still fetched by tag |
| #197/#198 v0.0.16 release | Medium | Text-replace bump missed hard-coded versions. Fix: one CMake-generated `version.hpp` |
| #199 `.invalid` hosts | Low | Correct. Tests cover a stub |
| #200 untrack settings | Low | Check git history for secrets |
| #201 dependency floors | Low | Correct; build artefacts removed before merge |
| #202 complexity ratchet | Low | Sound. Keys are lizard `long_name`, so a signature change re-flags a function; CSV columns are hard-coded to lizard 1.26.1. Make it a required check |
| #203 pip bump | Low | Trivial |
| #204 stripped and split-debug releases | Low | Correct order (strip, then sha256, then cosign). The macOS TLS test covers only the HTTP path, not WebSocket. .deb/.rpm are built separately and are neither static nor stripped, which is inconsistent |

### 2.2 Performance, concurrency and resources

Nothing was profiled. The biggest wins come from the algorithms, not from micro-optimisation.

| # | Finding | Sev | Conf | Fix |
|---|---|---|---|---|
| P1 | `Graph::children()` and `parent()` scan every edge (`graph.cpp:70-90`, V). They are called per node in the DDG/CFG builders, taint, the call graph and the rule engine, so cost is O(N·E) or worse | Critical | V | Parent array plus a compact (CSR) child index built at insert time, making lookups O(1) (snippet in M2) |
| P2 | Taint BFS is O(sources·V·E) and copies the whole path on every push (`analyzer.cpp:155-166`) | Critical | H | Out-edge index; predecessor array; bitset of visited nodes; cap on flows per source |
| P3 | Substring search over the full subtree text, per node and per pattern. The root's text is the whole file, so cost is about bytes·depth·patterns | High | V | One Aho-Corasick or Hyperscan pass per file, then map each hit to its deepest enclosing node using pre-order ranges |
| P4 | Recursive AST walks; musl threads default to about 128 KiB stack; no stack size is set. Deep or minified JS can crash a worker thread | High | H | Iterative `TSTreeCursor` walk; `-Wl,-z,stack-size=8388608`; a deep-nesting test fixture |
| P5 | Parser: 3 copies per file, no size cap, silent truncation above 4 GiB (`uint32_t` cast), no tree-sitter timeout (V) | High | V | Read once into a pre-sized buffer or mmap; skip files above about 4 MiB (configurable); `ts_parser_set_timeout_micros` |
| P6 | All workers share one SQLite connection opened with `FULLMUTEX` (V). Statements are prepared on every call, every put commits on its own, there's no `busy_timeout`, and `SQLITE_BUSY` is ignored | High | V | Cached statements; read connections per thread; batch puts in one transaction after merging; `busy_timeout=5000` |
| P7 | OSV enrichment makes N sequential `/v1/query` calls, each with a new TLS client, no cache, and aborts on the first failure | High | H | `/v1/querybatch` (1,000 per call); a bounded pool for `/v1/vulns/{id}`; keep-alive; SQLite cache with a time limit |
| P8 | `Process::run_with_input` writes all of stdin before draining output, so above about 64 KiB of input both sides block forever, with no deadline | High | H | Poll stdin, stdout and stderr together with non-blocking I/O |
| P9 | Every file is read twice (hash, then parse), even on a cache hit; no size+mtime+inode fast path | Medium | H | Read once; check `(size, mtime_ns, ino)` before hashing |
| P10 | Static round-robin chunking in the scan pool, so one large file leaves other cores idle | Medium | H | Shared atomic work index; sort by descending size |
| P11 | Per-node `std::string` allocations in the CPG; specs rebuilt and a parser created for every file | Medium | H | Store the symbol ID rather than a string; per-file memory arena; thread-local parser per language |
| P12 | `setenv` and seccomp setup run between `fork` and `exec` in a multi-threaded process, which can deadlock on the malloc lock; the child is polled every 50 ms with `usleep` | Medium | H | `posix_spawn` with a prebuilt environment; `pidfd_open` + `poll` |
| P13 | Fleet's wall-clock "HARD CAP" is not enforced while every slot is busy (`cv.wait` has no timeout) | Medium | H | `wait_until(deadline)`; kill the agent processes |
| P14 | Data races: Telegram `session_meta_`, a `static mt19937` shared between threads in RetryPolicy, `EncryptedMemoryStore::entries()` mutating from const, non-atomic JsonCache stats, the `s_tool_cache` global | Medium | H | Locks, thread-local RNG, atomics; a TSan CI job |
| P15 | WebSocket client has a single response slot with no request id; the gateway keeps clients that disconnected with an error; sends while holding a lock | Medium | H | Map of request id to promise; `setMaxMessageSize`; erase on Error |
| P16 | `std::regex` compiled per call, including user-supplied PII patterns (a ReDoS risk, and recursion that can overflow a musl stack) | Medium | H | RE2 for user patterns; ctre for fixed ones |
| P17 | musl's default malloc under multi-threaded allocation | Medium | M | Link mimalloc statically in `EUXIS_STATIC_BINARY` builds |
| P18 | **No benchmark of the real scan path, and no regression gate.** Bencher skips parse, cpg, taint, reach, scan and cli; has no thresholds on PRs; and uses `bencherdev/bencher@main` unpinned. The "45 s / 3 min" README claims have no measurements behind them | High | H | A benchmark per stage plus an end-to-end run on a pinned corpus; Bencher on PRs with a threshold and `--err` |
| P19 | No `--gc-sections` linking; no PGO or BOLT | Low | H | Add them in release builds once P18 provides a profile |

### 2.3 Security, hardening and compliance

**Critical**

- **[S1, V] `euxis verify` trusts the key embedded in the bundle** (`attest.cpp:378`). `verify_envelope` also never checks:
  - that `payloadType` is in-toto
  - the `keyid`
  - that the subject digest matches the artifact
  - transparency-log entries
  - the certificate chain

  It prints the unverified hint as "keyid". The README's offline-verify example uses exactly this mode.

**High**

- **[S2, V] Path traversal and absolute-path writes in the gateway.** POST `/sessions` takes `session_id` from the JSON body unchecked, and `persist_message` builds `sessions_dir() / (session_id + ".jsonl")` (`state.cpp:72`). An authenticated caller can write `.jsonl` files anywhere the process can write. The same applies to `run_id`.
- **[S3, V] Scanning a repo runs that repo's code** (`detect_project_verifiers`, `fleet.cpp:525+`). There is no opt-out and no sandbox.
- **[S4, H] The scanner follows symlinks out of the target** (`scan.cpp:551`, the SCA scanner, the parser's `ifstream`). A hostile repo can link to `~/.ssh/id_*` or `~/.aws/credentials`, and that content can reach findings, caches, evidence or LLM prompts.
- **[S5, V] The skill sandbox fails open.** Without nsjail, a skill runs unsandboxed even when a policy is set (`executor.cpp:269-276`). With nsjail, `deny_all` passes `--disable_clone_newnet 0`, which in nsjail *keeps* host networking, plus a stray argument (`:303-306`). `read_only` maps to `--disable_proc` under `--chroot /`, which leaves the whole filesystem visible. The nsjail semantics come from its documentation, not a runtime test.
- **[S6, H] Children inherit the full environment**, including LLM API keys (`executor.cpp:90-98`). The deny list misses `LD_AUDIT`, `GLIBC_TUNABLES`, `NODE_OPTIONS`, `PYTHONPATH`, `BASH_ENV` and `PYTHONSTARTUP`.
- **[S7, M] Prompt injection reaching tools that act.** Scanned content goes to agentic CLIs: `aider --message` can edit files, and `claude --permission-mode dontAsk` honours the scanned repo's project settings when run from inside it. Prompts are passed without `--`, so a leading `-` is read as a flag.

**Medium**

- **[S8, H] Gateway exposure.** It binds `0.0.0.0` over plain HTTP and WebSocket by default, with no rate limit or connection cap. The body-size check runs after httplib has already buffered the body.
- **[S9, H] WebSocket auth fails open** when no token is set (HTTP fails closed). No Origin check, so cross-site WebSocket hijacking is possible; no message-size cap.
- **[S10, H] Secret files are written in two steps.** They are created under the umask, then `chmod 0600`, following symlinks and not atomically (`auth_profile_store.cpp:327`, the `attest.cpp:45` keygen).
- **[S11, M] Shell injection in custom slash commands.** User arguments are concatenated into `/bin/sh -c` (`knowledge.cpp:599-605`).
- **[S12, H] Hostile files and responses are read without size limits.** No per-file byte cap; no HTTP response-size cap.
- **[S13, H] Supply chain.**
  - FetchContent dependencies are pinned by tag (`CMakeLists.txt:122-246`, `libs/parse/CMakeLists.txt`).
  - GitHub Actions are pinned by tag; `release.yml` grants OIDC and release-write tokens at workflow level, so build jobs running third-party tools hold them.
  - The shipped `euxis-scan` Action interpolates its inputs into shell, an injection pattern that zizmor flags.
- **[S14, H] No fuzzing**, for a tool whose job is parsing hostile input.
- **[S15, H] Missing compiler hardening flags.** `_GLIBCXX_ASSERTIONS`, `-fstack-clash-protection`, `-fcf-protection=full` / `-mbranch-protection=standard`, `-ftrivial-auto-var-init=zero` and `-z noexecstack` are missing, and `-Wno-uninitialized` is set globally on GCC 14+.

**Low**

- **Credentials.** `verify_credential` ignores expiry; the id isn't covered by the MAC; it uses symmetric HMAC (not wired yet).
- **Keys and secrets.**
  - The public key comes from the last 32 bytes of the secret-key file without re-deriving it.
  - No zeroisation of secrets.
  - The `sign_payload` MCP tool signs with a throwaway key and returns no public key.
- **Memory store.** Its encryption tag covers only the tier label, so stored entries can be swapped or replayed within a tier.
- **A2A.** Defaults to `http://`.
- **Process launch.** Commands are resolved through `PATH`.
- **Installer.** `install.sh` makes cosign optional, and the sha256 file comes from the same origin as the archive.
- **Policy documents.** `SECURITY.md` and `security-policy.json` give different contacts and supported versions.

**Checked and correct**

- **Crypto:** the XChaCha20 fix, the Argon2id KDF, and constant-time comparisons (`sodium_memcmp`).
- **Gateway HTTP:** auth fails closed.
- **etx OAuth:** PKCE with a random state.
- **Process launching:** no `system()` or `popen`; argv-based spawning throughout.
- **Archives:** no extraction code, so zip-slip does not apply.
- **Builds:** ASan/UBSan are on in dev builds.
- **Release signing:** cosign plus build provenance.
- **Branch protection:** requires signatures; force-push is blocked.
- **Scanning and alerts:** secret scanning and push protection are on; there are no open alerts.

### 2.4 Product, DX and observability

- **[Critical, V] Exit codes.** Documented: 0 clean, 1 infrastructure error, 2 advisory, 3 blocking, 4 policy, 5 timeout, 130 interrupted (`exit_codes.hpp`, `docs/reference/exit-codes.md`). Actually returned by check, triage and review: 0 trusted, 1 for any finding, 2 for no evidence.
  - The documented GitLab `allow_failure: exit_codes: [2]` therefore turns "collected no evidence" into a pass.
  - Codes 3, 4, 5 and 130 are never returned, and there is no SIGINT handler.
  - Usage errors return 2 in about 30 places, which collides with "advisory".
- **[High, V] Unknown flags are silently dropped** (`parse_common`). `--fail-on`, `--sarif`, `--sbom`, `--evidence` and `--allow-timeout` are documented for check but don't exist there. `.euxis.yaml` `gates.*` is parsed and never applied.
- **[High, M] The target argument isn't the scan root.** It becomes the playbook "goal", and verifiers run on `current_path()`.
- **[High, H] The global `--json` hides the per-command one.** It is stripped before dispatch, so `vulndb --json` prints text.
- **[Medium, V] `verify` is registered twice** (`engine.cpp:75,90`). The prompt-integrity verifier can't be reached.
- **[Critical, V] The integrations are broken.**
  - The GitHub Action passes flags that don't exist, never writes SARIF, maps `mode: forensic` to an unknown command, has no `exit-code` output, and there's no root `action.yml` for `uses: …@v1`.
  - The GitLab template points `reports: sast` at a Code Quality file.
  - Two pre-commit hooks call `secrets` and `sbom-gate`, which don't exist.
- **[Medium, H] Configuration precedence lets the scanned repo override the user.** The order is defaults < user < **project** < env, with no CLI or `--config` layer. There are two config systems (JSON `ConfigLoader` and runtime YAML).
- **[Medium, H] Logging.**
  - spdlog is used in 53 files and never configured, so `--verbose` doesn't change its level.
  - The default sink is **stdout**, so warnings can corrupt JSON or SARIF output.
  - No structured logs.
  - TUI banners are mixed into `--json` output outside CI.
- **[Medium, H] Observability.** No OpenTelemetry, OTLP, Prometheus or `traceparent`. The gateway has only `/health` and `/ready`. The gateway signal handler calls `server->stop()`, which isn't async-signal-safe.
- **[Low] Rough edges.**
  - `euxis` with no arguments exits 0.
  - Help goes to stdout or stderr inconsistently.
  - The version is hard-coded in `engine.cpp:37`.
  - The docs have both Sphinx and mkdocs configs.

---

## Phase 3: Roadmap

**Principle:** trust first, then the moat, then reach. Each milestone has exit criteria a CI gate can check. Effort assumes one senior C++ engineer plus help from agents; adjust for your team.

### M1: Make it honest and safe (weeks 0–6) — required before any launch or sales conversation

#### M1.1 Verification that verifies (S1, docs)

**Target:** `verify` requires a trust root and checks the whole chain.

```cpp
// attest.cpp — verify flow (sketch)
struct TrustRoot { std::optional<Ed25519PublicKey> key;      // --key
                   std::optional<SigstoreIdentity> identity; // --certificate-identity + --certificate-oidc-issuer
};
auto verify_bundle(const Bundle& b, const TrustRoot& root,
                   std::optional<fs::path> artifact) -> std::expected<VerifiedStatement, VerifyError> {
    if (!root.key && !root.identity) return std::unexpected(VerifyError::NoTrustRoot); // never fall back to b.public_key
    if (b.envelope.payload_type != "application/vnd.in-toto+json")
        return std::unexpected(VerifyError::PayloadType);
    EUXIS_TRY(verify_dsse_pae(b.envelope, *root.key));      // PAE over type+payload, constant-time
    auto stmt = EUXIS_TRY(parse_statement(b.envelope.payload));
    if (artifact) EUXIS_TRY(match_subject_digest(stmt, sha256_file(*artifact)));
    if (b.tlog_entries.empty()) return VerifiedStatement{stmt, TlogStatus::NotChecked};
    EUXIS_TRY(verify_rekor_inclusion(b.tlog_entries, b.envelope)); // M3; until then report NotChecked
    return VerifiedStatement{stmt, TlogStatus::Verified};
}
```

- **Exit codes.** Exit 0 only for a pinned trust root, a matching digest (when an artifact is given) and a valid signature. An embedded key alone is `UNVERIFIED` with a non-zero exit.
- **Docs.** Rewrite README:27 and :54 until Fulcio and Rekor exist (M3).
- **Validation.** Tests for: a forged bundle with an attacker's key (fails); a wrong payloadType (fails); a swapped artifact (fails); a fixture from the `cosign verify-blob-attestation` interop test.

#### M1.2 One CI contract (2.4)

- Route check, triage and review through a single `GateResult → ExitCode` mapping that follows `exit_codes.hpp`, with usage errors on a distinct code (64, `EX_USAGE`).
- Implement:
  - `--fail-on <severity>`, `--sarif <path>`, `--sbom <path>` and `--evidence <path>` on check, triage and review
  - the `.euxis.yaml` `gates.*` thresholds
  - a SIGINT handler that returns 130
- Treat the positional target as the scan root.
- **Unknown flags are errors.** Replace hand-rolled parsing with CLI11 (header-only, MIT), with subcommands and per-command flags, so `--json` stops being global.
- **Fix the GitHub Action.**
  - Add a root `action.yml` (composite).
  - Pass inputs through `env:`, not `${{ }}` in `run:`.
  - Pin all actions by SHA.
  - Add an `exit-code` output.
  - Write outputs before failing the step.
- Fix the GitLab, Azure and Jenkins templates and the pre-commit hooks. Each template gets an end-to-end CI job that runs it against a fixture repo.
- **Validation:** a golden test matrix covering {clean, advisory, blocking, policy, timeout, no-evidence, usage error} × {check, triage, review}, with exact exit codes asserted. The Action is exercised in a workflow on every PR.

#### M1.3 Stop running scanned code by default (S3, S7)

- Running repo verifiers becomes opt-in (`--run-verifiers`). It is off by default and **always off when `CI` is set and the PR comes from a fork**.
- When it is on, run them under a sandbox that fails closed (M1.4).
- Run LLM provider CLIs from an empty temp cwd in read-only or no-tools modes, always pass `--`, and run them with a minimal environment.
- **Validation:** a fixture repo whose `npm test` writes a canary file. The default scan doesn't create it.

#### M1.4 Hostile-input basics (S2, S4, S5, S6, S10, S12)

```cpp
// Safe repo walk: never leave the root, never follow links.
for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::none); it != end; ++it) {
    auto st = it->symlink_status();
    if (fs::is_symlink(st)) { it.disable_recursion_pending(); continue; }
    if (!fs::is_regular_file(st) || it->file_size() > cfg.max_file_bytes) continue;
    int fd = ::openat(root_fd, rel(it->path()).c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    ...
}

// Gateway ids
static const std::regex kId{R"(^[A-Za-z0-9_-]{1,64}$)"}; // or a hand-written check
if (!std::regex_match(session_id, kId)) return bad_request();

// Secret files: create atomically and privately
int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
write_all(fd, data); ::fsync(fd); ::close(fd); fs::rename(tmp, final_path);
```

- **Sandbox.**
  - Fail closed whenever a policy is set.
  - `deny_all` keeps nsjail's default new network namespace, so remove `--disable_clone_newnet`.
  - Read-only bind mount of only the skill directory.
  - Minimal environment built from an allowlist.
- **Size caps.** Per-file (default 4 MiB, configurable) and per-HTTP-response caps, enforced through the content receiver.
- **Gateway defaults.** Bind `127.0.0.1`; `set_payload_max_length`; refuse to start without a token; WebSocket auth at the handshake, with an Origin check and a maximum message size.
- **Validation.** One test per class: traversal id rejected; symlink to `/etc/passwd` not read; sandbox test asserts no network (no silent skip); secret file mode is 0600 when it is created.

#### M1.5 Matching semantics and stable identity (2.1)

- **Rule engine and taint.** Match once per file (Aho-Corasick), and attribute each hit to its **deepest** enclosing node only. Sink and source locations are those nodes' ranges.
- **Stable fingerprint.** `hash(rule_id, normalised_path, enclosing_symbol, normalised_snippet)`, with no byte offsets.
- **Fail closed on rules.** An unknown language or malformed pattern is a load error that names the file and line.
- **Provenance and versions.** Label taint findings `RuleSource::Deterministic`. Generate `version.hpp` from CMake `project(VERSION)` and use it everywhere, removing "0.0.3" and "0.1.0".
- **Cache.** Serialise the full `Finding` (msgpack of the whole struct); a corrupt entry is a cache miss; the ruleset hash covers the canonical serialisation of the whole pack.
- **Validation.**
  - A golden corpus where one `AKIA` produces exactly one finding.
  - Adding a line above a finding keeps its fingerprint.
  - Cold and warm SARIF output are byte-identical.

#### M1.6 Truthful docs

- Rewrite the README claims to match the code: the frameworks shipped, "self-signed Ed25519 (Sigstore keyless planned)", the actual language list, and the real CI flags.
- Turn the documentation-vs-reality gate into a test: every flag in the docs and templates must parse.

**M1 exit criteria:**
- the verify forgery test fails closed
- the exit-code matrix is green
- the Action passes end to end
- the canary is not executed
- the hostile-input tests pass
- the golden corpus is stable
- the README is audited

### M2: Performance and robustness (weeks 6–12)

1. **Measure first (P18).**
   - A benchmark per stage (parse, CPG, DDG, taint, reach, rules) plus an end-to-end `euxis scan` on two pinned corpora: a mid-size JS/TS repo and a mid-size C++ repo, vendored by SHA. Add a deep-nesting fixture.
   - Run Bencher on PRs with a `t_test` threshold and `--err`; widen the path filter; pin the action by SHA.
   - Publish the measured triage and check times, then update the README with real numbers.
2. **CPG adjacency index (P1).**
   ```cpp
   // Nodes are added in pre-order; children are contiguous per parent after a finalize().
   class Graph {
     std::vector<NodeId> parent_;                    // parent_[child] — O(1)
     std::vector<uint32_t> child_off_;               // CSR offsets, size N+1
     std::vector<NodeId>   child_ids_;               // CSR targets
     void finalize();                                // build CSR from AstChild edges once
   public:
     NodeId parent(NodeId c) const noexcept { return parent_[c.v]; }
     std::span<const NodeId> children(NodeId p) const noexcept {
       return {child_ids_.data() + child_off_[p.v], child_off_[p.v + 1] - child_off_[p.v]}; }
   };
   ```
   Do the same for DDG out-edges, used by taint (P2). Rebuild paths from predecessor arrays; mark visited nodes with an epoch counter.
3. **Parser (P4, P5, P9, P11).** Iterative `TSTreeCursor` walks; a single read or mmap; timeout per file; thread-local parsers; arena allocation per file; `-Wl,-z,stack-size=8M` for musl.
4. **Cache (P6).** Statements prepared once; read connections per thread; one write transaction per run; `busy_timeout`; a size+mtime+inode fast path.
5. **Network (P7).** OSV `querybatch`, a bounded pool for hydration, keep-alive, results cached in SQLite for a set time, jitter, `Retry-After`.
6. **Concurrency (P8, P10, P12–P15).** Shared atomic work index; `posix_spawn` with `pidfd`; polled stdin; enforced deadlines; fix the races; add a **TSan** CI job alongside ASan/UBSan.
7. **Allocator and linking (P17, P19).** mimalloc in static builds; `--gc-sections`; then PGO from the benchmark corpus.
8. **Hardening (S14, S15).**
   - Add the OpenSSF compiler-hardening flag set and a `checksec` assertion on release binaries.
   - Add libFuzzer harnesses for the parse, lockfile (npm, Pipfile, Cargo, go.sum), DSSE/bundle JSON and SARIF parsers.
   - Run ClusterFuzzLite on PRs and nightly.
   - Replace `assert` bounds with checked errors.
9. **Supply chain (S13).** Pin FetchContent by SHA plus `URL_HASH`; pin actions by SHA; per-job permissions; `zizmor` and `actionlint` in CI; an OpenSSF Scorecard workflow.
10. **Structure.** Split `cmd_playbook` into a dispatcher plus one handler per mode, which the ratchet makes measurable. Move gateway and CLI logic into libs. Merge the 6 SHA-256 implementations into `libs/crypto`, and the routers and publishers into one each. Either wire up the unused libraries (a2a, identity, inference, memory, adapters) or move them out of the SDK until they are used.
11. **Observability.**
    - Configure spdlog: stderr sink, `--log-level` and `EUXIS_LOG`, JSON format when `--log-format json`.
    - Optional OpenTelemetry (opentelemetry-cpp, OTLP/HTTP) behind a build flag for the gateway and long runs.
    - Prometheus `/metrics` on the gateway.
    - `euxis debug-bundle`.

**M2 exit criteria:**
- scan p95 on the corpus improves by a factor of 5 or more over the M2 baseline, at statistical significance (target to be confirmed by the first measurement)
- no stack overflow on the deep fixture
- TSan is clean
- the fuzzers have run for 24 hours with no crashes
- `checksec` passes
- the Scorecard result is at least 8

### M3: The differentiating features (weeks 12–24)

1. **Real Sigstore.** Keyless signing through Fulcio (OIDC in CI), Rekor inclusion proofs, and a TUF trust root, using sigstore-cpp or a thin client over the public APIs. `verify` checks the inclusion proof offline using the bundle's checkpoint. Include an interop test against `cosign verify-blob-attestation`.
2. **A reproducible, bound evidence pack.**
   - One in-toto Statement whose `subject[]` lists the SARIF, both SBOMs, OpenVEX and the control-mapping report, each by digest.
   - A deterministic `invocationId` derived from the inputs.
   - A `SOURCE_DATE_EPOCH`-style timestamp policy.
   - **Validation:** two runs on the same commit produce byte-identical bundles, apart from the signature.
3. **Ingest any scanner's SARIF (the moat).** `euxis evidence --sarif semgrep.sarif --sarif codeql.sarif …` normalises and deduplicates findings, maps them to controls, and signs the pack. Euxis becomes the signed evidence layer for whatever the customer already runs, so it doesn't need to beat CodeQL.
4. **Real control mappings.** Versioned, signed content packs for SOC 2 CC, ISO 27001:2022 Annex A, NIST SSDF 1.2, EU CRA Annex I, DORA ICT RMF and PCI DSS 4.0, each control mapped to evidence predicates. Content-pack tests assert that every control has at least one evidence source.
5. **CRA readiness.** A product-level vulnerability-handling report, an Article 14 notification draft generator (ENISA SRP field mapping), and support-period and SBOM retention metadata.
6. **CI essentials.**
   - `--baseline <sarif>` and `--diff <merge-base>` for PR scanning.
   - Inline `euxis-ignore[rule]: reason` with an expiry date.
   - Full SARIF 2.1.0: `codeFlows`, `security-severity`, `suppressions`, `baselineState`, `automationDetails`, relative URIs, UTF-16 columns.
7. **Reachability-aware dependency scanning.** Wire `libs/reach` into `vulndb` enrichment, and emit OpenVEX `not_affected` with `vulnerable_code_not_in_execute_path` justifications into the pack.
8. **Rule engine v2.** Semgrep-compatible YAML (a subset: `pattern`, `pattern-either`, `pattern-not`, `pattern-inside`, `metavariable-regex`, `pattern-regex` through RE2), plus a rule test harness (`# ruleid:` / `# ok:` annotations). Compatibility lets users bring their Opengrep rules. Add the TSX grammar.
9. **A2A v1.0** with the `A2A-Version` header and spec method names. Expose an MCP server: `euxis mcp` serving scan, verify and evidence tools, read-only by default.

**M3 exit criteria:**
- `cosign` verifies a euxis bundle
- reproducible-bundle test passes
- three external SARIF formats ingested
- the SOC 2 and CRA packs have full control coverage
- `--diff` scans only changed files
- the rule harness passes

### M4: Commercial layer (week 24 onward) — needs your pillar 4/5 decisions

Recommended open-core split, consistent with how Grafana and Anchore draw the line:

- **Open (AGPL):** the CLI, all scanners, evidence generation and verification, rule and content-pack formats, and community packs.
- **Commercial:**
  - a multi-repo evidence store and dashboard
  - drift detection and continuous rescans
  - SSO/SCIM, RBAC and an audit log
  - a policy management plane (OPA/Cedar)
  - a signed premium framework pack with updates (CRA, DORA, PCI)
  - an auditor portal for read-only verification links
  - a managed EU-hosted SaaS
  - an air-gapped licence with an advisory-feed mirror
  - support SLAs
- **Pricing shape:** per active committer, like GHAS, Semgrep and Snyk.
- **Architecture hooks to add early while they're cheap:**
  - a `EvidenceSink` interface (local filesystem today, hosted store later)
  - a `PolicyProvider` interface
  - stable JSON schemas, versioned, for findings and packs
  - feature flags read from a signed licence file, with **no telemetry and no phone-home by default**: the local-first trust promise is the product
- **Avoid:** Semgrep's rules-licence change (which led to the Opengrep fork) and MinIO-style removal of community features. Both damaged trust in the open core.
- **Licensing note:** dual licensing requires a CLA or copyright assignment for outside contributions. Decide before accepting external PRs.

### Ordering and why

| Order | Why |
|---|---|
| M1 before everything | S1 and S3 are disqualifying for a security vendor. The CI contract is how every user first meets the product. |
| M2 before M3 | M3 features (diff scanning, reachability) multiply engine cost. P1 and P2 would make them unusable on real repos. |
| M3.3 SARIF ingestion early in M3 | It is the quickest route to value without having to beat the incumbents on detection. |
| M4 last | Monetise the moat once it is real. |

**Stop or defer:**
- **Swarm and FinOps routing, A2A gateway features, Telegram adapter, etx Qt app:** none of these drives a purchase today, and they make up a large share of the code and the attack surface.
- **Sandbox-dependent skills:** keep them only if they're needed for M1.3.

---

## Open questions for you

1. **Pillars 4 and 5 are undefined.** Which beachhead: EU-regulated (CRA/DORA) or US SOC 2 startups? Self-hosted licence first, or SaaS? This determines whether M3.4 or M4's SaaS comes first.
2. **Detection engine strategy:** build rule engine v2 (M3.8) or put that effort into SARIF ingestion (M3.3)? We recommend M3.3 first.
3. **Can the swarm, etx and gateway surface be frozen** to reduce the attack surface and maintenance load during M1–M2?
4. **CLA policy** before external contributions, if dual licensing is likely.

---

## Appendix A: Verification status

| Item | Status |
|---|---|
| S1 verify uses embedded key | Re-read: `attest.cpp:378` |
| S2 gateway session id unchecked | Re-read: `routes_sessions.cpp:25-44`, `state.cpp:72` |
| S3 repo verifiers executed | Re-read: `fleet.cpp:525-560` |
| S5 sandbox fail-open and inverted net flag | Re-read: `executor.cpp:269-276,303-306` (nsjail semantics from its documentation, not tested) |
| Exit-code mismatch | Re-read: `fleet.cpp:2755-2777` |
| Unknown flags dropped | Re-read: `surface.cpp:35-68` |
| Action passes non-existent flags | Re-read: `action.yml:132-135` |
| `verify` registered twice | Re-read: `engine.cpp:75,90` |
| CPG children/parent edge scans | Re-read: `graph.cpp:70-90` |
| Rule engine matches every node | Re-read: `rule_engine.cpp:168-186` |
| Taint labelled LlmInferred | Re-read: `analyzer.cpp:254` |
| Hard-coded "0.0.3" | Re-read: `scan.cpp:413,476,723` |
| SQLite FULLMUTEX | Re-read: `store.cpp:83` |
| 5 rules, 2 taxonomies, `.tsx` skipped | Re-read |
| Double reap in `Process::run` | **False; dropped** |
| Everything else | From the reviewers' reading (H/M/L as tagged); not re-read |
| Runtime behaviour, performance numbers, fuzzing | **Not done** |

## Appendix B: Sources (market claims)

- EU CRA SRP: https://www.enisa.europa.eu/cra-srp/ ; https://ncsc.gov.ie/cra/
- EO 14306: https://www.wilmerhale.com/insights/client-alerts/20250613-new-executive-order-modifies-cybersecurity-requirements-to-be-imposed-on-federal-contractors-and-subcontractors
- CISA 2026 SBOM minimum elements: https://www.helpnetsecurity.com/2026/07/30/cisa-sbom-guidance-updated/ ; https://www.cyber.gov.au/publication/2026-minimum-elements-for-a-software-bill-of-materials-sbom
- SLSA v1.2: https://slsa.dev/blog/2025/11/announce-slsa-v1.2
- PyPI attestations: https://blog.sigstore.dev/pypi-attestations-ga/ ; https://blog.pypi.org/posts/2025-12-31-pypi-2025-in-review
- Slopsquatting (USENIX Security 2025): https://www.usenix.org/conference/usenixsecurity25/presentation/spracklen
- Trivy compromise: https://www.aquasec.com/blog/trivy-supply-chain-attack/
- A2A anniversary and v1.0: https://opensource.googleblog.com/2026/04/a-year-of-open-collaboration-celebrating-the-anniversary-of-a2a.html
- Semgrep pricing: https://semgrep.dev/pricing ; Snyk: https://snyk.io/pricing/ ; GHAS: https://github.blog/changelog/2025-03-04-introducing-github-secret-protection-and-github-code-security/
- Socket: https://socket.dev/blog/socket-raises-60m-series-c-press-release ; Endor AURI: https://securitybrief.co.uk/story/endor-labs-launches-auri-to-secure-ai-driven-coding
- Chainguard: https://fortune.com/2025/04/23/exclusive-chainguard-secures-356-million-series-d-as-valuation-soars-to-3-5-billion/ ; Aikido: https://www.aikido.dev/blog/aikido-funding-series-b
- Checkmarx/Tromzo: https://checkmarx.com/press-releases/checkmarx-acquires-tromzo-to-launch-new-era-of-application-security/
- Vanta: https://sacra.com/research/vanta-at-300m-year/ ; Drata/SafeBase: https://techcrunch.com/2025/02/12/security-compliance-firm-drata-acquires-safebase-for-250m
- Grafana AGPL: https://grafana.com/blog/2021/04/20/grafana-loki-tempo-relicensing-to-agplv3 ; Anchore Enterprise: https://docs.anchore.com/current/docs/overview/capabilities/
- Opengrep fork: https://www.infoq.com/news/2025/02/semgrep-forked-opengrep ; MinIO: https://blocksandfiles.com/2025/06/19/minio-removes-management-features-from-basic-community-edition-object-storage-code/

Market figures (valuations, pricing tiers) come from these sources and were not checked independently.
