/// @file
/// @brief Verifiable Credential (VC) support for agent claims.
#pragma once

#include <chrono>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::identity {

/// @brief A single statement/claim made about a subject.
struct Claim {
    std::string type;
    std::string value;
    std::string scope;
};

/// @brief Cryptographic proof attached to a verifiable credential.
struct Proof {
    std::string type;
    std::string created;
    std::string verification_method;
    std::string proof_value;
};

/// @brief A W3C-compliant Verifiable Credential.
struct VerifiableCredential {
    std::string id;
    std::string issuer_did;
    std::string subject_did;
    std::vector<Claim> claims;
    Proof proof;
    std::string issued_at;
    std::string expires_at;

    [[nodiscard]] auto to_json() const -> nlohmann::json;
};

/// @brief Sign and issue a new verifiable credential.
[[nodiscard]] auto issue_credential(
    std::string_view issuer_did,
    std::span<const std::byte> issuer_key,
    std::string_view subject_did,
    std::vector<Claim> claims,
    std::chrono::seconds ttl = std::chrono::hours(24)
) -> VerifiableCredential;

/// @brief Cryptographically verify a credential's proof.
[[nodiscard]] auto verify_credential(
    const VerifiableCredential& credential,
    std::span<const std::byte> issuer_key
) -> bool;

} // namespace euxis::identity
