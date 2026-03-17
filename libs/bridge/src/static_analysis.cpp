#include <euxis/bridge/static_analysis.hpp>

#include <fstream>
#include <regex>
#include <sstream>

namespace euxis::bridge {

namespace {

struct Pattern {
    std::regex regex;
    Severity severity;
    std::string name;
    std::string description;
};

auto get_patterns() -> const std::vector<Pattern>& {
    static const std::vector<Pattern> patterns = {
        // Critical: command execution
        {std::regex(R"(eval\s*\()"), Severity::Critical, "eval",
         "Dynamic code execution via eval()"},
        {std::regex(R"(exec\s*\()"), Severity::Critical, "exec",
         "Dynamic code execution via exec()"},
        {std::regex(R"(subprocess\.(call|run|Popen|check_output))"),
         Severity::Critical, "subprocess", "Subprocess execution"},
        {std::regex(R"(os\.system\s*\()"), Severity::Critical, "os.system",
         "OS command execution"},
        {std::regex(R"(child_process)"), Severity::Critical, "child_process",
         "Node.js child process execution"},
        {std::regex(R"(require\s*\(\s*['"]child_process['"]\s*\))"),
         Severity::Critical, "require_child_process",
         "Node.js require('child_process')"},
        {std::regex(R"(execSync|spawnSync|execFileSync)"),
         Severity::Critical, "sync_exec", "Synchronous process execution"},

        // Critical: network exfiltration
        {std::regex(R"(fetch\s*\()"), Severity::Warning, "fetch",
         "Network fetch call"},
        {std::regex(R"(XMLHttpRequest)"), Severity::Warning, "xhr",
         "XMLHttpRequest usage"},
        {std::regex(R"(\.connect\s*\()"), Severity::Warning, "connect",
         "Network connection attempt"},
        {std::regex(R"(socket\.)"), Severity::Warning, "socket",
         "Raw socket usage"},

        // Critical: file system manipulation
        {std::regex(R"(fs\.writeFile|fs\.unlink|fs\.rmdir)"),
         Severity::Critical, "fs_write", "File system write/delete operations"},
        {std::regex(R"(os\.remove|os\.unlink|shutil\.rmtree)"),
         Severity::Critical, "py_fs_write", "Python file system destructive operations"},

        // Warning: environment access
        {std::regex(R"(process\.env)"), Severity::Warning, "process_env",
         "Environment variable access"},
        {std::regex(R"(os\.environ)"), Severity::Warning, "os_environ",
         "Python environment variable access"},

        // Warning: dynamic imports
        {std::regex(R"(__import__\s*\()"), Severity::Warning, "__import__",
         "Dynamic Python import"},
        {std::regex(R"(importlib)"), Severity::Warning, "importlib",
         "Dynamic import via importlib"},

        // Info: potential concerns
        {std::regex(R"(crypto|bcrypt|hashlib)"), Severity::Info, "crypto_usage",
         "Cryptographic library usage"},
        {std::regex(R"(base64\.(encode|decode|b64encode|b64decode))"),
         Severity::Info, "base64", "Base64 encoding/decoding"},
    };
    return patterns;
}

auto is_analyzable(const std::filesystem::path& path) -> bool {
    auto ext = path.extension().string();
    return ext == ".js" || ext == ".ts" || ext == ".py" || ext == ".mjs"
        || ext == ".cjs" || ext == ".jsx" || ext == ".tsx";
}

}  // namespace

auto SkillStaticAnalyzer::analyze_file(const std::filesystem::path& path)
    -> std::vector<AnalysisFinding> {
    std::vector<AnalysisFinding> findings;

    if (!is_analyzable(path)) {
        return findings;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        return findings;
    }

    std::string line;
    size_t line_num = 0;
    while (std::getline(file, line)) {
        ++line_num;
        for (const auto& pattern : get_patterns()) {
            if (std::regex_search(line, pattern.regex)) {
                findings.push_back({
                    .severity = pattern.severity,
                    .pattern = pattern.name,
                    .location = path.string() + ":" + std::to_string(line_num),
                    .description = pattern.description,
                });
            }
        }
    }

    return findings;
}

auto SkillStaticAnalyzer::analyze_directory(const std::filesystem::path& dir)
    -> AnalysisReport {
    AnalysisReport report;
    report.scanned_files = 0;
    report.passed = true;

    if (!std::filesystem::exists(dir)) {
        return report;
    }

    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        if (!is_analyzable(entry.path())) continue;

        ++report.scanned_files;
        auto file_findings = analyze_file(entry.path());
        for (auto& finding : file_findings) {
            if (finding.severity == Severity::Critical) {
                report.passed = false;
            }
            report.findings.push_back(std::move(finding));
        }
    }

    return report;
}

}  // namespace euxis::bridge
