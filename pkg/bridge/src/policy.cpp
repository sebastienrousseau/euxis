#include <euxis/bridge/policy.hpp>

namespace euxis::bridge {

auto SkillExecutionPolicy::permissive() -> SkillExecutionPolicy {
    SkillExecutionPolicy policy;
    policy.require_signature = false;
    policy.filesystem.read_only = false;
    policy.network.deny_all = false;
    policy.resources.memory_mb = 512;
    policy.resources.timeout_seconds = 60;
    return policy;
}

auto SkillExecutionPolicy::to_json() const -> nlohmann::json {
    nlohmann::json j;
    j["require_signature"] = require_signature;
    if (public_key_path) {
        j["public_key_path"] = public_key_path->string();
    }
    j["filesystem"]["read_only"] = filesystem.read_only;
    nlohmann::json write_paths = nlohmann::json::array();
    for (const auto& p : filesystem.write_paths) {
        write_paths.push_back(p.string());
    }
    j["filesystem"]["write_paths"] = write_paths;
    j["network"]["deny_all"] = network.deny_all;
    j["network"]["allowed_hosts"] = network.allowed_hosts;
    j["resources"]["memory_mb"] = resources.memory_mb;
    j["resources"]["timeout_seconds"] = resources.timeout_seconds;
    if (output_schema) {
        j["output_schema"] = *output_schema;
    }
    if (audit_log_path) {
        j["audit_log_path"] = audit_log_path->string();
    }
    return j;
}

}  // namespace euxis::bridge
