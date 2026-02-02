# SECURITY-001: Injection Vulnerability Detection

## Pattern
Detect command injection, SQL injection, XSS, and path traversal vulnerabilities.

## Severity
CRITICAL

## Detection Rules
1. **Command Injection:** User input passed to `exec()`, `system()`, `os.popen()`, subprocess without sanitization
2. **SQL Injection:** String concatenation in SQL queries instead of parameterized queries
3. **XSS:** User input rendered in HTML without escaping (`innerHTML`, `dangerouslySetInnerHTML`, unescaped template vars)
4. **Path Traversal:** User input in file paths without canonicalization (`../` sequences, symlink following)

## Validation
- [ ] All user inputs are validated at system boundaries
- [ ] Parameterized queries used for all database operations
- [ ] Output encoding applied before rendering user data
- [ ] File paths canonicalized before access

## Remediation
- Use allowlists over denylists
- Apply principle of least privilege
- Use framework-provided sanitization functions
- Never trust client-side validation alone
