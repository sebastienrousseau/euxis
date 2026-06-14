/// @file
/// @brief Sigstore Bundle v0.3 layout (offline shape only).
///
/// The on-disk artefact a euxis evidence pack ships to a regulator
/// is a Sigstore Bundle: one JSON object that carries the DSSE
/// envelope, a hint at the public key used, and (when present)
/// transparency-log inclusion proofs.
///
/// Spec:
///   https://github.com/sigstore/protobuf-specs/blob/main/protos/sigstore_bundle.proto
///   media type: application/vnd.dev.sigstore.bundle.v0.3+json
///
/// This header models the OFFLINE shape — the bits a regulator
/// receives and can verify without contacting any network service.
/// Online Rekor v2 upload + inclusion-proof attachment lives in a
/// follow-up batch.
#pragma once

#include <expected>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "euxis/attest/dsse.hpp"

namespace euxis::attest {

inline constexpr const char* kSigstoreBundleMediaType =
    "application/vnd.dev.sigstore.bundle.v0.3+json";

/// Hint at the public key the verifier should use. For keyless
/// (OIDC-issued) signing this would contain a certificate; for
/// key-backed signing it carries the keyid + optional inline bytes.
struct PublicKeyMaterial {
    /// Free-form hint — the BLAKE2b-256 hex of the public key by
    /// default (derive_keyid() in signing.hpp).
    std::string hint;
    /// Optional inline public key, base64-encoded raw Ed25519 bytes.
    /// Empty when the verifier is expected to resolve via the hint.
    std::string raw_b64;
};

/// One transparency-log entry. The full Rekor v2 entry shape is
/// large; we keep the minimum a verifier needs offline: the entry
/// kind / log index / integrated time / inclusion proof shape.
struct TlogEntry {
    std::int64_t log_index{0};
    std::int64_t integrated_time_unix{0};
    std::string log_id;
    std::string kind_version_kind;
    std::string kind_version_version;
    nlohmann::json inclusion_proof = nlohmann::json::object();
};

/// The bundle.
struct Bundle {
    std::string media_type{kSigstoreBundleMediaType};
    PublicKeyMaterial public_key;
    Envelope dsse_envelope;
    std::vector<TlogEntry> tlog_entries;
};

struct BundleError {
    std::string message;
};

[[nodiscard]] auto to_json(const Bundle& b) -> nlohmann::json;
[[nodiscard]] auto from_json(const nlohmann::json& j)
    -> std::expected<Bundle, BundleError>;

} // namespace euxis::attest
