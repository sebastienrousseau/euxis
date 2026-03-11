#include "euxis/a2a/message.hpp"

namespace euxis::a2a {

// ---------------------------------------------------------------------------
// A2AMessage::to_json
// ---------------------------------------------------------------------------
nlohmann::json A2AMessage::to_json() const {
    nlohmann::json j;
    j["role"] = role;
    j["createdAt"] = created_at;

    auto parts_arr = nlohmann::json::array();
    for (const auto& part : parts) {
        nlohmann::json p;
        p["type"] = part.type;
        p["content"] = part.content;
        if (part.mime_type.has_value()) {
            p["mimeType"] = *part.mime_type;
        }
        parts_arr.push_back(std::move(p));
    }
    j["parts"] = std::move(parts_arr);

    return j;
}

// ---------------------------------------------------------------------------
// A2AMessage::from_json
// ---------------------------------------------------------------------------
auto A2AMessage::from_json(const nlohmann::json& j) -> A2AMessage {
    A2AMessage msg;
    msg.role = j.at("role").get<std::string>();
    msg.created_at = j.at("createdAt").get<std::string>();

    if (j.contains("parts")) {
        for (const auto& p : j.at("parts")) {
            ContentPart part;
            part.type = p.at("type").get<std::string>();
            part.content = p.at("content").get<std::string>();
            if (p.contains("mimeType")) {
                part.mime_type = p.at("mimeType").get<std::string>();
            }
            msg.parts.push_back(std::move(part));
        }
    }

    return msg;
}

// ---------------------------------------------------------------------------
// Artifact::to_json
// ---------------------------------------------------------------------------
nlohmann::json Artifact::to_json() const {
    return {
        {"name", name},
        {"mimeType", mime_type},
        {"data", data}
    };
}

// ---------------------------------------------------------------------------
// Artifact::from_json
// ---------------------------------------------------------------------------
auto Artifact::from_json(const nlohmann::json& j) -> Artifact {
    Artifact art;
    art.name = j.at("name").get<std::string>();
    art.mime_type = j.at("mimeType").get<std::string>();
    art.data = j.at("data").get<std::string>();
    return art;
}

} // namespace euxis::a2a
