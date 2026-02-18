# Security Policy

## Supported Versions

We actively support security updates for the following versions:

| Version | Supported          |
| ------- | ------------------ |
| 0.1.x   | :white_check_mark: |
| < 0.1.0 | :x:                |

## Dependency Security

### Automated Scanning

This project uses automated dependency vulnerability scanning through:

- **Python Dependencies**: `safety`, `pip-audit`, `bandit`, and `semgrep`
- **Node.js Dependencies**: `npm audit` and `semgrep`
- **Scheduled Scans**: Weekly automated scans on Sundays at 2 AM UTC
- **PR Scans**: All pull requests trigger security scans before merge

### Security Gates

The following security gates are enforced:

1. **Critical/High Vulnerabilities**: Build fails if critical or high-severity vulnerabilities are detected
2. **Code Security Issues**: High-severity code security issues trigger warnings and require review
3. **Dependency Freshness**: Dependencies should be updated regularly (tracked in security reports)

### Manual Security Review

For security-sensitive changes, ensure:

- [ ] No credentials or secrets are hardcoded
- [ ] Input validation is implemented for user-facing APIs
- [ ] Authentication and authorization controls are properly implemented
- [ ] Cryptographic operations use approved libraries and algorithms
- [ ] Error messages don't leak sensitive information

## Reporting a Vulnerability

### For Security Researchers

If you discover a security vulnerability, please report it responsibly:

1. **DO NOT** create a public GitHub issue
2. Send details to: security@euxis.co
3. Include:
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact assessment
   - Your contact information (optional)

### Response Timeline

- **Initial Response**: Within 48 hours
- **Vulnerability Assessment**: Within 5 business days
- **Fix Development**: Based on severity (Critical: 7 days, High: 14 days, Medium: 30 days)
- **Public Disclosure**: After fix is deployed and users have time to update

### Severity Classification

| Severity | Description | Examples |
|----------|-------------|----------|
| **Critical** | Immediate risk to data/systems | RCE, SQLi, credential exposure |
| **High** | Significant security impact | XSS, privilege escalation, sensitive data leak |
| **Medium** | Moderate security risk | DoS, information disclosure |
| **Low** | Minor security concern | Security headers, weak algorithms |

## Security Best Practices for Contributors

### Code Security

1. **Input Validation**: Validate all user inputs and external data
2. **Output Encoding**: Properly encode outputs to prevent injection attacks
3. **Authentication**: Use strong authentication mechanisms
4. **Authorization**: Implement proper access controls
5. **Error Handling**: Don't expose sensitive information in error messages
6. **Logging**: Log security-relevant events without sensitive data

### Dependency Management

1. **Pin Versions**: Use exact versions in production dependencies
2. **Regular Updates**: Keep dependencies updated (automated PRs welcome)
3. **Minimal Dependencies**: Only add dependencies that are necessary
4. **Source Verification**: Verify package integrity where possible

### Development Environment

1. **Secrets Management**: Never commit secrets, use environment variables
2. **Local Security**: Keep development environment secure
3. **Branch Protection**: Use signed commits for security-sensitive changes
4. **Code Review**: All security-relevant changes require peer review

## Security Tools and Configuration

### Python Security Tools

- **safety**: Checks for known vulnerabilities in Python packages
- **bandit**: Static security analysis for Python code
- **pip-audit**: Comprehensive Python package vulnerability scanning
- **semgrep**: Advanced static analysis with security rules

### Node.js Security Tools

- **npm audit**: Built-in npm vulnerability scanning
- **semgrep**: JavaScript/TypeScript security analysis

### Configuration Files

Security tool configurations are stored in:
- `.bandit.yml` - Bandit security linter configuration
- `.semgrepignore` - Semgrep analysis exclusions
- `pyproject.toml` - Python tool configurations

## Security Contacts

- **Security Team**: security@euxis.co
- **Project Maintainer**: sebastien.rousseau@gmail.com
- **Security Advisories**: https://github.com/euxis/euxis/security/advisories

---

**Last Updated**: 2026-02-18
**Policy Version**: 1.0