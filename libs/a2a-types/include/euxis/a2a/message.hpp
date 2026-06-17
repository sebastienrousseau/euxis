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

/// @brief Request sent by orchestrator to start a resource auction.
struct BidRequest {
    std::string task_id;
    std::string capability_required;
    int max_latency_ms = 0;

    [[nodiscard]] nlohmann::json to_json() const;
    [[nodiscard]] static auto from_json(const nlohmann::json& j) -> BidRequest;
};

/// @brief Response from a worker agent bidding for a task.
struct BidResponse {
    std::string task_id;
    std::string agent_id;
    double estimated_cost = 0.0;
    int estimated_latency_ms = 0;
    bool accepted = false;

    [[nodiscard]] nlohmann::json to_json() const;
    [[nodiscard]] static auto from_json(const nlohmann::json& j) -> BidResponse;
};

} // namespace euxis::a2a
