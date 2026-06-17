# EU Cyber Resilience Act — euxis evidence mapping

[The EU Cyber Resilience Act (CRA)](https://eur-lex.europa.eu/eli/reg/2024/2847)
is in force from December 11 2024. The headline date for euxis is
**September 11 2026** — the day on which Article 14 (vulnerability
reporting to ENISA within 24 hours of awareness) and Article 13
(machine-readable SBOM availability for in-scope products) become
mandatory.

The rest of this document maps each CRA obligation that euxis can
help with onto the artefacts a v0.1.0 evidence pack produces.

## Scope

Euxis is **not** a substitute for legal counsel and does not produce
the full CRA conformity assessment. What it produces is the
machine-readable, cryptographically-signed evidence trail that a
human conformity assessor (internal or notified body) consumes.

| CRA reference | What it requires | What euxis emits |
|---|---|---|
| Art. 13 §1, Annex I §2(b) | "Provide a software bill of materials … in a commonly used and machine-readable format" | `sbom.cdx.json` (CycloneDX 1.6) **and** `sbom.spdx.json` (SPDX 3.0.1) per scan |
| Annex I §2(c) | "Apply effective and regular tests and reviews of the security of the product" | `findings.sarif` carrying the full CWE Top 25:2025 + OWASP Top 10:2025 mapping, signed by the evidence bundle |
| Art. 14 §1 | "Notify [ENISA] without undue delay and in any event within 24 hours of becoming aware of an actively exploited vulnerability" | `vex.openvex.json` records the manufacturer's status decisions (affected / fixed / not_affected / under_investigation) per CVE; the 24h clock starts when status flips to "affected" |
| Annex I §2(a) | "Be designed, developed and produced … in such a way that … vulnerabilities can be addressed through security updates" | The signed bundle binds findings to a `predicate.runDetails.metadata.invocationId`, giving each remediation a provable origin |
| Annex II §1 | "Identity, type and additional information enabling [product] identification" | `Statement.subject[].name` + `Statement.subject[].digest.sha256` |
| Annex II §6 | "Information on … security support" | `predicate.buildDefinition.externalParameters.mode` + `predicate.runDetails.builder.version.euxis` record the scanner used and the scan posture |

## How the artefacts compose

```
              ┌────────────────────────────────────────────┐
              │ euxis-evidence.tar.gz.sigstore.json        │
              │  (the bundle)                              │
              └─────────────────────────────────────────┬──┘
                                                        │ signs over (PAE)
                                                        ▼
              ┌──────────────────────────────────────────┐
              │ in-toto Statement v1                     │
              │  subject[].digest.sha256 ───────┐        │
              │  predicateType = SLSA v1.2      │        │
              └────────────────────────────────┼────────┘
                                                ▼
              ┌──────────────────────────────────────────┐
              │ euxis-evidence.tar.gz                    │
              │  ├─ findings.sarif        (CRA §2(c))    │
              │  ├─ sbom.cdx.json         (CRA Art. 13)  │
              │  ├─ sbom.spdx.json        (CRA Art. 13)  │
              │  ├─ vex.openvex.json      (CRA Art. 14)  │
              │  ├─ controls.cra.json     (this overlay) │
              │  └─ metadata.json                        │
              └──────────────────────────────────────────┘
```

The `controls.cra.json` file is the structured form of the table at
the top of this document — one entry per CRA reference, each linked
to the SARIF rule IDs / SBOM purls / VEX statements that satisfy
it. The exact shape is locked at v0.1.0-beta; until then, refer to
this Markdown table as the source of truth.

## What's still out of scope for v0.1.0

- **Notified-body conformity assessment.** CRA Annex VIII module
  H+ is a process question, not an artefact-emission question.
  Euxis evidence packs are an input to that process, not a
  replacement for it.
- **24-hour notification mechanics.** Euxis emits the OpenVEX
  document that tracks status decisions; the actual transmission
  to ENISA (Annex VII reporting templates) is left to whoever
  operates the customer's compliance pipeline.
- **Vulnerability disclosure policy publication.** CRA Annex I
  §1(2)(c) requires a published VDP. That is a documentation
  question on the manufacturer's website; euxis does not edit
  external sites.

## See also

- [`docs/evidence-packs/anatomy.md`](../evidence-packs/anatomy.md) — bundle anatomy
- [`docs/evidence-packs/verifying-a-bundle.md`](../evidence-packs/verifying-a-bundle.md) — verifier run-book
- [Cyber Resilience Act — full text](https://eur-lex.europa.eu/eli/reg/2024/2847)
- [Hogan Lovells — CRA 2026 milestones](https://www.hoganlovells.com/en/publications/eu-cyber-resilience-act-getting-ready-for-cra-compliance-in-2026)
- [OpenSSF — EU CRA position](https://openssf.org/public-policy/eu-cyber-resilience-act/)
