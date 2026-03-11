/// @file
/// @brief Parser for skill files and frontmatter content.
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace euxis::bridge {

/// @brief Metadata and body content parsed from a skill file.
struct Frontmatter {
    std::map<std::string, std::string> fields;
    std::string body;

    [[nodiscard]] auto get(const std::string& key) const -> std::optional<std::string>;
};

/// @brief Parse raw string content for YAML-like frontmatter.
[[nodiscard]] auto parse_frontmatter(const std::string& content) -> Frontmatter;

/// @brief Load and parse a skill file from disk.
[[nodiscard]] auto parse_skill_file(const std::filesystem::path& path) -> std::optional<Frontmatter>;

} // namespace euxis::bridge
