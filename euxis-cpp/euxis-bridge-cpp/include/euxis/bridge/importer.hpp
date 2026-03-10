/// @file
/// @brief Importer for OpenClaw-compatible skills from ClawHub.
#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "skill.hpp"

namespace euxis::bridge {

/// @brief Discovers and imports skills from a local OpenClaw repository.
class ClawHubImporter {
public:
    /// @brief Construct importer targeting a specific skills directory.
    explicit ClawHubImporter(std::filesystem::path skills_dir);

    /// @brief Import a single skill from its directory.
    auto import_skill(const std::filesystem::path& skill_dir) -> std::expected<BridgedSkill, std::string>;
    
    /// @brief Batch import all skills found in the base directory.
    auto import_all() -> std::vector<BridgedSkill>;

    /// @brief Get list of non-fatal errors encountered during import.
    auto errors() const -> const std::vector<std::string>&;

private:
    std::filesystem::path skills_dir_;
    std::vector<std::string> errors_;

    auto parse_openclaw_json(const std::filesystem::path& path) -> std::expected<nlohmann::json, std::string>;
    auto merge_metadata(const BridgedSkill& skill, const nlohmann::json& openclaw) -> BridgedSkill;
};

} // namespace euxis::bridge
