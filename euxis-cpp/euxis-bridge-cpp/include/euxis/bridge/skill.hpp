/// @file
/// @brief Bridged skill definition for external tool integration.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::bridge {

/// @brief metadata and configuration for a skill imported from an external bridge.
struct BridgedSkill {
    std::string name;
    std::string slug;
    std::filesystem::path source_dir;
    std::string description;
    std::string runtime;
    std::filesystem::path entrypoint;
    std::vector<std::string> tags;
    nlohmann::json metadata;
    std::optional<std::filesystem::path> signature_path;
    std::optional<nlohmann::json> output_schema;

    [[nodiscard]] auto to_json() const -> nlohmann::json;
    [[nodiscard]] static auto from_json(const nlohmann::json& j) -> BridgedSkill;
};

} // namespace euxis::bridge
