/// @file
/// @brief evidentiary framework for tracking and verifying agent claims.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::metrics {

/// @brief qualitative grades for evidence (E1 to E5).
enum class EvidenceGrade {
    Verified,   // E1
    Measured,   // E2
    Observed,   // E3
    Inferred,   // E4
    Speculated, // E5
};

/// @brief Convert grade to string representation.
auto grade_to_string(EvidenceGrade g) -> std::string;

/// @brief Parse grade from string representation.
auto grade_from_string(const std::string& s) -> EvidenceGrade;

/// @brief A discrete unit of evidence supporting a claim.
struct Evidence {
    std::string source_file;
    std::optional<int> source_line;
    std::string evidence_type;
    EvidenceGrade grade{};
    std::string content;
    std::string timestamp;
    std::optional<std::string> verification_cmd;
    nlohmann::json metadata;

    /// @brief Generate a unique SHA-256 hash for this evidence.
    auto hash() const -> std::string;
    
    auto to_json() const -> nlohmann::json;
    static auto from_json(const nlohmann::json& j) -> Evidence;
};

/// @brief A high-level statement supported by one or more pieces of evidence.
struct Claim {
    std::string statement;
    std::vector<Evidence> supporting_evidence;
    double confidence{0.0};

    /// @brief determine the strongest evidence grade available for this claim.
    auto highest_grade() const -> std::optional<EvidenceGrade>;
};

/// @brief manages the persistence and verification of evidence and claims.
class EvidenceFramework {
public:
    /// @brief Construct framework targeting a specific directory.
    explicit EvidenceFramework(const std::filesystem::path& evidence_dir = {});

    /// @brief Persist evidence and return its unique hash.
    auto store_evidence(const Evidence& evidence) -> std::string;
    
    /// @brief Load a specific piece of evidence by hash.
    auto load_evidence(const std::string& evidence_hash) -> std::optional<Evidence>;
    
    /// @brief Load all stored evidence units.
    auto load_all_evidence() -> std::vector<Evidence>;

    /// @brief check if evidence has exceeded its grade-specific decay period.
    auto check_decay(const Evidence& evidence) const -> bool;
    
    /// @brief Verify if a claim's evidence satisfies confidence and grade requirements.
    auto verify_claim(const Claim& claim) const -> bool;

    /// @brief Get the configured evidence directory.
    [[nodiscard]] auto evidence_dir() const -> const std::filesystem::path& { return evidence_dir_; }

private:
    std::filesystem::path evidence_dir_;
    std::filesystem::path evidence_db_;
};

} // namespace euxis::metrics
