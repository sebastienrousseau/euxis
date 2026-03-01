#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace euxis::bridge {

enum class Severity { Critical, Warning, Info };

struct AnalysisFinding {
    Severity severity;
    std::string pattern;
    std::string location;
    std::string description;
};

struct AnalysisReport {
    std::vector<AnalysisFinding> findings;
    bool passed;
    size_t scanned_files;
};

class SkillStaticAnalyzer {
public:
    [[nodiscard]] auto analyze_file(const std::filesystem::path& path)
        -> std::vector<AnalysisFinding>;
    [[nodiscard]] auto analyze_directory(const std::filesystem::path& dir)
        -> AnalysisReport;
};

}  // namespace euxis::bridge
