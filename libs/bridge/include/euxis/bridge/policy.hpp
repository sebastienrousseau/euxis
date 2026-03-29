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

/// Per-agent capability token for Zero-Trust execution.
struct AgentCapabilityToken {
    std::string agent_id;
    std::string session_id;
    std::vector<std::string> allowed_providers;
    std::vector<std::string> allowed_tools;
    int max_token_budget{4096};
    bool filesystem_read_only{true};
    bool network_deny_all{true};
    std::string issued_at;
    std::string expires_at;
    std::string hmac_signature;

    /// Deterministic canonical string for HMAC computation.
    [[nodiscard]] auto canonical_string() const -> std::string;

    /// Serialize to JSON.
    [[nodiscard]] auto to_json() const -> nlohmann::json;

    /// Deserialize from JSON.
    [[nodiscard]] static auto from_json(const nlohmann::json& j) -> AgentCapabilityToken;
};

/// Issue a capability token with HMAC-SHA256 signature.
[[nodiscard]] auto issue_capability_token(
    const std::string& agent_id,
    const std::string& session_id,
    const std::vector<std::string>& allowed_providers,
    const std::vector<std::string>& allowed_tools,
    int max_token_budget,
    const unsigned char* signing_key) -> AgentCapabilityToken;

/// Verify a capability token's HMAC signature.
[[nodiscard]] auto verify_capability_token(
    const AgentCapabilityToken& token,
    const unsigned char* signing_key) -> bool;

} // namespace euxis::bridge
