# Debugging Workflow

**From bug report to verified fix.**

This guide walks you through debugging with Euxis, from initial triage to confirmed resolution.

---

## When to Use This Workflow

- You have a bug report or unexpected behavior
- You need to trace a problem to its root cause
- You want to fix an issue without introducing regressions

---

## The Process

```
Bug Report → Triage → Investigation → Fix → Verification
```

---

## Step 1: Triage the Issue

Start by understanding what you're dealing with.

```bash
euxis debugger "Analyze this bug: Users report login fails after password reset"
```

The debugger will:
- Identify potential root causes
- Suggest investigation paths
- Flag related code areas

✅ **Checkpoint:** You have a hypothesis about what's wrong.

---

## Step 2: Deep Investigation

If the issue is complex, bring in forensics.

```bash
euxis investigator "Trace the authentication flow for password reset users"
```

For issues involving multiple systems:

```bash
euxis combo run protect "Investigate why sessions expire immediately after login"
```

The Protect combo runs: `sentinel → pentester → auditor → inspector → reviewer`

This gives you security, compliance, testing, and quality perspectives.

✅ **Checkpoint:** You've identified the root cause with evidence.

---

## Step 3: Implement the Fix

Now fix the issue with surgical precision.

```bash
euxis debugger "Fix the session expiration bug by updating token refresh logic in auth.py"
```

The debugger will:
- Make minimal, targeted changes
- Preserve existing behavior
- Add defensive checks where needed

✅ **Checkpoint:** The fix is implemented.

---

## Step 4: Verify the Fix

Never trust a fix until it's tested.

```bash
euxis tester "Write regression tests for the session token refresh fix"
```

For comprehensive verification:

```bash
euxis-squad deploy quality "Verify the authentication fix doesn't break existing flows"
```

✅ **Checkpoint:** Tests pass. The fix is verified.

---

## Step 5: Document and Remember

Store what you learned so it doesn't happen again.

```bash
euxis-cortex remember "Fixed session expiration bug: token refresh was using wrong timestamp field" "debugger" --type episodic

euxis-cortex remember "CONTRAINDICATION: Never compare token timestamps without timezone normalization" "debugger" --type procedural
```

✅ **Checkpoint:** Knowledge is preserved for future reference.

---

## Quick Reference

| Task | Command |
|------|---------|
| Quick bug analysis | `euxis debugger "<description>"` |
| Deep forensics | `euxis investigator "<trace request>"` |
| Security-focused investigation | `euxis combo run protect "<issue>"` |
| Write regression tests | `euxis tester "Write tests for <fix>"` |
| Full verification | `euxis-squad deploy quality "<verify>"` |

---

## Common Patterns

### Null Pointer / Undefined Errors

```bash
euxis debugger "Trace why user.profile returns undefined in the dashboard"
```

### Performance Regressions

```bash
euxis optimizer "Profile and identify why the search endpoint is 3x slower"
```

### Intermittent Failures

```bash
euxis investigator "Analyze flaky test: login_integration_test fails 10% of runs"
```

### Production Incidents

```bash
euxis responder "Production alert: 500 errors spiking on /api/checkout"
```

---

*Euxis v0.1.2 · Build something that matters.*
