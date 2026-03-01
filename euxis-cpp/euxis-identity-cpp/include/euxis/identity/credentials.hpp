#pragma once

#include <chrono>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::identity {

/// A single claim within a verifiable credential.
struct Claim {
    std::string type;
    std::string value;
    std::string scope;
};

/// Cryptographic proof attached to a verifiable credential.
/// Uses HMAC-SHA256 via libsodium for signature generation.
struct Proof {
    std::string type = "HmacSha256Signature2026";
    std::string created;
    std::string verification_method;
    std::string proof_value;
};

/// A W3C Verifiable Credential binding claims to a subject,
/// signed by an issuer using HMAC-SHA256.
struct VerifiableCredential {
    std::string id;
    std::string issuer_did;
    std::string subject_did;
    std::vector<Claim> claims;
    Proof proof;
    std::string issued_at;
    std::string expires_at;

    /// Serialise the credential to a JSON object.
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Issue a verifiable credential from an issuer to a subject with the given
/// claims. The proof is generated using HMAC-SHA256 with the issuer's key.
/// The credential expires after `ttl` seconds from issuance.
[[nodiscard]] auto issue_credential(
    std::string_view issuer_did,
    std::span<const std::byte> issuer_key,
    std::string_view subject_did,
    std::vector<Claim> claims,
    std::chrono::seconds ttl = std::chrono::seconds{3600}
) -> VerifiableCredential;

/// Verify the HMAC-SHA256 proof on a credential using the issuer's key.
/// Returns true if the proof is valid, false otherwise.
/// Uses constant-time comparison via sodium_memcmp.
[[nodiscard]] auto verify_credential(
    const VerifiableCredential& credential,
    std::span<const std::byte> issuer_key
) -> bool;

} // namespace euxis::identity
