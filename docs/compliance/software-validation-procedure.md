# Software Validation Procedure (SVP)

> ISO 13485:2016 §7.5.6 / §7.6 — Validation of Processes for Production and Service Provision

**Document ID:** EUXIS-SVP-001
**Version:** 1.0
**Date:** 2026-03-18
**Standard:** ISO 13485:2016, IEC 62304:2006+A1:2015

## 1. Purpose

This procedure defines the validation approach for the Euxis software system, ensuring that all software changes are verified before release and that the development process maintains traceability from requirements through testing.

## 2. Scope

Applies to:
- All source code in the Euxis repository
- All third-party SOUP components (see EUXIS-SOUP-001)
- Build, test, and release infrastructure

## 3. Definitions

| Term | Definition |
|------|-----------|
| **Verification** | Confirmation through examination and provision of evidence that specified requirements have been fulfilled |
| **Validation** | Confirmation through examination and provision of evidence that requirements for a specific intended use are fulfilled |
| **SOUP** | Software of Unknown Provenance — third-party software not developed under this QMS |
| **SUT** | System Under Test |

## 4. Validation Activities

### 4.1 Unit Testing

- **Framework:** Google Test v1.14.0
- **Coverage target:** All public API functions must have corresponding tests
- **Execution:** Automated on every commit via CI
- **Current status:** 2337+ tests across 19 test binaries, all passing
- **Sanitizers:** ASan + UBSan enabled for all test builds

### 4.2 Static Analysis

- **Compiler warnings:** `-Wall -Wextra -Wpedantic -Werror` (all warnings are errors)
- **SAST:** clang-tidy with security-focused checks (see `.clang-tidy`)
- **Format security:** `-Wformat-security` enabled globally

### 4.3 Security Hardening Verification

The following compiler/linker hardening flags are verified in `cmake/EuxisDefaults.cmake`:

| Control | Flag | Verified |
|---------|------|----------|
| Stack protection | `-fstack-protector-strong` | Build log |
| Position-independent | `-fPIE` / `-pie` | Binary inspection |
| Full RELRO | `-Wl,-z,relro -Wl,-z,now` | `readelf -l` |
| FORTIFY_SOURCE | `_FORTIFY_SOURCE=2` | Release builds |
| Sanitizers | `-fsanitize=address,undefined` | Test execution |

### 4.4 SOUP Validation

Per IEC 62304 §8.1.2:
1. All SOUP components are catalogued in EUXIS-SOUP-001
2. Versions are pinned in `CMakeLists.txt` (FetchContent with `GIT_TAG`)
3. License compatibility verified (all permissive)
4. Integration verified through dependent test suites

### 4.5 Commit Integrity

- **Requirement:** All commits to protected branches must be GPG-signed
- **Enforcement:** `git config commit.gpgsign true` (repository-level)
- **Verification:** `git log --show-signature` in CI pipeline
- **Certification gate:** `commit_signing` gate in `certify-readiness` command

### 4.6 Build Integrity

- **Reproducible builds:** CMake FetchContent with pinned versions
- **No pre-built binaries:** All dependencies built from source
- **Build verification:** `certify-readiness` runs `cmake --build` and checks exit code

## 5. Release Validation Checklist

Before any release:

- [ ] All unit tests pass (`ctest --output-on-failure`)
- [ ] No ASan/UBSan violations in test suite
- [ ] No compiler warnings (`-Werror`)
- [ ] clang-tidy passes with zero findings
- [ ] SOUP register reviewed and current (EUXIS-SOUP-001)
- [ ] CHANGELOG updated with all changes
- [ ] Version bumped in CMakeLists.txt, engine.cpp, lifecycle.cpp
- [ ] `certify-readiness` reports READY or READY WITH GAPS
- [ ] All commits GPG-signed
- [ ] Security policy (SECURITY.md) version table current

## 6. Traceability Matrix

| Requirement Source | Verification Method | Evidence |
|-------------------|---------------------|----------|
| Functional requirements | Unit tests | Test reports (ctest XML) |
| Security requirements | Hardening flags + SAST | Build logs, clang-tidy output |
| SOUP management | SOUP register | EUXIS-SOUP-001 |
| Commit integrity | GPG signing | `git log --show-signature` |
| Build integrity | Reproducible build | CI build logs |
| Code quality | Compiler warnings as errors | Zero-warning builds |

## 7. Non-Conformance Handling

When validation fails:
1. Issue is logged as a defect
2. Root cause analysis performed
3. Corrective action implemented
4. Re-validation performed
5. Evidence recorded

## 8. Review Schedule

This procedure is reviewed:
- Annually
- After any major process change
- After any non-conformance event

## 9. Approval

| Role | Name | Date |
|------|------|------|
| Author | — | 2026-03-18 |
| Reviewer | — | Pending |
| Approver | — | Pending |
