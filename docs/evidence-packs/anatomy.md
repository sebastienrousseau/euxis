# Anatomy of a euxis evidence pack

This document walks through every artefact a euxis evidence pack
contains, what each one means to a regulator, and which euxis library
emits it. It is the canonical answer to "what am I looking at?"
when someone hands you a `*.sigstore.json` bundle.

## TL;DR

A v0.1.0 evidence pack is **one JSON file** at
`euxis-evidence.tar.gz.sigstore.json` (or whatever path
`--bundle=<path>` set) plus the **subject** the bundle attests over
(typically a tarball of the SARIF + SBOM + VEX artefacts produced by
the same scan). A verifier reads the bundle, extracts the in-toto
Statement, walks every claim back to the subject's SHA-256 / BLAKE2b
digest, and validates the Ed25519 signature against the public key
in the bundle's `verificationMaterial`.

## The bundle file

The bundle is a Sigstore Bundle v0.3 JSON document:

    mediaType: application/vnd.dev.sigstore.bundle.v0.3+json

It has three top-level keys:

| Key | Purpose | Emitted by |
|---|---|---|
| `verificationMaterial` | Public key hint + (optional) inline raw key, plus Rekor v2 transparency-log entries when network upload happened | `libs/attest/bundle.{hpp,cpp}` |
| `dsseEnvelope` | The signed payload + signature pair | `libs/attest/dsse.{hpp,cpp}` |
| `mediaType` | Stable identifier so verifiers can dispatch on bundle version | hard-coded constant `kSigstoreBundleMediaType` |

### `verificationMaterial.publicKey`

```json
{
  "hint": "5e2a…",   // BLAKE2b-256 hex of the public key (derive_keyid)
  "rawBytes": "MCo…" // base64 of the raw 32-byte Ed25519 public key
}
```

The `hint` is what lets a verifier look up the right key when many
keys are in play (e.g. quarterly key rotation). `rawBytes` is the
optional inline path — when present, verifying offline is a single
file operation; when absent, the verifier resolves the hint against
a trusted directory (Sigstore's Fulcio root, an in-house KMS, or
the customer's GUAC instance).

### `verificationMaterial.tlogEntries`

Empty in this batch — Rekor v2 upload lands in a follow-up. When
populated, each entry carries:

```json
{
  "logIndex": 42,
  "integratedTime": 1700000000,
  "logID": "rekor-v2-public",
  "kindVersion": {"kind": "dsse", "version": "0.0.1"},
  "inclusionProof": { ... }
}
```

The `inclusionProof` lets an offline verifier reconstruct the
Merkle path from this entry up to a published Rekor checkpoint
hash, proving the signature was logged before some publicly
observable point in time.

### `dsseEnvelope`

The DSSE (Dead Simple Signing Envelope) wrapper around the
in-toto Statement:

```json
{
  "payloadType": "application/vnd.in-toto+json",
  "payload":     "eyJfdHlwZSI6Imh0dHBzOi8…",  // base64 of the canonical Statement JSON
  "signatures":  [{"keyid": "5e2a…", "sig": "MIIC…"}]
}
```

The signature is **not** over the payload bytes directly — it is
over the PAE (Pre-Authentication Encoding) of `(payloadType,
payload)`. PAE is the literal string:

    DSSEv1 <typeLen> <type> <bodyLen> <body>

This prevents an attacker from reusing a signed `application/json`
body under a `application/vnd.in-toto+json` claim. The PAE bytes
are reconstructed by the verifier; they are not stored in the
bundle. See [`libs/attest/dsse.hpp`](../../libs/attest/include/euxis/attest/dsse.hpp).

## The payload — in-toto Statement v1

When the verifier base64-decodes the `payload` field, it gets an
in-toto Statement v1:

```json
{
  "_type": "https://in-toto.io/Statement/v1",
  "subject": [
    {
      "name": "euxis-evidence.tar.gz",
      "digest": {
        "sha256":     "abc…",
        "blake2b-256":"def…"
      }
    }
  ],
  "predicateType": "https://slsa.dev/provenance/v1.2",
  "predicate": { ... }
}
```

The `subject` array binds the signed claim to specific bytes on
disk. The `digest` map carries **both** SHA-256 (the universal
sigstore vocab) and BLAKE2b-256 (the rest of the euxis pipeline)
so verifiers built on either ecosystem can confirm the binding
without re-hashing under the algorithm they don't ship.

The `predicateType` URI tells the verifier what shape `predicate`
takes. For euxis scans it is always SLSA v1.2 Provenance.

## The predicate — SLSA v1.2 Provenance

SLSA v1.2 splits the predicate into `buildDefinition` (what was
asked for) and `runDetails` (how it actually ran):

```json
{
  "buildDefinition": {
    "buildType": "https://euxis.co/attest/scan/v1",
    "externalParameters": {
      "mode": "check",
      "subject": "/repos/myrepo"
    },
    "internalParameters": {
      "host": "euxis-cli"
    },
    "resolvedDependencies": []
  },
  "runDetails": {
    "builder": {
      "id": "https://euxis.co/builder",
      "version": {"euxis": "0.1.0"}
    },
    "metadata": {
      "invocationId":  "euxis-7a9b…",
      "startedOn":     "2026-06-14T12:30:00Z",
      "finishedOn":    "2026-06-14T12:32:14Z"
    }
  }
}
```

`externalParameters` is fingerprintable input — anything a verifier
should treat as significant when comparing two scans. Two scans of
the same subject with different `mode` values should produce
different bundles, and a verifier will refuse to substitute one for
the other.

`internalParameters` is opaque host detail — useful for debugging,
but not part of what the verifier compares against.

`resolvedDependencies` lists the upstream inputs that flowed into
the scan: the OSV.dev snapshot the SCA pipeline matched against,
the slopsquatting corpus version, the rule pack versions. In this
batch the array is empty; it populates as those moving pieces land
behind real release pins.

## Companion artefacts (the "subject")

The bundle by itself is small. What it attests over — the subject
tarball — is the real payload:

- `findings.sarif` — SARIF 2.1.0 with CWE Top 25:2025 + OWASP Top
  10:2025 taxa
- `sbom.cdx.json` — CycloneDX 1.6 SBOM
- `sbom.spdx.json` — SPDX 3.0.1 JSON-LD SBOM
- `vex.openvex.json` — OpenVEX exception document
- `controls.cra.json` etc. — compliance overlays (one per framework)
- `metadata.json` — scanner version, ruleset version, host signature

The pack is intentionally a tar so the entire surface is one
hashable artefact. SHA-256 + BLAKE2b-256 of the tar are what end up
in the Statement's `subject[0].digest`.

## Verification, end to end

```bash
# 1. Get the public key.
#    Either trust the bundle's inline rawBytes:
jq -r '.verificationMaterial.publicKey.rawBytes' \
   euxis-evidence.tar.gz.sigstore.json | base64 -d > pubkey.bin
#    Or resolve the hint against your own KMS / Fulcio root.

# 2. Verify the bundle's signature.
euxis verify euxis-evidence.tar.gz.sigstore.json --key=pubkey.bin

# 3. Verify the subject digest still matches the tar.
sha256sum euxis-evidence.tar.gz   # compare to the Statement's subject.digest.sha256
```

Step 2 is what `euxis verify` runs internally. Step 3 is the bit a
regulator does themselves — it proves the bundle the auditor has
in hand was signed over the same bytes they're holding.

## Why this shape

Three properties make the bundle a credible regulator evidence
artefact:

1. **Offline-verifiable.** No network, no Fulcio, no Rekor required
   to confirm a signature. The verifier needs only the bundle and a
   public key.
2. **Tamper-evident.** Any change to the subject tar invalidates
   `subject[0].digest`. Any change to the predicate JSON invalidates
   the DSSE signature. Any change to the bundle metadata invalidates
   the DSSE-PAE encoding the signature is bound to.
3. **Vendor-neutral.** The Statement, DSSE envelope, and Sigstore
   Bundle are CNCF-managed open specs. A regulator who already
   trusts sigstore-go or cosign to verify a release artefact can
   reuse the same toolchain to verify a euxis evidence pack — no
   custom euxis client required.

## See also

- [verifying-a-bundle.md](verifying-a-bundle.md) — step-by-step run-book
- [SLSA v1.2 Provenance spec](https://slsa.dev/spec/v1.2/provenance)
- [in-toto Attestation Framework v1](https://github.com/in-toto/attestation/blob/main/spec/v1/statement.md)
- [Sigstore Bundle v0.3 protobuf spec](https://github.com/sigstore/protobuf-specs/blob/main/protos/sigstore_bundle.proto)
- [DSSE protocol](https://github.com/secure-systems-lab/dsse/blob/master/protocol.md)
