#pragma once

#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace euxis::identity {

/// An attestation from one agent about another, expressing trust or
/// capability confirmation with a confidence score.
struct Attestation {
    std::string id;
    std::string attester_did;
    std::string subject_did;
    std::string attestation_type;
    double confidence;
    std::string created_at;
    std::string evidence;

    /// Serialise the attestation to a JSON object.
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Create an attestation from `attester_did` about `subject_did`.
/// Confidence must be in the range [0.0, 1.0].
/// An optional evidence string provides supporting information.
[[nodiscard]] auto create_attestation(
    std::string_view attester_did,
    std::string_view subject_did,
    std::string_view type,
    double confidence,
    std::string_view evidence = ""
) -> Attestation;

} // namespace euxis::identity
