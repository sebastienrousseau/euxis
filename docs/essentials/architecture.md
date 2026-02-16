# Euxis Architecture

Comprehensive architectural documentation for the Euxis multi-provider AI agent orchestration framework.

**Version:** 0.0.8
**Framework Size:** ~1,270 LOC (8 library modules + main entry point)
**Agent Count:** 50 (12 core + 38 fleet)

---

## System Architecture Overview

The Euxis framework is organized into five distinct layers, each with specific responsibilities. The CLI layer provides the user interface, shell libraries handle core logic, the optional Python TUI offers rich visualization, the data layer manages persistence, and the provider layer abstracts AI model interactions.

## Gateway Control Plane (v0.1)

Euxis introduces a minimal Gateway control plane to front agent execution with a WebSocket surface and health endpoint. The Gateway is a thin runtime that delegates execution to existing Euxis CLI entry points.

**Responsibilities:**
- Accept WebSocket connections and validate auth headers.
- Provide a health endpoint for liveness checks.
- Route inbound frames to `euxis-dispatch` or `euxis-squad` (future).

**Entry Point:**
- `euxis-gateway run` (see `docs/reference/gateway-cli.md`)

```mermaid
flowchart TB
    subgraph CLI["CLI Layer"]
        direction LR
        euxis[euxis]
        dispatch[euxis-dispatch]
        squad[euxis-squad]
        combo[euxis-combo]
        playbook[euxis-playbook]
        council[euxis-council]
        loop[euxis-loop]
    end

    subgraph ShellLib["Shell Libraries (core/lib/)"]
        direction LR
        cli_sh[cli.sh<br/>Argument Parsing]
        dispatch_sh[dispatch.sh<br/>Command Routing]
        agents_sh[agents.sh<br/>Agent Discovery]
        providers_sh[providers.sh<br/>Provider Execution]
        memory_sh[memory.sh<br/>Tiered Memory]
        prompt_sh[prompt.sh<br/>Prompt Assembly]
        session_sh[session.sh<br/>Session Management]
        registry_sql_sh[registry_sql.sh<br/>SQLite Queries]
    end

    subgraph TUI["Python TUI (etx/)"]
        direction LR
        cortex[euxis-cortex<br/>Vector Memory]
        synthesize[euxis-synthesize<br/>Output Synthesis]
        ui[euxis-ui<br/>Textual Dashboard]
    end

    subgraph Data["Data Layer"]
        direction LR
        registry[(registry.db<br/>SQLite Agent Registry)]
        memory_files[(memory.md<br/>Per-Agent Memory)]
        lifecycle[(lifecycle/<br/>Agent State)]
        projects[(projects/<br/>Session Data)]
    end

    subgraph Providers["Provider Layer"]
        direction LR
        claude[Claude<br/>S-Tier Strategic]
        gemini[Gemini<br/>A-Tier Research]
        goose[Goose<br/>B-Tier Coding]
        ollama[Ollama<br/>C-Tier Utility]
        qwen[Qwen<br/>B-Tier Math]
        openai[OpenAI<br/>Generalist]
    end

    CLI --> ShellLib
    CLI --> TUI
    ShellLib --> Data
    ShellLib --> Providers
    TUI --> Data

    style CLI fill:#e1f5fe,stroke:#01579b
    style ShellLib fill:#fff3e0,stroke:#e65100
    style TUI fill:#f3e5f5,stroke:#7b1fa2
    style Data fill:#e8f5e9,stroke:#2e7d32
    style Providers fill:#fce4ec,stroke:#c2185b
```

### Layer Responsibilities

| Layer | Components | Purpose |
|-------|-----------|---------|
| **CLI** | `euxis`, `euxis-dispatch`, `euxis-squad`, `euxis-combo`, `euxis-playbook` | User-facing commands for agent invocation and orchestration |
| **Shell Libraries** | `cli.sh`, `dispatch.sh`, `agents.sh`, `providers.sh`, `memory.sh` | Core framework logic implemented in pure Bash |
| **Python TUI** | `euxis-cortex`, `euxis-synthesize`, `euxis-ui` | Optional rich interfaces requiring Python venv |
| **Data Layer** | `registry.db`, `memory.md`, `lifecycle/`, `projects/` | Persistent storage for agents, memory, and sessions |
| **Provider Layer** | Claude, Gemini, Goose, Ollama, Qwen, OpenAI | External AI model backends |

---

## Agent Coordination Flow

Euxis supports four distinct coordination patterns, each optimized for different use cases. Single agent dispatch is the simplest, while playbooks enable complex multi-phase workflows.

### Single Agent Dispatch

Direct invocation of a single agent for a specific task.

```mermaid
sequenceDiagram
    participant User
    participant CLI as euxis CLI
    participant Dispatch as dispatch.sh
    participant Agent as Agent Prompt
    participant Memory as memory.sh
    participant Provider as AI Provider
    participant Output as Session Output

    User->>CLI: euxis architect "review design"
    CLI->>Dispatch: parse_args()
    Dispatch->>Agent: resolve_agent_path()
    Dispatch->>Memory: build_tiered_memory()
    Memory-->>Dispatch: hot + relevant + cross-agent
    Dispatch->>Dispatch: prepare_prompt()
    Dispatch->>Provider: execute_provider()
    Provider-->>Dispatch: LLM response
    Dispatch->>Output: capture_output()
    Dispatch->>Memory: update memory.md
    Output-->>User: Display result
```

### Squad Parallel Deployment

Multiple agents execute simultaneously with a designated lead agent receiving priority.

```mermaid
sequenceDiagram
    participant User
    participant Squad as euxis-squad
    participant Manifest as Dispatch Manifest
    participant Dispatch as euxis-dispatch
    participant Lead as Lead Agent (P0)
    participant Member1 as Member Agent (P1)
    participant Member2 as Member Agent (P1)
    participant Bus as euxis-bus

    User->>Squad: euxis-squad deploy arch-review "audit system"
    Squad->>Squad: validate_squad_id()
    Squad->>Manifest: Generate JSON manifest
    Note over Manifest: Lead gets P0 priority<br/>Members get P1 priority
    Squad->>Dispatch: euxis-dispatch manifest.json

    par Parallel Execution
        Dispatch->>Lead: Execute (P0 priority)
        Dispatch->>Member1: Execute (P1 priority)
        Dispatch->>Member2: Execute (P1 priority)
    end

    Lead-->>Dispatch: Result
    Member1-->>Dispatch: Result
    Member2-->>Dispatch: Result

    Dispatch->>Bus: Publish squad-events
    Dispatch-->>User: Aggregated results
```

### Combo Sequential Chain

Agents execute in sequence, with each agent receiving the output of the previous agent as context.

```mermaid
sequenceDiagram
    participant User
    participant Combo as euxis-combo
    participant Agent1 as Agent 1<br/>(researcher)
    participant Agent2 as Agent 2<br/>(architect)
    participant Agent3 as Agent 3<br/>(reviewer)
    participant Bus as euxis-bus

    User->>Combo: euxis-combo run research-design "new feature"
    Combo->>Combo: validate_combo_id()

    rect rgb(240, 248, 255)
        Note over Agent1: Step 1/3
        Combo->>Agent1: Execute with original task
        Agent1-->>Combo: Output A
    end

    rect rgb(255, 248, 240)
        Note over Agent2: Step 2/3
        Combo->>Agent2: Execute with task + Output A
        Agent2-->>Combo: Output B
    end

    rect rgb(240, 255, 240)
        Note over Agent3: Step 3/3
        Combo->>Agent3: Execute with task + Output B
        Agent3-->>Combo: Final Output
    end

    Combo->>Bus: Publish combo-events
    Combo-->>User: Chain complete
```

### Playbook Multi-Phase Execution

Complex workflows with multiple phases, checkpoints, and failure handling.

```mermaid
sequenceDiagram
    participant User
    participant Playbook as euxis-playbook
    participant Session as Session Log
    participant Gate1 as Gate 1<br/>(Analysis)
    participant Gate2 as Gate 2<br/>(Implementation)
    participant Gate3 as Gate 3<br/>(Verification)
    participant Dispatch as euxis-dispatch
    participant Bus as euxis-bus

    User->>Playbook: euxis-playbook run deploy-safe "deploy v2"
    Playbook->>Session: Initialize session log

    rect rgb(230, 245, 255)
        Note over Gate1: Phase 1: Analysis
        Playbook->>Dispatch: Deploy analysis squad
        Dispatch-->>Playbook: Results
        Playbook->>Playbook: Checkpoint validation
        alt Checkpoint Failed
            Playbook->>Bus: Publish abort event
            Playbook-->>User: Abort (abort_on_failure=true)
        end
        Playbook->>Session: Log COMPLETED gate=1
    end

    rect rgb(255, 245, 230)
        Note over Gate2: Phase 2: Implementation
        Playbook->>Dispatch: Deploy implementation delegates
        Dispatch-->>Playbook: Results
        Playbook->>Session: Log COMPLETED gate=2
    end

    rect rgb(230, 255, 230)
        Note over Gate3: Phase 3: Verification
        Playbook->>Dispatch: Deploy verification squad
        Dispatch-->>Playbook: Results
        Playbook->>Session: Log COMPLETED gate=3
    end

    Playbook->>Bus: Publish playbook completed
    Playbook-->>User: Playbook complete
```

---

## Memory System Architecture

Euxis implements a sophisticated three-tier memory system inspired by MemGPT, enabling agents to maintain context across sessions while managing memory efficiently.

### Three-Tier Memory Model

```mermaid
flowchart TB
    subgraph Task["Incoming Task"]
        task_input[Task: "fix auth bug"]
    end

    subgraph HotMemory["Tier 1: Hot Memory"]
        direction TB
        hot_desc["Most Recent 20 Entries<br/>Always Included"]
        hot_data["tail -n 20 memory.md"]
    end

    subgraph RelevantMemory["Tier 2: Relevant Memory"]
        direction TB
        rel_desc["Keyword-Matched Entries<br/>Full Memory Search"]
        keywords["Extract keywords >= 5 chars"]
        grep_search["grep -iE pattern memory.md"]
        keywords --> grep_search
    end

    subgraph CrossAgent["Tier 3: Cross-Agent Memory"]
        direction TB
        cross_desc["Sibling Agent Memories<br/>Read-Only Context"]
        sibling1["architect/memory.md"]
        sibling2["reviewer/memory.md"]
        sibling3["tester/memory.md"]
    end

    subgraph Assembled["Assembled Memory Context"]
        prompt["### Tier 1: Hot Memory (Recent)<br/>...<br/>### Tier 2: Relevant Memory<br/>...<br/>### Tier 3: Cross-Agent Context<br/>..."]
    end

    task_input --> HotMemory
    task_input --> RelevantMemory
    task_input --> CrossAgent

    HotMemory --> Assembled
    RelevantMemory --> Assembled
    CrossAgent --> Assembled

    style HotMemory fill:#ffcdd2,stroke:#c62828
    style RelevantMemory fill:#fff9c4,stroke:#f9a825
    style CrossAgent fill:#c8e6c9,stroke:#2e7d32
    style Assembled fill:#e3f2fd,stroke:#1565c0
```

### Memory Types and Operations

```mermaid
flowchart LR
    subgraph Types["Memory Entry Types"]
        episodic["EPISODIC<br/>Session events, task outcomes"]
        semantic["SEMANTIC<br/>Facts, knowledge, relationships"]
        procedural["PROCEDURAL<br/>Learned patterns, contraindications"]
        reflection["REFLECTION<br/>Agent self-assessment"]
    end

    subgraph Operations["Memory Operations"]
        direction TB
        prune["prune_memory()<br/>Remove old entries<br/>Keep REFLECTION/CONTRAINDICATION"]
        drift["detect_semantic_drift()<br/>Find contradictions"]
        resolve["resolve_memory_contradiction()<br/>Supersede, keep_both, reject"]
        evolve["auto_evolve_graph()<br/>Extract entities<br/>Update knowledge graph"]
    end

    subgraph Retention["Retention Policy"]
        direction TB
        permanent["Permanent<br/>REFLECTION<br/>CONTRAINDICATION"]
        recent["Recent<br/>Last 100 entries"]
        pruned["Pruned<br/>Old entries removed"]
    end

    Types --> Operations
    Operations --> Retention

    style episodic fill:#e1f5fe
    style semantic fill:#fff3e0
    style procedural fill:#f3e5f5
    style reflection fill:#e8f5e9
```

### Memory Pruning Strategy

```mermaid
flowchart TB
    start([Memory File > 500 Lines]) --> extract_header[Extract Header Line]
    extract_header --> extract_permanent[Extract REFLECTION &<br/>CONTRAINDICATION Entries]
    extract_permanent --> keep_recent[Keep Last 100 Entries]
    keep_recent --> write_temp[Write to Temp File]
    write_temp --> atomic_replace[Atomic mv to Original]
    atomic_replace --> log_prune[Log: lines before -> after]

    subgraph Preserved["Always Preserved"]
        refl["REFLECTION entries"]
        contra["CONTRAINDICATION entries"]
        recent["Most recent 100 lines"]
    end

    style start fill:#ffcdd2
    style atomic_replace fill:#c8e6c9
```

### Semantic Drift Detection

When new information potentially contradicts existing memory, the system detects and resolves conflicts.

```mermaid
flowchart TB
    new_fact["New Fact: 'API uses OAuth'"] --> extract_entities
    extract_entities["Extract Entities<br/>(words >= 5 chars)"] --> search_existing
    search_existing["Search Existing Memory<br/>for Matching Entities"] --> check_negation

    check_negation{"Negation Pattern<br/>Detected?"}
    check_negation -->|"New: 'uses OAuth'<br/>Old: 'uses JWT'"| drift_detected
    check_negation -->|"No Contradiction"| append_normal

    drift_detected["[DRIFT] Value change detected"]
    drift_detected --> resolution

    resolution{"Resolution Strategy"}
    resolution -->|supersede| mark_old["Mark old as [SUPERSEDED]<br/>Add new fact"]
    resolution -->|keep_both| keep["Add new with [CONFLICTS]<br/>Keep both versions"]
    resolution -->|reject| reject_new["Add new as [REJECTED]<br/>Keep old version"]

    append_normal["Append to Memory"]
    mark_old --> done([Done])
    keep --> done
    reject_new --> done
    append_normal --> done

    style drift_detected fill:#fff9c4,stroke:#f9a825
    style resolution fill:#e3f2fd,stroke:#1565c0
```

---

## Provider Routing

Euxis implements intelligent provider routing based on agent role, task priority, and capability requirements.

### Intelligence Tier Routing

```mermaid
flowchart TB
    subgraph Input["Routing Input"]
        agent["Agent Name"]
        priority["Task Priority<br/>(P0/P1/P2)"]
    end

    subgraph P0Check["Priority Override"]
        p0_check{"Priority = P0?"}
    end

    subgraph TierRouting["Tier-Based Routing"]
        direction TB

        subgraph STier["S-Tier: Strategic"]
            s_agents["orchestrator<br/>architect<br/>planner<br/>reviewer"]
            s_provider["claude"]
        end

        subgraph ATier["A-Tier: Research/Domain"]
            a_research["researcher<br/>deep-researcher<br/>auditor"]
            a_domain["cryptographer<br/>ledger"]
            a_provider1["gemini"]
            a_provider2["claude"]
        end

        subgraph BTier["B-Tier: Coding/Systems"]
            b_coding["debugger<br/>tester<br/>automaton"]
            b_systems["conduit<br/>custodian"]
            b_math["optimizer<br/>telemetrist"]
            b_provider1["goose"]
            b_provider2["qwen"]
        end

        subgraph CTier["C-Tier: Utility"]
            c_agents["butler<br/>librarian<br/>writer"]
            c_provider["ollama"]
        end
    end

    subgraph Fallback["Default Fallback"]
        default_provider["claude<br/>(DEFAULT_PROVIDER)"]
    end

    Input --> P0Check
    P0Check -->|Yes| s_provider
    P0Check -->|No| TierRouting

    s_agents --> s_provider
    a_research --> a_provider1
    a_domain --> a_provider2
    b_coding --> b_provider1
    b_systems --> b_provider1
    b_math --> b_provider2
    c_agents --> c_provider

    TierRouting -->|"Unknown Agent"| Fallback

    style STier fill:#e8f5e9,stroke:#2e7d32
    style ATier fill:#e3f2fd,stroke:#1565c0
    style BTier fill:#fff3e0,stroke:#e65100
    style CTier fill:#f5f5f5,stroke:#616161
```

### Provider Selection Logic

```mermaid
flowchart TB
    start([resolve_tiered_provider]) --> check_priority

    check_priority{"Priority = P0?"}
    check_priority -->|Yes| return_claude["return 'claude'"]
    check_priority -->|No| match_agent

    match_agent{"Match Agent<br/>to Tier"}

    match_agent -->|"orchestrator, architect,<br/>planner, reviewer"| return_claude

    match_agent -->|"researcher, deep-researcher,<br/>auditor"| return_gemini["return 'gemini'"]

    match_agent -->|"responder"| return_kiro["return 'kiro-cli'"]

    match_agent -->|"debugger, tester,<br/>automaton,<br/>maintainer"| return_goose["return 'goose'"]

    match_agent -->|"crypto-auditor,<br/>payments-steward"| return_claude

    match_agent -->|"optimizer,<br/>telemetrist"| return_qwen["return 'qwen'"]

    match_agent -->|"butler, librarian,<br/>writer"| return_ollama["return 'ollama'"]

    match_agent -->|"No Match"| return_default["return DEFAULT_PROVIDER<br/>('claude')"]

    style return_claude fill:#e8f5e9
    style return_gemini fill:#e3f2fd
    style return_goose fill:#fff3e0
    style return_qwen fill:#fce4ec
    style return_ollama fill:#f5f5f5
```

### Provider Fallback Mechanisms

```mermaid
flowchart TB
    subgraph Execution["Provider Execution"]
        execute["execute_provider()"]
        timeout["run_with_timeout()<br/>EUXIS_API_TIMEOUT (300s)"]
    end

    subgraph Providers["Provider Commands"]
        claude_cmd["run_claude()<br/>claude --print --model ..."]
        gemini_cmd["run_gemini()<br/>gemini --output-format text"]
        goose_cmd["run_goose()<br/>goose run --model ..."]
        ollama_cmd["run_ollama()<br/>ollama run model"]
    end

    subgraph Errors["Error Handling"]
        timeout_err["Timeout (exit 124)"]
        missing_cli["CLI Not Found"]
        api_error["API Error"]
    end

    subgraph Recovery["Recovery Actions"]
        log_timeout["Log: 'Consider increasing<br/>EUXIS_API_TIMEOUT'"]
        log_install["Log: 'Install via: brew/npm...'"]
        return_error["Return Error to Caller"]
    end

    execute --> timeout
    timeout --> Providers

    Providers --> timeout_err
    Providers --> missing_cli
    Providers --> api_error

    timeout_err --> log_timeout --> return_error
    missing_cli --> log_install --> return_error
    api_error --> return_error

    style timeout_err fill:#ffcdd2
    style missing_cli fill:#ffcdd2
    style api_error fill:#ffcdd2
```

---

## Agent Lifecycle

Agents transition through a defined set of states during execution. The lifecycle system enables coordination overhead measurement and detection of stuck agents.

### State Diagram

```mermaid
stateDiagram-v2
    [*] --> idle: Agent registered

    idle --> active: agent_lifecycle_transition("active")

    active --> completed: Task succeeded
    active --> failed: Task error
    active --> timeout: Stale > 30 min

    completed --> idle: Next invocation
    failed --> idle: Next invocation
    timeout --> idle: cleanup_stale_agents()

    note right of idle
        No active task
        State file absent or "idle"
    end note

    note right of active
        Task in progress
        Tracked in lifecycle/agent.state
    end note

    note right of timeout
        cleanup_stale_agents() detects
        agents stuck > timeout_seconds
    end note
```

### Lifecycle State Flow

```mermaid
flowchart TB
    subgraph Entry["Task Entry"]
        cmd["euxis agent task"]
        parse["parse_args()"]
        setup["setup_session()"]
    end

    subgraph Active["Active State"]
        transition_active["agent_lifecycle_transition<br/>('active', session_id)"]
        write_state["Write 'active' to<br/>lifecycle/agent.state"]
        log_transition["Append to<br/>transitions.jsonl"]
    end

    subgraph Execution["Task Execution"]
        prompt["prepare_prompt()"]
        execute["execute_provider()"]
        capture["capture_output()"]
    end

    subgraph Exit["Task Exit"]
        transition_complete["agent_lifecycle_transition<br/>('completed', session_id)"]
        write_complete["Write 'completed' to<br/>lifecycle/agent.state"]
    end

    subgraph Monitoring["Background Monitoring"]
        list_active["list_active_agents()<br/>Find all state='active'"]
        count_active["count_active_agents()<br/>Coordination overhead"]
        cleanup["cleanup_stale_agents()<br/>Detect timeout > 30 min"]
    end

    Entry --> Active
    Active --> Execution
    Execution --> Exit

    Active -.-> Monitoring
    cleanup -.->|"Stale detected"| timeout_state["agent_lifecycle_transition<br/>('timeout')"]

    style Active fill:#fff9c4
    style Exit fill:#c8e6c9
    style timeout_state fill:#ffcdd2
```

### Lifecycle Tracking Files

```mermaid
flowchart LR
    subgraph StateFiles["State Files"]
        direction TB
        state1["lifecycle/architect.state<br/>Content: 'active'"]
        state2["lifecycle/reviewer.state<br/>Content: 'completed'"]
        state3["lifecycle/tester.state<br/>Content: 'idle'"]
    end

    subgraph TransitionLog["transitions.jsonl"]
        direction TB
        entry1["{'ts':'2026-02-14T10:00:00Z',<br/>'agent':'architect',<br/>'state':'active',<br/>'session':'20260214-100000'}"]
        entry2["{'ts':'2026-02-14T10:05:00Z',<br/>'agent':'architect',<br/>'state':'completed',<br/>'session':'20260214-100000'}"]
    end

    subgraph Queries["Query Functions"]
        get_state["agent_get_state('architect')<br/>-> 'active'"]
        list["list_active_agents()<br/>-> ['architect']"]
        count["count_active_agents()<br/>-> 1"]
    end

    StateFiles --> Queries
    TransitionLog --> Queries

    style StateFiles fill:#e3f2fd
    style TransitionLog fill:#fff3e0
```

---

## Data Flow Summary

### Complete Request Flow

```mermaid
flowchart TB
    subgraph User["User Input"]
        cmd["euxis architect 'review auth system'"]
    end

    subgraph CLI["CLI Processing"]
        parse["parse_args()<br/>agent=architect<br/>task='review auth system'"]
        session["setup_session()<br/>Generate SESSION_ID"]
    end

    subgraph Lifecycle["Lifecycle Update"]
        active["Mark agent 'active'"]
    end

    subgraph Memory["Memory Assembly"]
        hot["Tier 1: Hot Memory<br/>(last 20 entries)"]
        relevant["Tier 2: Relevant Memory<br/>(keyword match)"]
        cross["Tier 3: Cross-Agent<br/>(sibling memory)"]
    end

    subgraph Prompt["Prompt Assembly"]
        template["Load agent prompt<br/>prompts/core/architect.txt"]
        protocols["Inject protocols<br/>constitution.md, etc."]
        memory_inject["Inject tiered memory"]
        task_inject["Inject sanitized task"]
    end

    subgraph Provider["Provider Execution"]
        resolve["resolve_tiered_provider()<br/>-> 'claude'"]
        config["resolve_provider_config()<br/>model, flags"]
        execute["run_claude()<br/>claude --print ..."]
    end

    subgraph Output["Output Capture"]
        capture["capture_output()<br/>Write to output/"]
        memory_update["Update memory.md"]
        complete["Mark agent 'completed'"]
    end

    User --> CLI
    CLI --> Lifecycle
    Lifecycle --> Memory
    Memory --> Prompt
    Prompt --> Provider
    Provider --> Output

    style Memory fill:#fff9c4
    style Prompt fill:#e3f2fd
    style Provider fill:#fce4ec
```

---

## Directory Structure

```
~/.euxis/
├── bin/                          # Executables
│   ├── euxis                    # Main entry point
│   ├── euxis-dispatch           # Multi-agent dispatch
│   ├── euxis-squad              # Squad deployment
│   ├── euxis-combo              # Sequential chains
│   ├── euxis-playbook           # Multi-phase workflows
│   ├── euxis-council            # Collaborative decisions
│   ├── euxis-loop               # Iterative execution
│   ├── euxis-bus                # Event bus
│   ├── euxis-graph              # Knowledge graph
│   └── lib/                     # Shell libraries
│       ├── agents.sh            # Agent discovery
│       ├── cli.sh               # Argument parsing
│       ├── common.sh            # Shared utilities
│       ├── dispatch.sh          # Command routing
│       ├── memory.sh            # Tiered memory
│       ├── prompt.sh            # Prompt assembly
│       ├── providers.sh         # Provider execution
│       ├── registry_sql.sh      # SQLite queries
│       ├── session.sh           # Session management
│       └── validation.sh        # Input validation
├── config/                       # Configuration
│   ├── patterns/                # Validation patterns
│   ├── playbooks/               # Playbook definitions
│   └── templates/               # Prompt templates
├── data/                         # Runtime data
│   ├── lifecycle/               # Agent state files
│   ├── projects/                # Project-specific data
│   └── registry_pool/           # Connection pool locks
├── docs/                         # Documentation
├── prompts/                      # Agent prompts
│   ├── core/                    # Core tier (9 agents)
│   ├── fleet/                   # Fleet tier (32 agents)
│   └── protocols/               # Shared protocol fragments
├── registry.db                   # SQLite agent registry
├── registry.json                 # JSON registry (fallback)
└── squads.json                   # Squad/combo definitions
```

---

*Euxis v0.0.8 - Build something that matters.*
