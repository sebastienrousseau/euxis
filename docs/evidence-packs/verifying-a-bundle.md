# Verifying a euxis evidence bundle

Quick run-book for verifying a signed evidence bundle handed over by
auditor / regulator / customer / SOC. The flow assumes you have the
bundle file (`*.sigstore.json`) and the subject tarball
(`euxis-evidence.tar.gz` by default) in hand.

The full anatomy of the bundle is documented at
[`anatomy.md`](anatomy.md). This page is the operational steps.

## What "verified" means

After the steps below, you have three independent assurances:

1. **Signature is real.** The DSSE envelope verifies under the
   Ed25519 public key shipped with the bundle (or resolved from a
   trusted directory if the bundle ships only a hint).
2. **Subject is unchanged.** The SHA-256 (and BLAKE2b-256) of the
   tarball you have on disk match the digests inside the Statement.
3. **Statement is consistent.** The Statement carries the expected
   `predicateType` (SLSA v1.2 Provenance) and the predicate's
   `runDetails.metadata.invocationId` matches the one the original
   euxis run advertised.

Any one of those three failing invalidates the bundle. There is no
"partial pass" — sign over the wrong subject and the entire chain
collapses.

## Step 1 — euxis verify

The fastest path uses the in-tree CLI:

```bash
euxis verify euxis-evidence.tar.gz.sigstore.json \
  --key=signer.pub --artifact=euxis-evidence.tar.gz
```

`--key` is required and names the signer's public key: a path to a
file holding the base64 Ed25519 key (as `euxis attest --keygen`
writes to `<name>.pub`), or the base64 string itself. Get it from a
channel you trust independently of the bundle. The bundle's own
`verificationMaterial.publicKey.rawBytes` is never used: whoever made
the bundle chose that key, so a signature that matches it proves
nothing about who signed.

Exit codes per [`docs/reference/exit-codes.md`](../reference/exit-codes.md):

- `0` — the signature verifies against `--key`, the payloadType is
  in-toto and, with `--artifact`, the file's sha256 matches a subject
- `1` — `--key` missing, or the bundle or key is malformed
- `4` — signature, payloadType or digest mismatch (the
  regulator-visible failure)

The transparency log is not checked.

## Step 2 — Confirm the subject by hand (optional)

`--artifact` already re-hashes the subject. To check it without
euxis, so that "the bundle says X" cannot be substituted for "X is
on my disk":

```bash
# Extract the subject digest the bundle attests over.
jq -r '.dsseEnvelope.payload' euxis-evidence.tar.gz.sigstore.json \
  | base64 -d \
  | jq -r '.subject[0].digest.sha256'

# Compute the local digest of the tarball.
sha256sum euxis-evidence.tar.gz
```

These must match byte-for-byte. If they don't, either the tarball
was rebuilt (in which case the bundle is for a different subject)
or one of the two has been tampered with.

## Step 3 — Inspect the predicate

```bash
jq -r '.dsseEnvelope.payload' euxis-evidence.tar.gz.sigstore.json \
  | base64 -d \
  | jq '.predicate'
```

Confirm:

- `buildDefinition.buildType` is the expected URI (for euxis scans,
  `https://euxis.co/attest/scan/v1`).
- `buildDefinition.externalParameters.mode` is what the scan was
  expected to run as (`triage`, `check`, `review`, `forensic`).
- `runDetails.builder.id` is the expected builder identity.
- `runDetails.metadata.startedOn` / `finishedOn` are in the
  expected window — drift from the originating workflow's
  timestamps is a real signal.

## Step 4 — Cross-check with the SARIF / SBOM / VEX

Extract the subject tarball and re-run the local sanity checks the
auditor would:

```bash
tar -xzf euxis-evidence.tar.gz

# SARIF is valid against the OASIS schema?
jq . findings.sarif > /dev/null

# CycloneDX validates?
cyclonedx-cli validate --input-file sbom.cdx.json

# SPDX validates?
spdx-tools validate sbom.spdx.json

# OpenVEX validates?
jq . vex.openvex.json > /dev/null
```

`cyclonedx-cli` and `spdx-tools` are external tools — euxis does
not embed third-party validators in the binary. The point of this
step is that even an evidence pack signed by a trusted authority
should round-trip cleanly through tooling neither party controls.

## Step 5 — (optional) Re-derive the subject from the source

A regulator can re-run the exact same euxis scan against the
source-of-record:

```bash
euxis check /path/to/source \
  --mode check \
  --sarif    findings.sarif \
  --sbom     sbom.cdx.json \
  --evidence euxis-evidence.tar.gz
```

If euxis is reproducible (and it is — every benchmark
[methodology.md](../benchmarks/methodology.md) entry pinned in
this batch is deterministic), the SHA-256 of the resulting tarball
will match the digest in the original Statement. This is the
strongest form of verification: signature + subject + reproduction.

## What to do if a step fails

| Failure | Likely cause | Action |
|---|---|---|
| `euxis verify` returns 4 | Bundle was re-signed by a different key OR tampered with | Refuse the bundle; request a fresh sign |
| `sha256sum` mismatch | Subject was modified after sign OR you have a different tar | Re-fetch the tarball from the same provenance trail |
| `cyclonedx-cli validate` fails | Subject tarball was repacked with non-canonical JSON | Refuse — the bundle was signed over the original bytes, not these |
| Predicate `buildType` is unexpected | Bundle is from a different tool entirely | Treat as out-of-scope; do not consume |

## See also

- [anatomy.md](anatomy.md) — what every field in the bundle means
- [`euxis verify` reference](../reference/exit-codes.md) — exit codes
- [`docs/compliance/cra.md`](../compliance/cra.md) — how this maps to the EU Cyber Resilience Act
- [`docs/compliance/dora.md`](../compliance/dora.md) — DORA TLPT mapping
