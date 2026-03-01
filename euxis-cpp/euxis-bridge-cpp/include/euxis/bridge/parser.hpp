#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace euxis::bridge {

struct Frontmatter {
    std::unordered_map<std::string, std::string> fields;
    std::string body;

    [[nodiscard]] auto get(const std::string& key) const -> std::optional<std::string>;
};

/// Parse SKILL.md frontmatter (YAML-like key: value between --- delimiters)
[[nodiscard]] auto parse_frontmatter(const std::string& content) -> Frontmatter;

/// Read and parse a SKILL.md file
[[nodiscard]] auto parse_skill_file(const std::filesystem::path& path)
    -> std::optional<Frontmatter>;

}  // namespace euxis::bridge
