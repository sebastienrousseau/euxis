#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::a2a {

/// A single content part within a message.
struct ContentPart {
    std::string type;  // "text", "data", "file"
    std::string content;
    std::optional<std::string> mime_type;
};

/// An A2A message exchanged between user and agent.
struct A2AMessage {
    std::string role;  // "user", "agent"
    std::vector<ContentPart> parts;
    std::string created_at;

    /// Serialise to JSON.
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialise from JSON.
    [[nodiscard]] static auto from_json(const nlohmann::json& j) -> A2AMessage;
};

/// A binary artifact produced during task execution.
struct Artifact {
    std::string name;
    std::string mime_type;
    std::string data;  // base64-encoded

    /// Serialise to JSON.
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialise from JSON.
    [[nodiscard]] static auto from_json(const nlohmann::json& j) -> Artifact;
};

} // namespace euxis::a2a
