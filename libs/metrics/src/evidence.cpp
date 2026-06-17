#include "euxis/metrics/evidence.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <spdlog/spdlog.h>

namespace euxis::metrics {

auto grade_to_string(EvidenceGrade g) -> std::string {
    switch (g) {
        case EvidenceGrade::Verified: return "E1";
        case EvidenceGrade::Measured: return "E2";
        case EvidenceGrade::Observed: return "E3";
        case EvidenceGrade::Inferred: return "E4";
        case EvidenceGrade::Speculated: return "E5";
    }
    return "E5";
}

auto grade_from_string(const std::string& s) -> EvidenceGrade {
    if (s == "E1") return EvidenceGrade::Verified;
    if (s == "E2") return EvidenceGrade::Measured;
    if (s == "E3") return EvidenceGrade::Observed;
    if (s == "E4") return EvidenceGrade::Inferred;
    return EvidenceGrade::Speculated;
}

auto Evidence::hash() const -> std::string {
    // Simple hash: use content + source_file + timestamp
    std::hash<std::string> hasher;
    auto h = hasher(source_file + ":" + content + ":" + timestamp);
    std::ostringstream oss;
    oss << std::hex << std::setw(12) << std::setfill('0') << (h & 0xFFFFFFFFFFFF);
    return oss.str();
}

auto Evidence::to_json() const -> nlohmann::json {
    nlohmann::json j = {
        {"source_file", source_file},
        {"evidence_type", evidence_type},
        {"grade", grade_to_string(grade)},
        {"content", content},
        {"timestamp", timestamp},
        {"metadata", metadata},
        {"hash", hash()},
    };
    if (source_line) j["source_line"] = *source_line;
    if (verification_cmd) j["verification_cmd"] = *verification_cmd;
    return j;
}

auto Evidence::from_json(const nlohmann::json& j) -> Evidence {
    Evidence e;
    e.source_file = j.value("source_file", "");
    if (j.contains("source_line") && !j["source_line"].is_null()) {
        e.source_line = j["source_line"].get<int>();
    }
    e.evidence_type = j.value("evidence_type", "");
    e.grade = grade_from_string(j.value("grade", "E5"));
    e.content = j.value("content", "");
    e.timestamp = j.value("timestamp", "");
    if (j.contains("verification_cmd") && !j["verification_cmd"].is_null()) {
        e.verification_cmd = j["verification_cmd"].get<std::string>();
    }
    e.metadata = j.value("metadata", nlohmann::json::object());
    return e;
}

auto Claim::highest_grade() const -> std::optional<EvidenceGrade> {
    if (supporting_evidence.empty()) return std::nullopt;
    auto best = supporting_evidence[0].grade;
    for (const auto& e : supporting_evidence) {
        if (static_cast<int>(e.grade) < static_cast<int>(best)) {
            best = e.grade;
        }
    }
    return best;
}

EvidenceFramework::EvidenceFramework(
    const std::filesystem::path& evidence_dir) {
    if (evidence_dir.empty()) {
        const char* home = std::getenv("EUXIS_HOME");
        auto base = home ? std::filesystem::path(home)
                         : std::filesystem::path(
                               std::getenv("HOME") ? std::getenv("HOME") : "/tmp") /
                               ".euxis";
        evidence_dir_ = base / "metrics" / "evidence";
    } else {
        evidence_dir_ = evidence_dir;
    }
    std::filesystem::create_directories(evidence_dir_);
    evidence_db_ = evidence_dir_ / "evidence.jsonl";
}

auto EvidenceFramework::store_evidence(const Evidence& evidence) -> std::string {
    auto j = evidence.to_json();
    std::ofstream f(evidence_db_, std::ios::app);
    f << j.dump() << '\n';
    spdlog::debug("Stored evidence: {}", j["hash"].get<std::string>());
    return j["hash"].get<std::string>();
}

auto EvidenceFramework::load_evidence(const std::string& evidence_hash)
    -> std::optional<Evidence> {
    if (!std::filesystem::exists(evidence_db_)) return std::nullopt;
    std::ifstream f(evidence_db_);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        try {
            auto j = nlohmann::json::parse(line);
            if (j.value("hash", "") == evidence_hash) {
                return Evidence::from_json(j);
            }
        } catch (const std::exception&) {
            continue;
        }
    }
    return std::nullopt;
}

auto EvidenceFramework::load_all_evidence() -> std::vector<Evidence> {
    std::vector<Evidence> result;
    if (!std::filesystem::exists(evidence_db_)) return result;
    std::ifstream f(evidence_db_);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        try {
            result.push_back(Evidence::from_json(nlohmann::json::parse(line)));
        } catch (const std::exception&) {
            continue;
        }
    }
    return result;
}

auto EvidenceFramework::check_decay(const Evidence& evidence) const -> bool {
    // E5 evidence is always decayed (forbidden)
    if (evidence.grade == EvidenceGrade::Speculated) return true;

    // Function-local static: lazy-init avoids throwing during static-init phase
    // (bugprone-throwing-static-initialization).
    static const std::unordered_map<EvidenceGrade, int> kDecayDays = {
        {EvidenceGrade::Verified, 1},
        {EvidenceGrade::Measured, 1},
        {EvidenceGrade::Observed, 7},
        {EvidenceGrade::Inferred, 30},
    };

    auto it = kDecayDays.find(evidence.grade);
    if (it == kDecayDays.end()) return true;

    // Parse ISO-8601 timestamp (YYYY-MM-DDTHH:MM:SS or YYYY-MM-DD)
    std::tm tm{};
    std::istringstream ss(evidence.timestamp);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (ss.fail()) {
        ss.clear();
        ss.str(evidence.timestamp);
        ss >> std::get_time(&tm, "%Y-%m-%d");
        if (ss.fail()) return true;
    }

    auto evidence_time = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    auto now = std::chrono::system_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::hours>(
                   now - evidence_time).count() / 24;
    return age > it->second;
}

auto EvidenceFramework::verify_claim(const Claim& claim) const -> bool {
    if (claim.supporting_evidence.empty()) return false;
    // Must have at least E1-E3 grade evidence
    auto grade = claim.highest_grade();
    if (!grade) return false;
    if (static_cast<int>(*grade) > static_cast<int>(EvidenceGrade::Observed)) {
        return false;
    }
    // Check confidence threshold
    return claim.confidence >= 0.5;
}

} // namespace euxis::metrics
