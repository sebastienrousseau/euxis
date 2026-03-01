#include "euxis/runtime/validator.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace euxis::runtime {

RuntimeValidator::RuntimeValidator(std::filesystem::path runtime_root)
    : root_(std::move(runtime_root)) {}

void RuntimeValidator::validate() const {
    ensure_required_files();
    validate_perf_metrics();
    validate_lifecycle_transitions();
    validate_lifecycle_state_files();
    validate_manifest();
}

void RuntimeValidator::ensure_required_files() const {
    std::vector<std::filesystem::path> required{
        root_ / "README.md",
        root_ / "config" / "etx-settings.json",
        root_ / "data" / "perf" / "metrics.jsonl",
        root_ / "data" / "lifecycle" / "transitions.jsonl",
        root_ / "data" / "manifests" / "performance-optimization.json",
    };

    std::vector<std::string> missing;
    for (const auto& p : required) {
        if (!std::filesystem::exists(p)) {
            missing.push_back(p.string());
        }
    }
    if (!missing.empty()) {
        std::string msg = "Missing runtime assets: ";
        for (size_t i = 0; i < missing.size(); ++i) {
            if (i > 0) msg += ", ";
            msg += missing[i];
        }
        throw RuntimeValidationError(msg);
    }
}

auto RuntimeValidator::read_jsonl(const std::filesystem::path& path) const
    -> std::vector<nlohmann::json> {
    std::ifstream file(path);
    if (!file) throw RuntimeValidationError("Cannot open: " + path.string());

    std::vector<nlohmann::json> records;
    std::string line;
    int lineno = 0;
    while (std::getline(file, line)) {
        ++lineno;
        // Skip blank lines
        if (line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
        try {
            auto obj = nlohmann::json::parse(line);
            if (!obj.is_object()) {
                throw RuntimeValidationError(
                    path.string() + ":" + std::to_string(lineno) +
                    ": record is not an object");
            }
            records.push_back(std::move(obj));
        } catch (const nlohmann::json::parse_error&) {
            throw RuntimeValidationError(
                path.string() + ":" + std::to_string(lineno) +
                ": invalid jsonl record");
        }
    }
    if (records.empty()) {
        throw RuntimeValidationError(path.string() + ": no records");
    }
    return records;
}

void RuntimeValidator::assert_keys(const std::vector<nlohmann::json>& records,
                                   const std::vector<std::string>& required,
                                   const std::string& context) const {
    for (size_t idx = 0; idx < records.size(); ++idx) {
        std::vector<std::string> missing;
        for (const auto& key : required) {
            if (!records[idx].contains(key)) {
                missing.push_back(key);
            }
        }
        if (!missing.empty()) {
            std::ranges::sort(missing);
            std::string keys_str;
            for (size_t i = 0; i < missing.size(); ++i) {
                if (i > 0) keys_str += ", ";
                keys_str += missing[i];
            }
            throw RuntimeValidationError(
                context + "[" + std::to_string(idx) + "] missing keys: " + keys_str);
        }
    }
}

void RuntimeValidator::validate_perf_metrics() const {
    auto path = root_ / "data" / "perf" / "metrics.jsonl";
    auto records = read_jsonl(path);
    assert_keys(records, {"ts", "op", "agent", "ms"}, path.string());

    for (size_t idx = 0; idx < records.size(); ++idx) {
        const auto& ms = records[idx]["ms"];
        if (!ms.is_number() || ms.get<double>() < 0) {
            throw RuntimeValidationError(
                path.string() + "[" + std::to_string(idx) + "] invalid ms value");
        }
    }
}

void RuntimeValidator::validate_lifecycle_transitions() const {
    auto path = root_ / "data" / "lifecycle" / "transitions.jsonl";
    auto records = read_jsonl(path);
    assert_keys(records, {"ts", "agent", "state", "session"}, path.string());
}

void RuntimeValidator::validate_lifecycle_state_files() const {
    auto lifecycle_dir = root_ / "data" / "lifecycle";
    std::vector<std::filesystem::path> state_files;
    for (const auto& entry : std::filesystem::directory_iterator(lifecycle_dir)) {
        if (entry.path().extension() == ".state") {
            state_files.push_back(entry.path());
        }
    }
    if (state_files.empty()) {
        throw RuntimeValidationError(
            lifecycle_dir.string() + ": no .state files found");
    }
    std::ranges::sort(state_files);
    for (const auto& path : state_files) {
        std::ifstream file(path);
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        // Trim
        auto start = content.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            throw RuntimeValidationError(path.string() + ": empty state file");
        }
    }
}

void RuntimeValidator::validate_manifest() const {
    auto path = root_ / "data" / "manifests" / "performance-optimization.json";
    std::ifstream file(path);
    if (!file) throw RuntimeValidationError("Cannot open: " + path.string());

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    try {
        auto data = nlohmann::json::parse(content);
        if (!data.is_object()) {
            throw RuntimeValidationError(
                path.string() + ": manifest must be an object");
        }
    } catch (const nlohmann::json::parse_error&) {
        throw RuntimeValidationError(path.string() + ": invalid json");
    }
}

void validate_runtime_layout(const std::filesystem::path& runtime_root) {
    RuntimeValidator(runtime_root).validate();
}

} // namespace euxis::runtime
