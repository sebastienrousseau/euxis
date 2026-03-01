#pragma once

#include <optional>
#include <string>
#include <vector>

#include "policy.hpp"
#include "skill.hpp"
#include "static_analysis.hpp"

namespace euxis::bridge {

struct AdmissionResult {
    bool admitted;
    std::vector<std::string> stages_passed;
    std::vector<std::string> stages_failed;
    std::vector<AnalysisFinding> findings;
};

class AdmissionPipeline {
public:
    explicit AdmissionPipeline(double reputation_threshold = 0.3);

    [[nodiscard]] auto evaluate(
        const BridgedSkill& skill,
        const std::optional<SkillExecutionPolicy>& policy = std::nullopt
    ) -> AdmissionResult;

private:
    double reputation_threshold_;

    auto check_structure(const BridgedSkill& skill) -> bool;
    auto check_static_analysis(const BridgedSkill& skill) -> std::pair<bool, std::vector<AnalysisFinding>>;
    auto check_signature(const BridgedSkill& skill, const std::optional<SkillExecutionPolicy>& policy) -> bool;
};

}  // namespace euxis::bridge
