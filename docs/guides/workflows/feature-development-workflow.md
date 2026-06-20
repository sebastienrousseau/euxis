# Feature Development Workflow

**From idea to shipped feature.**

This guide walks you through building a new feature with Euxis, from initial design to production deployment.

---

## When to Use This Workflow

- Building a new feature from scratch
- Adding significant functionality to an existing system
- Implementing a feature that spans multiple components

---

## The Process

```
Design → Plan → Implement → Test → Review → Ship
```

---

## Step 1: Design the Architecture

Start with the big picture. Don't write code yet.

```bash
euxis architect "Design a notification system that supports email, SMS, and push notifications"
```

The architect will:
- Identify components and boundaries
- Define interfaces between systems
- Surface architectural decisions you need to make

For complex features, get the full strategic view:

```bash
euxis combo run envision "Design user notification preferences with multi-channel support"
```

The Envision combo runs: `deep-researcher → planner → architect → evangelist → reviewer`

This gives you scope, structure, communication clarity, and quality validation.

✅ **Checkpoint:** You have a clear architecture with defined interfaces.

---

## Step 2: Plan the Implementation

Break the design into actionable tasks.

```bash
euxis planner "Break down the notification system into implementation tasks"
```

The planner will:
- Sequence tasks by dependency
- Identify parallelizable work
- Flag risks and unknowns

✅ **Checkpoint:** You have a prioritized task list.

---

## Step 3: Implement Components

Build each component with focused agents.

```bash
# Core logic
euxis debugger "Implement the notification dispatcher that routes to email/SMS/push providers"

# Database schema
euxis architect "Design the database schema for storing notification preferences"

# API endpoints
euxis debugger "Implement REST endpoints for managing notification preferences"
```

For infrastructure work:

```bash
euxis automaton "Set up the notification service with Docker and CI/CD pipeline"
```

✅ **Checkpoint:** Core functionality implemented.

---

## Step 4: Add Tests

Write tests as you build, not after.

```bash
euxis tester "Write unit tests for the notification dispatcher"
euxis tester "Write integration tests for the notification preference API"
```

For comprehensive coverage:

```bash
euxis inspector "Create E2E tests for the complete notification flow"
```

✅ **Checkpoint:** Test coverage for all new code.

---

## Step 5: Review and Refine

Get fresh eyes on your implementation.

```bash
euxis reviewer "Review the notification system implementation for quality and correctness"
```

Check for security issues:

```bash
euxis pentester "Security review of the notification system"
```

Optimize performance:

```bash
euxis optimizer "Profile and optimize the notification dispatcher for high throughput"
```

✅ **Checkpoint:** Code reviewed, secured, and optimized.

---

## Step 6: Document

Write documentation that helps users and future developers.

```bash
euxis writer "Document the notification system API for developers"
euxis writer "Create a user guide for notification preferences"
```

✅ **Checkpoint:** Documentation complete.

---

## Step 7: Ship It

Prepare for release.

```bash
# Pre-release verification
euxis-playbook run verify-everything "Notification system v1.0" --dry-run

# Full release
euxis-playbook run zero-to-one "Launch notification system"
```

The Zero to One playbook executes: `Vision → Build → Quality → Growth`

✅ **Checkpoint:** Feature shipped and announced.

---

## Quick Reference

| Task | Command |
|------|---------|
| Architecture design | `euxis architect "<feature>"` |
| Full strategic design | `euxis combo run envision "<feature>"` |
| Implementation planning | `euxis planner "<breakdown>"` |
| Code implementation | `euxis debugger "<implement>"` |
| Infrastructure setup | `euxis automaton "<setup>"` |
| Unit/integration tests | `euxis tester "<tests>"` |
| E2E tests | `euxis inspector "<e2e>"` |
| Code review | `euxis reviewer "<review>"` |
| Security review | `euxis pentester "<security>"` |
| Documentation | `euxis writer "<docs>"` |
| Release | `euxis-playbook run zero-to-one "<feature>"` |

---

## Coordination Patterns

### Solo Development

For small features, work with individual agents:

```bash
euxis architect "Design it"
euxis debugger "Build it"
euxis tester "Test it"
euxis reviewer "Review it"
```

### Team Coordination

For larger features, use squads and playbooks:

```bash
# Strategic planning
euxis-squad deploy vision "Define the product roadmap for Q2"

# Parallel implementation
euxis-squad deploy build "Implement the notification system"

# Quality assurance
euxis-squad deploy quality "Pre-release audit"

# Launch
euxis-squad deploy growth "Announce the new feature"
```

### End-to-End Pipeline

For complete feature development:

```bash
euxis-playbook run zero-to-one "Build and launch notification system"
```

---

## Common Patterns

### Adding an API Endpoint

```bash
euxis architect "Design REST API for user preferences"
euxis debugger "Implement the endpoint with validation"
euxis tester "Write API tests with edge cases"
euxis writer "Add to API documentation"
```

### Building a UI Component

```bash
euxis designer "Design the notification settings page"
euxis debugger "Implement the React component"
euxis animator "Add loading states and transitions"
euxis interactor "Implement keyboard navigation"
```

### Database Migration

```bash
euxis architect "Design schema for notification history"
euxis maintainer "Create migration with rollback support"
euxis tester "Test migration on copy of production data"
```

---

*Euxis v0.1.3 · Build something that matters.*
