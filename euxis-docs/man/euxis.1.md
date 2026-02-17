% EUXIS(1) Euxis 0.1.0
% Euxis Team
% February 2026

# NAME

euxis - Enterprise Unified eXecution Intelligence System

# SYNOPSIS

**euxis** [*COMMAND*] [*OPTIONS*]

**euxis** *AGENT* *TASK* [**--provider** *NAME*]

# DESCRIPTION

Euxis is a multi-provider AI agent framework managing 50+ autonomous specialists.
It coordinates agent squads, sequential chains (combos), and phased playbooks
for complex software engineering tasks.

# COMMANDS

## Fleet Operations

**squad** *SUBCOMMAND*
:   Manage agent squads. Subcommands: **list**, **info**, **deploy**

**combo** *SUBCOMMAND*
:   Chain agents sequentially. Subcommands: **list**, **info**, **run**

**playbook** *SUBCOMMAND*
:   Phased execution with stage gates. Subcommands: **list**, **info**, **run**

**dispatch** *MANIFEST*
:   Deploy fleet from an architect manifest file

**council** *TOPIC*
:   Multi-agent adversarial debate on a topic

**loop** *AGENT* *TASK*
:   Autonomous retry loop with learning

## Quality & Infrastructure

**verify**
:   Run pre-commit quality gates (5 checks)

**health** [**--silent**]
:   Check system probes and connectivity

**certify**
:   Full certification pipeline

**lint**
:   Static analysis pipeline

**test**
:   Infrastructure unit tests

**audit**
:   Security audit with probes

**bench**
:   Performance benchmarking

## Memory & Knowledge

**cortex** *SUBCOMMAND*
:   Vector memory interface (ChromaDB). Subcommands: **search**, **prune**, **sync**

**graph** *SUBCOMMAND*
:   GraphRAG knowledge graph. Subcommands: **query**, **add**, **visualize**

**bus** *SUBCOMMAND*
:   Async pub/sub message bus. Subcommands: **publish**, **subscribe**, **topics**

## Discovery

**agents**
:   List all 50 agents with descriptions

**search** *KEYWORD*
:   Find agents by capability (e.g., "security", "testing")

# GLOBAL OPTIONS

**--provider** *NAME*
:   Override AI provider. Values: claude, gemini, openai, ollama, qwen, crush, kiro-cli, goose

**--json**
:   Output as JSON for piping to jq

**--verbose**
:   Enable verbose logging

**--no-color**
:   Disable colored output

**--help**
:   Show help for command

**--version**
:   Show version information

# AGENTS

Euxis includes 50 specialist agents organized into tiers:

## Core Agents (12)

arbiter, architect, auditor, critic, guard, historian, librarian,
orchestrator, pair, planner, reviewer, route

## Fleet Agents (38)

**Development:** debugger, tester, repairer, maintainer, optimizer

**Research:** researcher, deep-researcher, investigator, inspector

**Security:** pentester, sentinel, watchdog, gatekeeper, cryptographer

**Creative:** designer, writer, animator, marketer, evangelist

**Operations:** automaton, butler, custodian, heal, govern, trace

**Integration:** bridge, conduit, ambassador, interactor, responder

**Analytics:** strategist, tactician, accountant, ledger, telemetrist

**Language:** polyglot, localizer, distill

# SQUADS

Pre-configured agent teams for common tasks:

**vision**
:   Strategy, Research, Architecture (orchestrator, architect, planner...)

**build**
:   Engineering & Execution (debugger, maintainer, tester...)

**quality**
:   Assurance & Security (reviewer, inspector, sentinel, pentester...)

**growth**
:   Branding & Documentation (writer, evangelist, strategist...)

**experience**
:   UI Excellence (designer, animator, interactor...)

**specialist**
:   Domain-Specific (cryptographer, ledger, conduit...)

# COMBOS

Pre-configured agent chains:

**envision**
:   deep-researcher → planner → architect → evangelist → reviewer

**protect**
:   sentinel → pentester → auditor → inspector → reviewer

**craft**
:   writer → strategist → evangelist → reviewer

**refine**
:   designer → animator → interactor → reviewer

**seal**
:   sentinel → cryptographer → pentester → reviewer

# EXAMPLES

Single agent execution:

    $ euxis architect "Design a REST API for user auth"
    $ euxis debugger "Find the memory leak in worker.py"

Squad deployment:

    $ euxis squad deploy quality "Audit the payment module"

Combo execution:

    $ euxis combo run envision "Build a CLI for data pipelines"

Playbook execution:

    $ euxis playbook run python "Refactor the utils module"

Search for agents:

    $ euxis search security

# ENVIRONMENT

**EUXIS_HOME**
:   Euxis installation directory (default: ~/.euxis)

**EUXIS_PROJECT**
:   Project name (default: derived from current directory)

**EUXIS_SESSION_ID**
:   Session identifier (default: timestamp)

**EUXIS_OLLAMA_MODEL**
:   Ollama model to use (default: llama3.2)

**NO_COLOR**
:   Disable colored output when set

# FILES

**~/.euxis/**
:   Main installation directory

**~/.euxis/euxis-core/agents/registry.json**
:   Agent registry

**~/.euxis/euxis-core/agents/squads.json**
:   Squad and combo definitions

**~/.euxis/euxis-core/config/playbooks/**
:   Playbook definitions

**~/.euxis/euxis-runtime/data/projects/**
:   Project data and agent outputs

# EXIT STATUS

**0**
:   Success

**1**
:   General error

**2**
:   Usage error / Invalid arguments

**3**
:   Agent not found

**4**
:   Provider error / Connection failed

**5**
:   Manifest validation failed

# SEE ALSO

**euxis-squad**(1), **euxis-combo**(1), **euxis-playbook**(1)

Online documentation: https://docs.euxis.co

# BUGS

Report bugs at: https://github.com/euxis/euxis/issues

# COPYRIGHT

Copyright (c) 2026 Euxis. MIT License.
