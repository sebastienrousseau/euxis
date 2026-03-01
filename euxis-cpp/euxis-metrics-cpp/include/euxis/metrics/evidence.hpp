#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::metrics {

/// Evidence grade hierarchy (E1=highest, E5=forbidden).
enum class EvidenceGrade {
    Verified,   // E1: automated tool output, test result, direct observation
    Measured,   // E2: profiling, benchmarking, metric collection
    Observed,   // E3: seen in source/logs/docs, not independently verified
    Inferred,   // E4: logically deduced, not directly confirmed
    Speculated, // E5: FORBIDDEN — guesses without supporting observations
};

auto grade_to_string(EvidenceGrade g) -> std::string;
auto grade_from_string(const std::string& s) -> EvidenceGrade;

struct Evidence {
    std::string source_file;
    std::optional<int> source_line;
    std::string evidence_type; // "measurement", "observation", "test_result", etc.
    EvidenceGrade grade;
    std::string content;
    std::string timestamp; // ISO-8601
    std::optional<std::string> verification_cmd;
    nlohmann::json metadata;

    [[nodiscard]] auto hash() const -> std::string;
    [[nodiscard]] auto to_json() const -> nlohmann::json;
    static auto from_json(const nlohmann::json& j) -> Evidence;
};

struct Claim {
    std::string statement;
    nlohmann::json value; // number or string
    std::optional<std::string> unit;
    std::string context;
    std::vector<Evidence> supporting_evidence;
    double confidence{0.0}; // 0.0–1.0
    bool verified{false};

    [[nodiscard]] auto highest_grade() const -> std::optional<EvidenceGrade>;
};

class EvidenceFramework {
public:
    explicit EvidenceFramework(
        const std::filesystem::path& evidence_dir = {});

    auto store_evidence(const Evidence& evidence) -> std::string;
    auto load_evidence(const std::string& evidence_hash) -> std::optional<Evidence>;
    auto load_all_evidence() -> std::vector<Evidence>;

    auto check_decay(const Evidence& evidence) const -> bool;
    auto verify_claim(const Claim& claim) const -> bool;

    [[nodiscard]] auto evidence_dir() const -> const std::filesystem::path& {
        return evidence_dir_;
    }

private:
    std::filesystem::path evidence_dir_;
    std::filesystem::path evidence_db_;

    static const std::unordered_map<EvidenceGrade, int> decay_days_;
};

} // namespace euxis::metrics
