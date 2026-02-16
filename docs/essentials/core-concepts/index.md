# Euxis Concepts

**Understand the mental model behind Euxis.**

These guides explain how to think about Euxis, not just how to use it.

---

## Core Concepts

| Concept | What You'll Learn |
|---------|-------------------|
| [Choosing Coordination Patterns](choosing-coordination.md) | When to use agents, combos, squads, or playbooks |
| [Agent Selection](agent-selection.md) | How to pick the right agent for your task |
| [Provider Routing](provider-routing.md) | How Euxis matches agents to AI providers |

---

## The Big Picture

Euxis is built on three ideas:

### 1. Specialization Over Generalization

Each agent does one thing well. The `debugger` debugs. The `pentester` finds vulnerabilities. The `writer` writes documentation.

This means you don't need to prompt engineer. You describe what you want, and the specialist handles the rest.

### 2. Coordination Over Isolation

Agents work together. The `orchestrator` breaks down goals. Squads run in parallel. Combos chain in sequence. Playbooks coordinate multi-phase workflows.

This means complex tasks don't need complex prompts. You pick the right coordination pattern, and Euxis handles the orchestration.

### 3. Memory Over Forgetting

The Cortex remembers across sessions. Lessons learned become procedural memory. Decisions become semantic memory. Events become episodic memory.

This means your agents get better over time. They don't repeat mistakes. They recall solutions.

---

## Quick Decision Guide

**I need a quick answer about code or architecture:**
→ Use a single agent

**I need multiple perspectives on one problem:**
→ Use a combo (sequential) or squad (parallel)

**I need to execute a multi-phase project:**
→ Use a playbook

**I'm not sure what I need:**
→ Start with `euxis orchestrator "your goal"`: it will figure out the right approach

---

*Euxis v0.0.8 · Build something that matters.*
