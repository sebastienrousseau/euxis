/// @file
/// @brief `euxis attest` and `euxis verify` — produce / check
///        Sigstore-format signed evidence bundles.
#pragma once

#include "euxis/cli/command.hpp"

#include <string>
#include <vector>

namespace euxis::cli::cmd {

/// `euxis attest <subject-file>
///                [--mode=<scan-mode>]
///                [--subject-name=<friendly>]
///                [--key=<path>] [--keygen=<path>]
///                [--bundle=<path>]`
///
/// Computes BLAKE2b-256 + SHA-256 of `<subject-file>`, builds a
/// SLSA v1.2 provenance predicate, wraps it in an in-toto Statement,
/// DSSE-signs with the supplied Ed25519 key, and writes the resulting
/// Sigstore bundle (offline shape; no Rekor upload in this batch).
///
/// `--keygen=<path>` creates a fresh Ed25519 keypair, writes the
/// secret + public to `<path>.priv` / `<path>.pub` (base64), and
/// uses it for the signing step. Useful for first-run.
int cmd_attest(Context& ctx, const std::vector<std::string>& args);

/// `euxis verify <bundle> --key=<path-or-pubkey-b64> [--artifact=<path>]`
///
/// Validates the bundle structure, decodes the DSSE envelope,
/// reconstructs the PAE bytes, and verifies the signature against
/// the trusted public key given with `--key` (required: the key
/// embedded in the bundle is never used). With `--artifact`, also
/// checks the file's sha256 against the signed statement's subjects.
/// Returns Success (0) on a fully-valid bundle, PolicyViolation (4)
/// on a signature, payloadType or digest mismatch, InfraError (1) on
/// malformed input or a missing key. Does not check a transparency log.
int cmd_verify_attest(Context& ctx, const std::vector<std::string>& args);

} // namespace euxis::cli::cmd
