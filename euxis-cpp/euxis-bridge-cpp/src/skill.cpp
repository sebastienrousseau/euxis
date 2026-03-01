#include <euxis/bridge/skill.hpp>

namespace euxis::bridge {

auto BridgedSkill::to_json() const -> nlohmann::json {
    nlohmann::json j;
    j["name"] = name;
    j["slug"] = slug;
    j["source_dir"] = source_dir.string();
    j["description"] = description;
    j["runtime"] = runtime;
    j["entrypoint"] = entrypoint.string();
    j["tags"] = tags;
    j["metadata"] = metadata;
    if (signature_path) {
        j["signature_path"] = signature_path->string();
    }
    if (output_schema) {
        j["output_schema"] = *output_schema;
    }
    return j;
}

auto BridgedSkill::from_json(const nlohmann::json& j) -> BridgedSkill {
    BridgedSkill skill;
    skill.name = j.value("name", "");
    skill.slug = j.value("slug", "");
    if (j.contains("source_dir")) {
        skill.source_dir = j["source_dir"].get<std::string>();
    }
    skill.description = j.value("description", "");
    skill.runtime = j.value("runtime", "node");
    if (j.contains("entrypoint")) {
        skill.entrypoint = j["entrypoint"].get<std::string>();
    }
    if (j.contains("tags")) {
        skill.tags = j["tags"].get<std::vector<std::string>>();
    }
    skill.metadata = j.value("metadata", nlohmann::json::object());
    if (j.contains("signature_path")) {
        skill.signature_path = std::filesystem::path{j["signature_path"].get<std::string>()};
    }
    if (j.contains("output_schema")) {
        skill.output_schema = j["output_schema"];
    }
    return skill;
}

}  // namespace euxis::bridge
