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

/// `euxis verify <bundle> --key=<path-or-pubkey-b64>`
///
/// Validates the bundle structure, decodes the DSSE envelope,
/// reconstructs the PAE bytes, and verifies the signature against
/// the supplied public key. Returns Success (0) on a fully-valid
/// bundle, PolicyViolation (4) on signature mismatch, InfraError
/// (1) on malformed input.
int cmd_verify_attest(Context& ctx, const std::vector<std::string>& args);

} // namespace euxis::cli::cmd
