#include <euxis/bridge/importer.hpp>
#include <euxis/bridge/parser.hpp>

#include <fstream>
#include <sstream>

namespace euxis::bridge {

ClawHubImporter::ClawHubImporter(std::filesystem::path skills_dir)
    : skills_dir_(std::move(skills_dir)) {}

auto ClawHubImporter::import_skill(const std::filesystem::path& skill_dir)
    -> std::expected<BridgedSkill, std::string> {
    auto skill_md_path = skill_dir / "SKILL.md";
    auto openclaw_path = skill_dir / "openclaw.json";

    // Parse SKILL.md frontmatter
    auto frontmatter = parse_skill_file(skill_md_path);
    if (!frontmatter) {
        return std::unexpected("Failed to parse SKILL.md in " + skill_dir.string());
    }

    BridgedSkill skill;
    skill.source_dir = skill_dir;
    skill.name = frontmatter->get("name").value_or(skill_dir.filename().string());
    skill.slug = frontmatter->get("slug").value_or(skill.name);
    skill.description = frontmatter->get("description").value_or("");
    skill.runtime = frontmatter->get("runtime").value_or("node");

    auto entrypoint = frontmatter->get("entrypoint").value_or("index.js");
    auto resolved_entry = (skill_dir / entrypoint).lexically_normal();
    if (resolved_entry.string().find("..") != std::string::npos ||
        !resolved_entry.string().starts_with(skill_dir.lexically_normal().string())) {
        return std::unexpected("Entrypoint path escapes skill directory: " + entrypoint);
    }
    skill.entrypoint = resolved_entry;

    // Parse tags (comma-separated)
    auto tags_str = frontmatter->get("tags").value_or("");
    if (!tags_str.empty()) {
        std::istringstream tag_stream(tags_str);
        std::string tag;
        while (std::getline(tag_stream, tag, ',')) {
            auto start = tag.find_first_not_of(" \t");
            auto end = tag.find_last_not_of(" \t");
            if (start != std::string::npos) {
                skill.tags.push_back(tag.substr(start, end - start + 1));
            }
        }
    }

    // Check for signature
    auto sig_path = skill_dir / (entrypoint + ".sig");
    if (std::filesystem::exists(sig_path)) {
        skill.signature_path = sig_path;
    }

    // Merge openclaw.json if present
    if (std::filesystem::exists(openclaw_path)) {
        auto openclaw = parse_openclaw_json(openclaw_path);
        if (openclaw) {
            skill = merge_metadata(skill, *openclaw);
        }
    }

    skill.metadata["source"] = "clawhub";
    skill.metadata["imported_from"] = skill_dir.string();

    return skill;
}

auto ClawHubImporter::import_all() -> std::vector<BridgedSkill> {
    errors_.clear();
    std::vector<BridgedSkill> skills;

    if (!std::filesystem::exists(skills_dir_)) {
        errors_.push_back("Skills directory does not exist: " + skills_dir_.string());
        return skills;
    }

    for (const auto& entry : std::filesystem::directory_iterator(skills_dir_)) {
        if (!entry.is_directory()) continue;

        auto skill_md = entry.path() / "SKILL.md";
        if (!std::filesystem::exists(skill_md)) continue;

        auto result = import_skill(entry.path());
        if (result) {
            skills.push_back(std::move(*result));
        } else {
            errors_.push_back(result.error());
        }
    }

    return skills;
}

auto ClawHubImporter::errors() const -> const std::vector<std::string>& {
    return errors_;
}

auto ClawHubImporter::parse_openclaw_json(const std::filesystem::path& path)
    -> std::expected<nlohmann::json, std::string> {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::unexpected("Cannot open " + path.string());
    }
    try {
        return nlohmann::json::parse(file);
    } catch (const nlohmann::json::parse_error& e) {
        return std::unexpected(std::string("JSON parse error: ") + e.what());
    }
}

auto ClawHubImporter::merge_metadata(const BridgedSkill& skill,
                                     const nlohmann::json& openclaw) -> BridgedSkill {
    BridgedSkill merged = skill;
    if (openclaw.contains("description") && merged.description.empty()) {
        merged.description = openclaw["description"].get<std::string>();
    }
    if (openclaw.contains("output_schema")) {
        merged.output_schema = openclaw["output_schema"];
    }
    if (openclaw.contains("metadata")) {
        for (auto& [key, val] : openclaw["metadata"].items()) {
            merged.metadata[key] = val;
        }
    }
    return merged;
}

}  // namespace euxis::bridge
