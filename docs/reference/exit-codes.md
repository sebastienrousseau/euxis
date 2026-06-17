# euxis exit codes

`euxis` returns structured exit codes so CI gates can distinguish between
infrastructure failures, advisory findings, and policy-blocking
findings without parsing stdout.

| Code | Meaning | Typical CI behaviour |
|---:|---|---|
| **0** | Scan completed; no findings at or above the advisory threshold. | Job passes. |
| **1** | Infrastructure error: missing config, parse failure, network error, internal panic. | Job fails as a real CI error (page on-call). |
| **2** | Findings exist, all below the configured `blocking_threshold`. | Job passes with a warning annotation; PR comment is posted. |
| **3** | At least one finding at or above `blocking_threshold`. | Job fails. Block the merge. |
| **4** | Policy violation: signed-evidence verification failed, baseline tampered, ruleset signature invalid. | Job fails. Treat as a security event. |
| **5** | Time budget exceeded; partial results emitted. | Job fails as flake-prone unless `--allow-timeout` is set. |
| **130** | Interrupted (SIGINT). | Pass through Ctrl-C semantics. |

## Threshold mapping

The `advisory_threshold` and `blocking_threshold` come from `.euxis.yaml`
(`gates.advisory_threshold`, `gates.blocking_threshold`) or the `--fail-on`
CLI flag. Severities are totally ordered:

`critical > high > medium > low > info`

`advisory_threshold` defaults to `medium`; `blocking_threshold` defaults
to `high`. A finding at `high` therefore returns exit code 3; a finding
at `medium` returns exit code 2.

## GitHub Actions example

```yaml
- name: euxis triage
  id: scan
  uses: sebastienrousseau/euxis@v1
  with:
    mode: triage
    fail-on: high
  continue-on-error: true

- name: Treat advisory as comment, not failure
  if: steps.scan.outcome == 'failure'
  run: |
    case "${{ steps.scan.outputs.exit-code }}" in
      2) echo "advisory findings — leaving job green" ;;
      3) exit 1 ;;
      *) exit 1 ;;
    esac
```

## GitLab example

```yaml
euxis_audit:
  script:
    - /euxis check . --fail-on high
  allow_failure:
    exit_codes: [2]   # advisory only
```

## Jenkins example

```groovy
script {
  def code = sh(returnStatus: true, script: '/euxis check .')
  if (code == 2) {
    currentBuild.result = 'UNSTABLE'
  } else if (code != 0) {
    error("euxis failed with exit code ${code}")
  }
}
```
