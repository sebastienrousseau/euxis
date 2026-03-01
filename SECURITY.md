# Security Policy

## Our Commitment

The Euxis team takes security seriously. We appreciate your efforts to responsibly disclose your findings and will make every effort to acknowledge your contributions.

## Supported Versions

We provide security updates for the following versions:

| Version | Supported          |
| ------- | ------------------ |
| v0.0.3   | :white_check_mark: |
| < v0.0.3 | :x:                |

Users are strongly encouraged to upgrade to the latest supported version to ensure they receive all security patches.

## Reporting a Vulnerability

### How to Report

**Please do not report security vulnerabilities through public GitHub issues.**

Instead, please report security vulnerabilities via email to:

**Email:** [security@euxis.co](mailto:security@euxis.co)

### Response Timeline

- **Acknowledgment:** Within 48 hours of receipt
- **Initial Assessment:** Within 7 business days
- **Resolution Timeline:** Varies based on severity; critical issues are prioritized

### What to Include in Your Report

To help us triage and respond to your report efficiently, please include:

1. **Description:** A clear description of the vulnerability
2. **Impact:** The potential security impact and severity assessment
3. **Steps to Reproduce:** Detailed steps to reproduce the issue, including:
   - Affected version(s)
   - Operating system and environment details
   - Configuration settings (if relevant)
4. **Proof of Concept:** Code snippets, screenshots, or logs demonstrating the vulnerability
5. **Suggested Fix:** If you have recommendations for remediation (optional)
6. **Contact Information:** Your preferred method of contact for follow-up

### What NOT to Do

- **Do not** disclose the vulnerability publicly before we have had a chance to address it
- **Do not** exploit the vulnerability beyond what is necessary to demonstrate the issue
- **Do not** access, modify, or delete data belonging to other users
- **Do not** perform actions that could degrade the service for other users
- **Do not** use social engineering, phishing, or physical attacks against our team

## Security Measures

Euxis implements multiple layers of security controls:

### Input Validation

- All user inputs are validated and sanitized before processing
- Strict type checking and boundary validation are enforced
- Command injection prevention through parameterized execution

### Sandboxed Execution

- Operations run in isolated, sandboxed environments
- Process isolation prevents unauthorized system access
- Resource limits prevent denial-of-service conditions
- File system access is restricted to designated directories

### Credential Handling

- **No credential storage:** Euxis does not store user credentials locally
- API keys and tokens are handled securely in memory
- Sensitive data is never written to logs or persistent storage
- Environment variables are used for sensitive configuration

### Audit Logging

- Security-relevant events are logged for audit purposes
- Logs do not contain sensitive information (credentials, tokens, PII)
- Log integrity is maintained for forensic analysis
- Retention policies comply with organizational requirements

## Security Update Process

1. **Discovery:** Security issues are identified through internal review, automated scanning, or external reports
2. **Triage:** Issues are assessed for severity using CVSS scoring
3. **Development:** Patches are developed and tested in isolated environments
4. **Review:** Security fixes undergo code review and security testing
5. **Release:** Updates are released with appropriate security advisories
6. **Notification:** Users are notified through release notes and, for critical issues, direct communication

### Severity Classifications

| Severity | CVSS Score | Response Time |
| -------- | ---------- | ------------- |
| Critical | 9.0 - 10.0 | 24-48 hours   |
| High     | 7.0 - 8.9  | 7 days        |
| Medium   | 4.0 - 6.9  | 30 days       |
| Low      | 0.1 - 3.9  | 90 days       |

## Acknowledgments

We believe in recognizing the security researchers who help keep Euxis secure. With your permission, we will acknowledge your contribution in our security advisories and release notes.

### Hall of Fame

We maintain a list of security researchers who have responsibly disclosed vulnerabilities to us. If you would like to be recognized, please let us know when submitting your report.

*No acknowledgments yet - be the first!*

## PGP Key

For sensitive communications, you may encrypt your message using our PGP key:

```
-----BEGIN PGP PUBLIC KEY BLOCK-----

[PGP KEY PLACEHOLDER]

To be published at: https://euxis.co/.well-known/security.txt

Key ID: [PENDING]
Fingerprint: [PENDING]

-----END PGP PUBLIC KEY BLOCK-----
```

You can also find our security contact information at `/.well-known/security.txt` once published.

## Scope

This security policy applies to:

- The Euxis CLI application
- Official Euxis repositories on GitHub
- Associated build and release infrastructure

## Contact

For general security questions (not vulnerability reports), you may reach out to:

- **Security Team:** [security@euxis.co](mailto:security@euxis.co)
- **General Inquiries:** Open a discussion on GitHub

---

*This security policy follows industry best practices and aligns with SOC 2 Trust Services Criteria for security, availability, and confidentiality.*

*Last updated: February 2026*
