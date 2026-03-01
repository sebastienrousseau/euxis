#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "skill.hpp"

namespace euxis::bridge {

class ClawHubImporter {
public:
    explicit ClawHubImporter(std::filesystem::path skills_dir);

    /// Import a single skill from a directory containing SKILL.md + openclaw.json
    [[nodiscard]] auto import_skill(const std::filesystem::path& skill_dir)
        -> std::expected<BridgedSkill, std::string>;

    /// Discover and import all skills from the skills directory
    [[nodiscard]] auto import_all() -> std::vector<BridgedSkill>;

    /// Get import errors from the last import_all() call
    [[nodiscard]] auto errors() const -> const std::vector<std::string>&;

private:
    std::filesystem::path skills_dir_;
    std::vector<std::string> errors_;

    auto parse_openclaw_json(const std::filesystem::path& path)
        -> std::expected<nlohmann::json, std::string>;
    auto merge_metadata(const BridgedSkill& skill, const nlohmann::json& openclaw)
        -> BridgedSkill;
};

}  // namespace euxis::bridge
