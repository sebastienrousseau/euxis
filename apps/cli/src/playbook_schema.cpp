#include "euxis/cli/playbook_schema.hpp"

#include <regex>
#include <set>

namespace euxis::cli {

auto is_compatible_version(const std::string& version) -> bool {
    return version == "1.0" || version == "1.0.0" || version.starts_with("1.");
}

auto validate_playbook(const nlohmann::json& playbook) -> PlaybookValidationResult {
    PlaybookValidationResult result;

    // Check for steps or phases (legacy format uses "steps", new format uses "phases")
    bool has_steps = playbook.contains("steps") && playbook["steps"].is_array();
    bool has_phases = playbook.contains("phases") && playbook["phases"].is_array();

    if (!has_steps && !has_phases) {
        result.valid = false;
        result.errors.push_back("playbook must contain 'steps' or 'phases' array");
    }

    // Optional but recommended fields
    if (!playbook.contains("name") && !playbook.contains("id")) {
        result.warnings.push_back("playbook should have a 'name' or 'id' field");
    }

    // Validate ID format if present
    if (playbook.contains("id")) {
        std::string id = playbook["id"].get<std::string>();
        static const std::regex id_pattern("[a-z0-9][a-z0-9-]*[a-z0-9]|[a-z0-9]");
        if (!std::regex_match(id, id_pattern)) {
            result.warnings.push_back("playbook id should match pattern [a-z0-9-]+");
        }
    }

    // Schema version compatibility
    if (playbook.contains("schema_version")) {
        std::string ver = playbook["schema_version"].get<std::string>();
        if (!is_compatible_version(ver)) {
            result.valid = false;
            result.errors.push_back("incompatible schema_version: " + ver);
        }
    }

    // Validate steps
    if (has_steps) {
        const auto& steps = playbook["steps"];
        if (steps.empty()) {
            result.warnings.push_back("steps array is empty");
        }
        std::set<std::string> step_names;
        for (size_t i = 0; i < steps.size(); ++i) {
            const auto& step = steps[i];
            if (!step.is_object()) {
                result.valid = false;
                result.errors.push_back("step " + std::to_string(i) + " is not an object");
                continue;
            }
            // Each step should have an agent or task
            if (!step.contains("agent") && !step.contains("task") && !step.contains("task_template")) {
                result.warnings.push_back("step " + std::to_string(i) + " has no 'agent' or 'task'");
            }
            // Check for duplicate names
            if (step.contains("name")) {
                auto name = step["name"].get<std::string>();
                if (step_names.count(name)) {
                    result.warnings.push_back("duplicate step name: " + name);
                }
                step_names.insert(name);
            }
        }
    }

    // Validate phases (new format)
    if (has_phases) {
        const auto& phases = playbook["phases"];
        if (phases.empty()) {
            result.warnings.push_back("phases array is empty");
        }
        std::set<std::string> gate_names;
        int prev_sequence = -1;
        for (size_t i = 0; i < phases.size(); ++i) {
            const auto& phase = phases[i];
            if (!phase.is_object()) {
                result.valid = false;
                result.errors.push_back("phase " + std::to_string(i) + " is not an object");
                continue;
            }
            // Check sequential phase numbers (accept both "sequence" and "phase" keys)
            auto seq_key = phase.contains("sequence") ? "sequence" : (phase.contains("phase") ? "phase" : nullptr);
            if (seq_key) {
                int seq = phase[seq_key].get<int>();
                if (seq <= prev_sequence) {
                    result.warnings.push_back("phase " + std::to_string(i) + " has non-sequential number");
                }
                prev_sequence = seq;
            }
            // Check delegates
            if (phase.contains("delegates") && phase["delegates"].is_array()) {
                for (size_t d = 0; d < phase["delegates"].size(); ++d) {
                    const auto& delegate = phase["delegates"][d];
                    if (!delegate.contains("agent")) {
                        result.warnings.push_back("phase " + std::to_string(i) + " delegate " + std::to_string(d) + " has no 'agent'");
                    }
                    if (!delegate.contains("task_template") && !delegate.contains("task")) {
                        result.warnings.push_back("phase " + std::to_string(i) + " delegate " + std::to_string(d) + " has no 'task_template'");
                    }
                }
            }
            // Check for duplicate gates
            if (phase.contains("gate")) {
                auto gate = phase["gate"].get<std::string>();
                if (gate_names.count(gate)) {
                    result.warnings.push_back("duplicate gate: " + gate);
                }
                gate_names.insert(gate);
            }
        }
    }

    return result;
}

} // namespace euxis::cli
