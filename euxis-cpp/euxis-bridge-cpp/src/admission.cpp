#include <euxis/bridge/admission.hpp>
#include <euxis/bridge/reputation.hpp>
#include <euxis/bridge/static_analysis.hpp>
#include <euxis/bridge/verification.hpp>

namespace euxis::bridge {

AdmissionPipeline::AdmissionPipeline(double reputation_threshold)
    : reputation_threshold_(reputation_threshold) {}

auto AdmissionPipeline::evaluate(
    const BridgedSkill& skill,
    const std::optional<SkillExecutionPolicy>& policy
) -> AdmissionResult {
    AdmissionResult result;
    result.admitted = true;

    // Stage 1: Structure check
    if (check_structure(skill)) {
        result.stages_passed.push_back("structure");
    } else {
        result.stages_failed.push_back("structure");
        result.admitted = false;
    }

    // Stage 2: Static analysis
    auto [analysis_passed, findings] = check_static_analysis(skill);
    result.findings = std::move(findings);
    if (analysis_passed) {
        result.stages_passed.push_back("static_analysis");
    } else {
        result.stages_failed.push_back("static_analysis");
        result.admitted = false;
    }

    // Stage 3: Signature verification (if required by policy)
    if (check_signature(skill, policy)) {
        result.stages_passed.push_back("signature");
    } else {
        result.stages_failed.push_back("signature");
        result.admitted = false;
    }

    return result;
}

auto AdmissionPipeline::check_structure(const BridgedSkill& skill) -> bool {
    if (skill.name.empty()) return false;
    if (skill.runtime.empty()) return false;
    if (skill.entrypoint.empty()) return false;
    if (skill.source_dir.empty()) return false;
    return true;
}

auto AdmissionPipeline::check_static_analysis(const BridgedSkill& skill)
    -> std::pair<bool, std::vector<AnalysisFinding>> {
    SkillStaticAnalyzer analyzer;

    if (!std::filesystem::exists(skill.source_dir)) {
        return {true, {}};  // No source to analyze
    }

    auto report = analyzer.analyze_directory(skill.source_dir);
    return {report.passed, std::move(report.findings)};
}

auto AdmissionPipeline::check_signature(
    const BridgedSkill& skill,
    const std::optional<SkillExecutionPolicy>& policy
) -> bool {
    // If no policy or signature not required, pass
    if (!policy || !policy->require_signature) {
        return true;
    }

    // Signature required but no signature path
    if (!skill.signature_path) {
        return false;
    }

    // Signature required but no public key path
    if (!policy->public_key_path) {
        return false;
    }

    auto key_result = load_public_key(*policy->public_key_path);
    if (!key_result) {
        return false;
    }

    auto verify_result = verify_skill_signature(
        skill.entrypoint, *skill.signature_path, *key_result
    );

    return verify_result && verify_result->valid;
}

}  // namespace euxis::bridge
