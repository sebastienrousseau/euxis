# ${DOMAIN} Review Protocol

## Overview

This protocol guides systematic review of ${DOMAIN} patches/changes for correctness,
style compliance, and potential regressions.

## Pre-Review Setup

Before beginning review:
1. ALWAYS load `technical-patterns.md` first
2. Load subsystem-specific files based on changed code (see triggers below)
3. Load pattern files when specific patterns are detected

## Review Tasks

### TASK 0: Identify Changed Functions/Components

Use available tools to identify all functions/components modified:
- For git commits: examine the diff
- List each modified function with its file location

### TASK 1: Subsystem Context Loading

Load subsystem files based on code locations and patterns:

| Trigger | File to Load |
|---------|--------------|
| ${SUBSYSTEM_TRIGGER_1} | `${SUBSYSTEM_FILE_1}` |
| ${SUBSYSTEM_TRIGGER_2} | `${SUBSYSTEM_FILE_2}` |
| ${SUBSYSTEM_TRIGGER_3} | `${SUBSYSTEM_FILE_3}` |

### TASK 2: Pattern Detection

When you encounter these patterns, load the corresponding pattern file:

| Pattern | File |
|---------|------|
| ${PATTERN_TRIGGER_1} | `patterns/${PATTERN_FILE_1}` |
| ${PATTERN_TRIGGER_2} | `patterns/${PATTERN_FILE_2}` |

### TASK 3: Per-Function Analysis

For each modified function, analyze:

**3.1 Error Handling**
- Are all error paths handled correctly?
- Are errors propagated properly?
- Are cleanup/dispose patterns applied correctly?

**3.2 Resource Management**
- Are all allocated resources freed on all paths?
- Are ownership transfers explicit?
- Is cleanup order correct (LIFO)?

**3.3 Thread/Process Safety**
- Are shared resources properly synchronized?
- Are race conditions possible?
- Is the code signal-safe where required?

**3.4 Style Compliance**
- Does the code follow ${DOMAIN} coding style?
- Are naming conventions followed?
- Are patterns consistent with existing code?

### TASK 4: Integration Analysis

**4.1 Caller Impact**
- How do callers use this function?
- Could the changes break existing callers?
- Are API contracts preserved?

**4.2 Regression Risk**
- What could this change break?
- What test coverage exists?
- What edge cases need testing?

## Severity Classification

**CRITICAL**: Security vulnerabilities, crashes, data corruption
**HIGH**: Functional bugs, resource leaks
**MEDIUM**: Style violations, potential issues
**LOW**: Cosmetic, suggestions

## False Positive Elimination

Before reporting ANY issue, apply the Euxis False Positive Elimination protocol:
1. Can the code path actually execute?
2. Are prerequisites validated elsewhere?
3. Is this defensive programming territory?
4. Is this test/debug code?

**If any check is uncertain, do NOT report the issue.**

## Output Format

For each verified issue:
```
VERIFIED ISSUE: [brief description]
Code path: function_a() -> function_b() -> issue_site
Condition: [what must be true for this to trigger]
Evidence: [code snippet showing the problem]
Severity: CRITICAL | HIGH | MEDIUM | LOW
Remediation: [specific fix]
```
