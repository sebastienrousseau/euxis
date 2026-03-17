/// @file
/// @brief Admission pipeline for evaluating external skills.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "skill.hpp"
#include "static_analysis.hpp"
#include "verification.hpp"
#include "policy.hpp"

namespace euxis::bridge {

/// @brief Detailed result of the skill admission process.
struct AdmissionResult {
    bool admitted{false};
    std::vector<std::string> stages_passed;
    std::vector<std::string> stages_failed;
    std::vector<AnalysisFinding> findings;
};

/// @brief Orchestrates multiple checks (signature, static analysis, etc) to admit a skill.
class AdmissionPipeline {
public:
    /// @brief Construct pipeline with a specific reputation threshold.
    explicit AdmissionPipeline(double reputation_threshold = 0.5);

    /// @brief Run the full admission evaluation for a skill.
    auto evaluate(const BridgedSkill& skill,
                  const std::optional<SkillExecutionPolicy>& policy = std::nullopt) -> AdmissionResult;

private:
    double reputation_threshold_;

    auto check_structure(const BridgedSkill& skill) -> bool;
    auto check_static_analysis(const BridgedSkill& skill) -> std::pair<bool, std::vector<AnalysisFinding>>;
    auto check_signature(const BridgedSkill& skill, const std::optional<SkillExecutionPolicy>& policy) -> bool;
};

} // namespace euxis::bridge
