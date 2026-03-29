#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace euxis::cli {

struct PlaybookValidationResult {
    bool valid{true};
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

/// Validate a playbook JSON against the euxis playbook schema.
[[nodiscard]] auto validate_playbook(const nlohmann::json& playbook) -> PlaybookValidationResult;

/// Check if a schema version string is compatible with the current version.
[[nodiscard]] auto is_compatible_version(const std::string& version) -> bool;

} // namespace euxis::cli
