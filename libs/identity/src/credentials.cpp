#include "euxis/identity/credentials.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <format>

#include <sodium.h>
#include <spdlog/spdlog.h>

namespace euxis::identity {

namespace {

/// Format a time_point as an ISO-8601 UTC timestamp "YYYY-MM-DDTHH:MM:SSZ".
[[nodiscard]] auto format_iso8601(std::chrono::system_clock::time_point tp) -> std::string {
    const auto tt = std::chrono::system_clock::to_time_t(tp);
    std::tm utc{};
    gmtime_r(&tt, &utc);
    char buf[32];
    (void) std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return std::string{buf};
}

/// Generate a random hex string of the form "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
/// using libsodium's randombytes_buf for cryptographically secure randomness.
[[nodiscard]] auto generate_credential_id() -> std::string {
    std::array<unsigned char, 16> buf{};
    randombytes_buf(buf.data(), buf.size());

    // Format as UUID-like hex: 8-4-4-4-12
    char hex[37];
    (void) std::snprintf(hex, sizeof(hex),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        buf[0], buf[1], buf[2], buf[3],
        buf[4], buf[5],
        buf[6], buf[7],
        buf[8], buf[9],
        buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
    return std::string{hex};
}

/// Serialise a list of claims to canonical JSON for HMAC signing.
[[nodiscard]] auto claims_to_canonical_json(
    std::string_view issuer_did,
    std::string_view subject_did,
    const std::vector<Claim>& claims,
    std::string_view issued_at,
    std::string_view expires_at
) -> std::string {
    auto j = nlohmann::json{
        {"issuer", issuer_did},
        {"subject", subject_did},
        {"issued_at", issued_at},
        {"expires_at", expires_at},
    };

    auto claims_arr = nlohmann::json::array();
    for (const auto& c : claims) {
        claims_arr.push_back({
            {"type", c.type},
            {"value", c.value},
            {"scope", c.scope},
        });
    }
    j["claims"] = std::move(claims_arr);

    // dump with sorted keys for canonical form
    return j.dump(-1, ' ', false, nlohmann::json::error_handler_t::strict);
}

/// Compute HMAC-SHA256 of `data` with `key` and return the hex-encoded result.
[[nodiscard]] auto hmac_sha256_hex(
    std::span<const std::byte> key,
    std::string_view data
) -> std::string {
    // libsodium's crypto_auth_hmacsha256 requires a 32-byte key.
    // We pad or hash the key to 32 bytes using crypto_generichash if needed.
    std::array<unsigned char, 32> key_bytes{};
    if (key.size() == 32) {
        std::memcpy(key_bytes.data(), key.data(), 32);
    } else {
        // Hash the key to 32 bytes
        crypto_generichash(
            key_bytes.data(), 32,
            reinterpret_cast<const unsigned char*>(key.data()), key.size(),
            nullptr, 0
        );
    }

    std::array<unsigned char, crypto_auth_hmacsha256_BYTES> mac{};
    crypto_auth_hmacsha256(
        mac.data(),
        reinterpret_cast<const unsigned char*>(data.data()),
        data.size(),
        key_bytes.data()
    );

    // Hex-encode the MAC
    const auto hex_len = crypto_auth_hmacsha256_BYTES * 2 + 1;
    std::string hex(hex_len, '\0');
    sodium_bin2hex(hex.data(), hex.size(), mac.data(), mac.size());
    hex.resize(std::strlen(hex.c_str()));
    return hex;
}

/// Compute raw HMAC-SHA256 bytes for constant-time comparison.
[[nodiscard]] auto hmac_sha256_raw(
    std::span<const std::byte> key,
    std::string_view data
) -> std::array<unsigned char, crypto_auth_hmacsha256_BYTES> {
    std::array<unsigned char, 32> key_bytes{};
    if (key.size() == 32) {
        std::memcpy(key_bytes.data(), key.data(), 32);
    } else {
        crypto_generichash(
            key_bytes.data(), 32,
            reinterpret_cast<const unsigned char*>(key.data()), key.size(),
            nullptr, 0
        );
    }

    std::array<unsigned char, crypto_auth_hmacsha256_BYTES> mac{};
    crypto_auth_hmacsha256(
        mac.data(),
        reinterpret_cast<const unsigned char*>(data.data()),
        data.size(),
        key_bytes.data()
    );
    return mac;
}

} // namespace

auto issue_credential(
    std::string_view issuer_did,
    std::span<const std::byte> issuer_key,
    std::string_view subject_did,
    std::vector<Claim> claims,
    std::chrono::seconds ttl
) -> VerifiableCredential {
    const auto now = std::chrono::system_clock::now();
    const auto issued_at = format_iso8601(now);
    const auto expires_at = format_iso8601(now + ttl);
    const auto cred_id = generate_credential_id();

    const auto canonical = claims_to_canonical_json(
        issuer_did, subject_did, claims, issued_at, expires_at);
    const auto proof_value = hmac_sha256_hex(issuer_key, canonical);

    Proof proof{
        .type = "HmacSha256Signature2026",
        .created = issued_at,
        .verification_method = std::format("{}#keys-1", issuer_did),
        .proof_value = proof_value,
    };

    VerifiableCredential cred{
        .id = cred_id,
        .issuer_did = std::string{issuer_did},
        .subject_did = std::string{subject_did},
        .claims = std::move(claims),
        .proof = std::move(proof),
        .issued_at = issued_at,
        .expires_at = expires_at,
    };

    spdlog::debug("Issued credential {} from {} to {}", cred_id, issuer_did, subject_did);
    return cred;
}

auto verify_credential(
    const VerifiableCredential& credential,
    std::span<const std::byte> issuer_key
) -> bool {
    const auto canonical = claims_to_canonical_json(
        credential.issuer_did,
        credential.subject_did,
        credential.claims,
        credential.issued_at,
        credential.expires_at
    );

    const auto expected_mac = hmac_sha256_raw(issuer_key, canonical);

    // Decode the hex proof_value to raw bytes for constant-time comparison
    const auto& hex_proof = credential.proof.proof_value;
    if (hex_proof.size() != crypto_auth_hmacsha256_BYTES * 2) {
        spdlog::warn("Credential {} has invalid proof length", credential.id);
        return false;
    }

    std::array<unsigned char, crypto_auth_hmacsha256_BYTES> actual_mac{};
    if (sodium_hex2bin(
            actual_mac.data(), actual_mac.size(),
            hex_proof.c_str(), hex_proof.size(),
            nullptr, nullptr, nullptr) != 0) {
        spdlog::warn("Credential {} has malformed hex proof", credential.id);
        return false;
    }

    // Constant-time comparison to prevent timing attacks
    const auto result = sodium_memcmp(expected_mac.data(), actual_mac.data(),
                                       crypto_auth_hmacsha256_BYTES);
    if (result != 0) {
        spdlog::warn("Credential {} proof verification failed", credential.id);
        return false;
    }

    spdlog::debug("Credential {} verified successfully", credential.id);
    return true;
}

auto VerifiableCredential::to_json() const -> nlohmann::json {
    auto claims_arr = nlohmann::json::array();
    for (const auto& c : claims) {
        claims_arr.push_back({
            {"type", c.type},
            {"value", c.value},
            {"scope", c.scope},
        });
    }

    return nlohmann::json{
        {"id", id},
        {"issuer", issuer_did},
        {"subject", subject_did},
        {"claims", std::move(claims_arr)},
        {"proof", {
            {"type", proof.type},
            {"created", proof.created},
            {"verificationMethod", proof.verification_method},
            {"proofValue", proof.proof_value},
        }},
        {"issuedAt", issued_at},
        {"expiresAt", expires_at},
    };
}

} // namespace euxis::identity
