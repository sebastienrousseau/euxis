/// @file
/// @brief Static analysis for imported skills to detect suspicious patterns.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace euxis::bridge {

/// @brief Severity level of an analysis finding.
enum class Severity {
    Info,
    Warning,
    Critical
};

/// @brief A single finding from static analysis.
struct AnalysisFinding {
    Severity severity;
    std::string pattern;
    std::string location;
    std::string description;
};

/// @brief Summary report of a static analysis run.
struct AnalysisReport {
    bool passed{true};
    std::vector<AnalysisFinding> findings;
    int scanned_files{0};
};

/// @brief Analyzer that scans skill files for security and quality issues.
class SkillStaticAnalyzer {
public:
    /// @brief Analyze a single file.
    auto analyze_file(const std::filesystem::path& path) -> std::vector<AnalysisFinding>;
    
    /// @brief Recursively analyze a directory of skill files.
    auto analyze_directory(const std::filesystem::path& dir) -> AnalysisReport;
};

} // namespace euxis::bridge
