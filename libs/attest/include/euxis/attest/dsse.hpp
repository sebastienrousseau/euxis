/// @file
/// @brief DSSE (Dead Simple Signing Envelope) PAE + envelope.
///
/// Implements DSSE v1 per
///   https://github.com/secure-systems-lab/dsse/blob/master/protocol.md
///
/// PAE (Pre-Authentication Encoding) prevents type confusion between
/// the payload type URI and the payload bytes. Signatures are over
/// PAE(payloadType, payload) rather than payload directly, so an
/// attacker cannot reuse a signed body under a different type.
///
/// Layout:
///   PAE(type, body) = "DSSEv1 "    (literal, with trailing space)
///                   + LEN(type)     (ASCII decimal byte count)
///                   + " "
///                   + type
///                   + " "
///                   + LEN(body)
///                   + " "
///                   + body
///
/// Envelope shape:
///   {
///     "payloadType": "<type-URI>",
///     "payload": "<base64-of-payload>",
///     "signatures": [{"keyid": "<id>", "sig": "<base64-of-signature>"}]
///   }
#pragma once

#include <expected>
#include <nlohmann/json.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace euxis::attest {

/// Canonical payload type for in-toto Statement v1.
inline constexpr const char* kDsseInTotoPayloadType =
    "application/vnd.in-toto+json";

struct Signature {
    /// Free-form key identifier the verifier uses to look up the
    /// public key. Often the hex digest of the public key.
    std::string keyid;
    /// Base64-encoded signature bytes.
    std::string sig;
};

struct Envelope {
    std::string payload_type;
    /// Base64-encoded payload bytes.
    std::string payload;
    std::vector<Signature> signatures;
};

struct DsseError {
    std::string message;
};

/// Compute the PAE bytes for `(payload_type, payload)`. This is the
/// authenticated input to the signer; subsequent verification feeds
/// the same bytes back to the signature checker.
[[nodiscard]] auto pae(std::string_view payload_type,
                       std::span<const std::byte> payload) -> std::string;

/// Convenience: PAE from string-shaped payload.
[[nodiscard]] auto pae(std::string_view payload_type,
                       std::string_view payload) -> std::string;

/// Standard base64 (RFC 4648 §4) encode/decode helpers. DSSE uses
/// the standard alphabet with `=` padding; sigstore tools accept
/// both standard and URL-safe but we emit the spec-canonical form.
[[nodiscard]] auto base64_encode(std::span<const std::byte> data) -> std::string;
[[nodiscard]] auto base64_encode(std::string_view s) -> std::string;
[[nodiscard]] auto base64_decode(std::string_view encoded)
    -> std::expected<std::vector<std::byte>, DsseError>;

/// Serialise / deserialise an Envelope to/from JSON.
[[nodiscard]] auto to_json(const Envelope& env) -> nlohmann::json;
[[nodiscard]] auto from_json(const nlohmann::json& j)
    -> std::expected<Envelope, DsseError>;

} // namespace euxis::attest
