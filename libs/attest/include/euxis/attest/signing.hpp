/// @file
/// @brief Ed25519 sign / verify for DSSE envelopes.
///
/// Sits between the in-toto Statement layer (statement.hpp +
/// slsa.hpp) and the on-the-wire DSSE format (dsse.hpp). Uses the
/// Ed25519 primitive from libs/crypto so the same hardened sodium
/// build supplies both AES-GCM keys and signing keys.
#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <span>
#include <string>

#include "euxis/attest/dsse.hpp"
#include "euxis/attest/statement.hpp"

namespace euxis::attest {

struct SigningError {
    std::string message;
};

/// Produce a DSSE envelope by serialising the Statement, encoding
/// its bytes in PAE, signing with Ed25519, and base64-encoding the
/// payload + signature. The resulting envelope verifies under any
/// DSSE-compliant verifier (cosign, sigstore-go, in-toto/witness).
///
/// `secret_key` is the 64-byte Ed25519 secret key produced by
/// libs/crypto/ed25519.hpp::generate_keypair().
/// `keyid` is a free-form identifier the verifier uses to select
/// the right public key — euxis emits the hex BLAKE2b-256 of the
/// public key by default.
[[nodiscard]] auto sign_statement(
    const Statement& statement,
    std::span<const std::byte, 64> secret_key,
    std::string keyid)
    -> std::expected<Envelope, SigningError>;

/// Verify a DSSE envelope. Decodes the base64 payload, reconstructs
/// the PAE bytes, and checks every signature against the supplied
/// public key. Returns success iff *at least one* signature matches
/// — DSSE explicitly supports multi-signer envelopes.
///
/// Returns the decoded payload bytes on success so callers can
/// parse the Statement without re-decoding.
[[nodiscard]] auto verify_envelope(
    const Envelope& envelope,
    std::span<const std::byte, 32> public_key)
    -> std::expected<std::string, SigningError>;

/// Derive a stable keyid from an Ed25519 public key. Uses BLAKE2b-256
/// (via the existing libs/cache primitive) so the format matches the
/// rest of the euxis evidence pipeline. Returned as a 64-char hex
/// string.
[[nodiscard]] auto derive_keyid(std::span<const std::byte, 32> public_key)
    -> std::string;

} // namespace euxis::attest
