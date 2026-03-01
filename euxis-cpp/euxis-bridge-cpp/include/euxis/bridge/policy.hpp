#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::bridge {

struct ResourceLimits {
    uint32_t memory_mb = 256;
    uint32_t timeout_seconds = 20;
};

struct FilesystemPolicy {
    bool read_only = true;
    std::vector<std::filesystem::path> write_paths;
};

struct NetworkPolicy {
    bool deny_all = true;
    std::vector<std::string> allowed_hosts;
};

struct SkillExecutionPolicy {
    bool require_signature = false;
    std::optional<std::filesystem::path> public_key_path;
    FilesystemPolicy filesystem;
    NetworkPolicy network;
    ResourceLimits resources;
    std::optional<nlohmann::json> output_schema;
    std::optional<std::filesystem::path> audit_log_path;

    [[nodiscard]] static auto permissive() -> SkillExecutionPolicy;
    [[nodiscard]] nlohmann::json to_json() const;
};

}  // namespace euxis::bridge
