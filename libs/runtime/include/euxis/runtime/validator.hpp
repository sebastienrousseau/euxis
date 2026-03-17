/// @file
/// @brief Validation of runtime directory structure and data integrity.
#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::runtime {

/// @brief exception thrown when runtime validation fails.
class RuntimeValidationError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// @brief Performs exhaustive checks on the runtime filesystem and state files.
class RuntimeValidator {
public:
    /// @brief Construct validator targeting a runtime root.
    explicit RuntimeValidator(std::filesystem::path runtime_root);

    /// @brief run all validation checks. Throws RuntimeValidationError on failure.
    void validate() const;

private:
    std::filesystem::path root_;

    void ensure_required_files() const;
    void validate_perf_metrics() const;
    void validate_lifecycle_transitions() const;
    void validate_lifecycle_state_files() const;
    void validate_manifest() const;

    auto read_jsonl(const std::filesystem::path& path) const
        -> std::vector<nlohmann::json>;

    void assert_keys(const std::vector<nlohmann::json>& records,
                     const std::vector<std::string>& required,
                     const std::string& context) const;
};

/// @brief Top-level helper to validate a runtime layout.
void validate_runtime_layout(const std::filesystem::path& runtime_root);

} // namespace euxis::runtime
