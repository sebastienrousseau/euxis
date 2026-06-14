# DORA / TLPT — euxis evidence mapping

[The EU Digital Operational Resilience Act (DORA)](https://eur-lex.europa.eu/eli/reg/2022/2554)
applies from January 17 2025 to financial entities operating in the
EU. The two pieces euxis evidence directly supports are:

- **Article 24** — ICT-related incident reporting (major incidents
  reported to the relevant competent authority within strict
  windows after detection)
- **Article 26 and the TLPT RTS (Reg. (EU) 2025/1190)** —
  Threat-Led Penetration Testing, mandatory every three years on
  live systems for the largest entities

This document is the runtime mapping for both.

## Scope

DORA is process-heavy. Euxis does not run the TLPT itself, does not
file the regulatory notifications, and does not replace the firm's
ICT risk management framework. What euxis ships is the
machine-readable, cryptographically-signed evidence trail that
testers and ICT risk officers consume.

## Article 24 — incident reporting

| DORA reference | What it requires | What euxis emits |
|---|---|---|
| Art. 19 §1 | Classification of ICT-related incidents per Article 18 | `findings.sarif` carries severity (Critical/High/…), CWE id, and the `confidence` tier needed for triage scoring |
| Art. 24 §1 (initial notification) | Initial notification "as soon as the incident is classified as major" | `predicate.runDetails.metadata.startedOn` is the provable timestamp of incident-relevant detection; `vex.openvex.json` records the status decision with its own timestamp |
| Art. 24 §1 (intermediate report) | "An intermediate report after the initial notification" | Re-emit the same evidence bundle with a new invocationId once status changes; the chain of bundles is the report sequence |
| Art. 24 §1 (final report) | "A final report" | Same — the bundle whose VEX status reaches `fixed` is the final report payload |

The signed evidence chain (initial bundle → intermediate bundle →
final bundle, all signed by the same Ed25519 key) is what gives
the supervisory authority assurance the timeline has not been
post-hoc reconstructed.

## Article 26 + TLPT RTS — threat-led penetration testing

| DORA / TLPT reference | What it requires | What euxis emits |
|---|---|---|
| TLPT RTS Art. 5 (scoping) | Documented scope of the live-systems test | `predicate.buildDefinition.externalParameters.subject` is the scoped target; `internalParameters.host` records the runner |
| TLPT RTS Art. 8 (red-team report) | Findings with severity, exploitability, and remediation | `findings.sarif` + `vex.openvex.json` |
| TLPT RTS Art. 10 (purple-team transcript) | Recorded interaction between red and blue teams | The chain of evidence bundles produced during the test forms the purple-team transcript when each is signed at the moment of state change |
| TLPT RTS Annex (test report contents) | Threat-intel input, scoping, findings, remediation plan | `predicate.buildDefinition.resolvedDependencies` records the threat-intel snapshot (CVE corpus version, slopsquatting corpus version); the findings + VEX provide the rest |

A v0.1.0-beta DORA TLPT evidence pack is therefore:

```
euxis-evidence.tar.gz/
├── findings.sarif                  // SARIF 2.1.0 with CWE/OWASP taxa
├── sbom.cdx.json                   // CycloneDX 1.6
├── sbom.spdx.json                  // SPDX 3.0.1
├── vex.openvex.json                // OpenVEX status decisions
├── controls.dora.json              // structured form of this table
└── metadata.json                   // scanner + ruleset + host detail
```

…signed by the bundle in the manner documented at
[`docs/evidence-packs/anatomy.md`](../evidence-packs/anatomy.md).

## What's still out of scope for v0.1.0

- **Filing the Article 24 initial notification.** Euxis emits the
  evidence; the customer's compliance pipeline does the filing.
- **TIBER-EU alignment authorities.** TLPT RTS Article 4 requires
  alignment with the TIBER-EU framework where applicable. Euxis
  evidence is consumable by TIBER-EU's reporting templates, but
  the mapping itself is an integration step.
- **Threat-intelligence sourcing.** Euxis records *what threat-intel
  snapshot was used* in `resolvedDependencies`. It does not source
  threat intel.
- **Live-system scoping decisions.** TLPT requires testing on live
  production systems. Euxis runs in CI by default; the on-live
  invocation is a deployment decision, not a euxis feature.

## See also

- [`docs/evidence-packs/anatomy.md`](../evidence-packs/anatomy.md)
- [DORA — full text](https://eur-lex.europa.eu/eli/reg/2022/2554)
- [TLPT Delegated Regulation (EU) 2025/1190](https://eur-lex.europa.eu/eli/reg_del/2025/1190)
- [Jones Day — DORA TLPT analysis](https://www.jonesday.com/en/insights/2025/07/eu-standards-for-threatled-penetration-testing-new-cyber-compliance-imperatives-for-financial-institutions)
- [TIBER-EU framework](https://www.ecb.europa.eu/paym/cyber-resilience/tiber-eu/html/index.en.html)
