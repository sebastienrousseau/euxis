# Security Audit Workflow

**Find vulnerabilities before attackers do.**

This guide walks you through a comprehensive security audit using Euxis agents.

---

## When to Use This Workflow

- Before releasing a new feature to production
- After making changes to authentication or authorization
- During periodic security reviews
- When handling sensitive data (payments, PII, credentials)

---

## The Process

```
Threat Model → Vulnerability Scan → Deep Audit → Remediation → Verification
```

---

## Step 1: Establish Threat Model

Start by understanding what you're protecting.

```bash
euxis sentinel "Identify threat vectors for the user authentication system"
```

**Expected output:**
```
[euxis] Agent: sentinel
[euxis] Provider: claude

FINAL ANSWER:
# Threat Model: User Authentication System

## Attack Surface
1. Login endpoint (/api/auth/login)
2. Password reset flow (/api/auth/reset)
3. Session token storage (client-side)
4. OAuth callback handlers

## Trust Boundaries
- Browser ↔ API Gateway (untrusted)
- API Gateway ↔ Auth Service (semi-trusted)
- Auth Service ↔ Database (trusted)

## Prioritized Risks
| Risk | Impact | Likelihood | Priority |
|------|--------|------------|----------|
| Credential stuffing | HIGH | HIGH | P0 |
| Session hijacking | HIGH | MEDIUM | P1 |
| Password reset abuse | MEDIUM | MEDIUM | P2 |

## Recommended Mitigations
1. Rate limiting on login endpoint
2. Secure, HttpOnly session cookies
3. Token expiry and rotation
```

The sentinel will:
- Map attack surfaces
- Identify trust boundaries
- Prioritize risks by impact

✅ **Checkpoint:** You have a threat model with prioritized risks.

---

## Step 2: Quick Vulnerability Scan

Run automated checks for common vulnerabilities.

```bash
euxis pentester "Scan the login endpoint for OWASP Top 10 vulnerabilities"
```

Check for specific issues:

```bash
# SQL injection
euxis pentester "Test for SQL injection in the search API"

# XSS
euxis pentester "Check for XSS vulnerabilities in user-generated content"

# Authentication bypass
euxis pentester "Test for authentication bypass in password reset flow"
```

✅ **Checkpoint:** Automated vulnerabilities identified.

---

## Step 3: Deep Security Audit

For comprehensive analysis, deploy the Quality squad.

```bash
euxis-squad deploy quality "Full security audit of the payment processing module"
```

This deploys 8 agents in parallel:
- `reviewer`: Code quality
- `inspector`: E2E testing
- `pentester`: Vulnerability assessment
- `auditor`: Compliance check
- `optimizer`: Performance (DoS resistance)
- `watchdog`: Regression analysis
- `polyglot`: Language-specific patterns
- `arbiter`: Conflict resolution

For cryptographic systems:

```bash
euxis-combo run seal "Audit the encryption implementation for user data"
```

The Seal combo runs: `sentinel → cryptographer → pentester → reviewer`

✅ **Checkpoint:** Deep audit complete with findings report.

---

## Step 4: Remediate Issues

Fix identified vulnerabilities with targeted agents.

```bash
# Fix security issues
euxis debugger "Fix the SQL injection vulnerability in search.py:142"

# Harden configuration
euxis automaton "Add security headers to all API responses"

# Update dependencies
euxis maintainer "Upgrade vulnerable dependencies identified in security scan"
```

✅ **Checkpoint:** Vulnerabilities remediated.

---

## Step 5: Verify Fixes

Confirm fixes don't introduce regressions.

```bash
euxis tester "Write security regression tests for the SQL injection fix"
```

Re-run the security scan:

```bash
euxis pentester "Verify SQL injection fix in search endpoint"
```

For full verification:

```bash
euxis-playbook run verify-everything "Confirm security audit remediation"
```

✅ **Checkpoint:** All fixes verified. No regressions.

---

## Step 6: Document and Monitor

Store findings for future reference.

```bash
euxis-cortex remember "Security audit-02: Found and fixed SQL injection in search, XSS in comments, added CSP headers" "pentester" --type episodic

euxis-cortex remember "PROCEDURAL: Before any release touching auth: run euxis-squad deploy quality" "sentinel" --type procedural
```

Set up ongoing monitoring:

```bash
euxis telemetrist "Add security event logging for failed authentication attempts"
```

✅ **Checkpoint:** Security posture documented and monitored.

---

## Quick Reference

| Task | Command |
|------|---------|
| Threat modeling | `euxis sentinel "<system>"` |
| Quick vulnerability scan | `euxis pentester "<target>"` |
| Full security audit | `euxis-squad deploy quality "<scope>"` |
| Crypto audit | `euxis-combo run seal "<system>"` |
| Compliance check | `euxis auditor "<requirement>"` |
| Security testing | `euxis tester "Security tests for <fix>"` |

---

## Audit Checklist

### Authentication
- [ ] Password policies enforced
- [ ] Brute force protection
- [ ] Session management secure
- [ ] MFA implementation correct

### Authorization
- [ ] Role-based access working
- [ ] Privilege escalation blocked
- [ ] Resource isolation verified

### Data Protection
- [ ] Encryption at rest
- [ ] Encryption in transit
- [ ] PII handling compliant
- [ ] Secrets management secure

### Input Validation
- [ ] SQL injection blocked
- [ ] XSS prevented
- [ ] Command injection blocked
- [ ] Path traversal blocked

### Configuration
- [ ] Security headers present
- [ ] CORS properly configured
- [ ] Debug mode disabled
- [ ] Error messages sanitized

---

*Euxis v0.0.10 · Build something that matters.*
