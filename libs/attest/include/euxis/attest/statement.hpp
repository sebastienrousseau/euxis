/// @file
/// @brief in-toto Statement v1 builder.
#pragma once

#include <expected>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

///
/// Implements the in-toto Attestation Framework Statement v1 layout:
///   https://github.com/in-toto/attestation/blob/main/spec/v1/statement.md
///
/// Shape:
///   {
///     "_type": "https://in-toto.io/Statement/v1",
///     "subject": [
///       {"name": "<resource>", "digest": {"<alg>": "<hex>"}}
///     ],
///     "predicateType": "<URI>",
///     "predicate": { ... }
///   }
///
/// Statements are consumed by DSSE (libs/attest/dsse.hpp) to produce
/// a signed envelope that any cosign / sigstore-go / TUF client can
/// verify offline. Network upload to Rekor v2 lives in a separate
/// follow-up batch.

namespace euxis::attest {

/// Constant `_type` field per the in-toto v1 spec.
inline constexpr const char* kStatementType =
    "https://in-toto.io/Statement/v1";

/// One subject the statement attests over (e.g. an evidence pack
/// tarball). Multiple subjects are allowed; each must carry at least
/// one digest. Algorithm names follow IANA hash-function-text-names
/// (sha256, sha512, blake2b-256, …) so consumers match the same vocab
/// SARIF and CycloneDX already use.
struct Subject {
    std::string name;
    /// Map from algorithm name → hex-encoded digest.
    std::unordered_map<std::string, std::string> digest;
};

/// The Statement itself. `predicate` is opaque to in-toto — its shape
/// is governed by `predicate_type`. For euxis the canonical predicate
/// is SLSA v1.2 provenance (slsa.hpp), but anything that round-trips
/// through nlohmann::json works.
struct Statement {
    std::vector<Subject> subjects;
    std::string predicate_type;
    nlohmann::json predicate;
};

/// Serialise to canonical JSON. The output is the bytes signed by
/// DSSE; callers must not re-encode through a different serialiser
/// or the signature will not verify. `dump(-1)` (no indentation, no
/// trailing whitespace) is what cosign and sigstore-go expect.
[[nodiscard]] auto to_json(const Statement& s) -> nlohmann::json;

/// Serialise to canonical bytes for signing. Equivalent to
/// `to_json(s).dump()` but isolated as a function so any future
/// canonicalisation change (RFC 8785 JCS, for example) has a single
/// touchpoint.
[[nodiscard]] auto serialise_for_signing(const Statement& s) -> std::string;

/// Validation result.
struct StatementError {
    std::string message;
};

/// Validate the structural invariants of a Statement before signing:
/// at least one subject, every subject has a name and at least one
/// non-empty hex digest, the predicateType is a non-empty URI shape.
[[nodiscard]] auto validate(const Statement& s)
    -> std::expected<void, StatementError>;

} // namespace euxis::attest
