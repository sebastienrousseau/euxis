#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "errors.hpp"

namespace euxis::runtime {

class RuntimeValidator {
public:
    explicit RuntimeValidator(std::filesystem::path runtime_root);

    /// Run all validation checks.  Throws RuntimeValidationError on failure.
    void validate() const;

    void ensure_required_files() const;
    void validate_perf_metrics() const;
    void validate_lifecycle_transitions() const;
    void validate_lifecycle_state_files() const;
    void validate_manifest() const;

private:
    std::filesystem::path root_;

    [[nodiscard]] auto read_jsonl(const std::filesystem::path& path) const
        -> std::vector<nlohmann::json>;

    void assert_keys(const std::vector<nlohmann::json>& records,
                     const std::vector<std::string>& required,
                     const std::string& context) const;
};

/// Convenience free-function.
void validate_runtime_layout(const std::filesystem::path& runtime_root);

} // namespace euxis::runtime
