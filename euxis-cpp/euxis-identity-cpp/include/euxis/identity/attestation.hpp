/// @file
/// @brief Identity attestations for agent peer review.
#pragma once

#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace euxis::identity {

/// @brief A peer-signed statement about an agent's identity or behavior.
struct Attestation {
    std::string id;
    std::string attester_did;
    std::string subject_did;
    std::string attestation_type;
    double confidence;
    std::string created_at;
    std::string evidence;

    [[nodiscard]] auto to_json() const -> nlohmann::json;
};

/// @brief Factory function to create a new attestation.
[[nodiscard]] auto create_attestation(
    std::string_view attester_did,
    std::string_view subject_did,
    std::string_view type,
    double confidence,
    std::string_view evidence = ""
) -> Attestation;

} // namespace euxis::identity
