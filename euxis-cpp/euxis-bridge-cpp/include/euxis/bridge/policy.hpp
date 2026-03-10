#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

/** @namespace euxis::bridge
 * @brief Agent skill bridging and sandboxed execution policies.
 */
namespace euxis::bridge {

/** @brief CPU/RAM constraints for bridged skill execution. */
struct ResourceLimits {
    uint32_t memory_mb = 256;
    uint32_t timeout_seconds = 20;
};

/** @brief Access controls for local filesystem operations. */
struct FilesystemPolicy {
    bool read_only = true;
    std::vector<std::filesystem::path> write_paths;
};

/** @brief Connectivity controls for bridged skills. */
struct NetworkPolicy {
    bool deny_all = true;
    std::vector<std::string> allowed_hosts;
};

/**
 * @brief Holistic security and resource policy for a bridged skill.
 * 
 * Defines the sandbox parameters and verification requirements for 
 * out-of-process agent capabilities.
 */
struct SkillExecutionPolicy {
    bool require_signature = false;
    std::optional<std::filesystem::path> public_key_path;
    FilesystemPolicy filesystem;
    NetworkPolicy network;
    ResourceLimits resources;
    std::optional<nlohmann::json> output_schema;
    std::optional<std::filesystem::path> audit_log_path;

    /** @brief Returns a policy with minimal restrictions. */
    [[nodiscard]] static auto permissive() -> SkillExecutionPolicy;
    
    /** @brief Serialise policy to JSON. */
    [[nodiscard]] nlohmann::json to_json() const;
};

} // namespace euxis::bridge
