/// @file
/// @brief A2A message and artifact definitions.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::a2a {

/// @brief A single part of a message (text, image, etc).
struct ContentPart {
    std::string type;
    std::string content;
    std::optional<std::string> mime_type;
};

/// @brief Agent-to-Agent message structure.
struct A2AMessage {
    std::string role;
    std::string created_at;
    std::vector<ContentPart> parts;

    [[nodiscard]] nlohmann::json to_json() const;
    [[nodiscard]] static auto from_json(const nlohmann::json& j) -> A2AMessage;
};

/// @brief Binary or text artifact exchanged between agents.
struct Artifact {
    std::string name;
    std::string mime_type;
    std::string data; // Base64

    [[nodiscard]] nlohmann::json to_json() const;
    [[nodiscard]] static auto from_json(const nlohmann::json& j) -> Artifact;
};

} // namespace euxis::a2a
