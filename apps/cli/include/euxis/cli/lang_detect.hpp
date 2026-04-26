/// @file
/// @brief Language and framework detection from repository file patterns.
#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace euxis::cli {

/// Detected language entry.
struct DetectedLanguage {
    std::string name;           // e.g. "C++", "Rust", "Python"
    int file_count{0};          // number of source files
    double confidence{0.0};     // 0.0-1.0
};

/// Detected framework/toolchain entry.
struct DetectedFramework {
    std::string name;           // e.g. "CMake", "Cargo", "npm"
    std::string config_file;    // file that triggered detection
};

/// Combined detection result.
struct LanguageProfile {
    std::vector<DetectedLanguage> languages;
    std::vector<DetectedFramework> frameworks;
    std::string primary_language;  // highest file count

    [[nodiscard]] auto to_json() const -> nlohmann::json;
    [[nodiscard]] auto summary() const -> std::string;
};

/// Detect languages and frameworks in the given directory.
[[nodiscard]] auto detect_languages(const std::string& root_dir) -> LanguageProfile;

} // namespace euxis::cli
