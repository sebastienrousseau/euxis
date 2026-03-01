#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::bridge {

struct BridgedSkill {
    std::string name;
    std::string slug;
    std::filesystem::path source_dir;
    std::string description;
    std::string runtime;  // "node", "python"
    std::filesystem::path entrypoint;
    std::vector<std::string> tags;
    nlohmann::json metadata;
    std::optional<std::filesystem::path> signature_path;
    std::optional<nlohmann::json> output_schema;

    [[nodiscard]] nlohmann::json to_json() const;
    [[nodiscard]] static auto from_json(const nlohmann::json& j) -> BridgedSkill;
};

}  // namespace euxis::bridge
