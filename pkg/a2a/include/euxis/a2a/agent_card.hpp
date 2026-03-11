#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

/** @namespace euxis::a2a
 * @brief Agent-to-Agent (A2A) protocol implementation.
 */
namespace euxis::a2a {

/**
 * @brief Authentication schema for agent discovery.
 */
struct AuthSchema {
    std::string type;           ///< Authentication type (e.g. "bearer", "api_key").
    std::string description;    ///< Human-readable auth instructions.
};

/**
 * @brief A single capability advertised by an agent.
 */
struct Capability {
    std::string name;           ///< Name of the capability/function.
    std::string description;    ///< What the capability does.
    std::optional<nlohmann::json> input_schema;  ///< JSON Schema for input.
    std::optional<nlohmann::json> output_schema; ///< JSON Schema for output.
};

/**
 * @brief An A2A v0.2 Agent Card describing an agent's identity and API.
 * 
 * Agent cards are the primary discovery mechanism in EUXIS, containing 
 * the endpoint URL, versioning, and functional capabilities.
 */
struct AgentCard {
    std::string name;           ///< Friendly name of the agent.
    std::string description;    ///< Detailed agent description.
    std::string url;            ///< API endpoint (HTTP or WS).
    std::string version;        ///< Semantic version string.
    std::vector<Capability> capabilities; ///< List of supported agent functions.
    std::optional<AuthSchema> auth;       ///< Authentication requirements.
    std::optional<std::string> identity_did; ///< Decentralized Identifier.
    nlohmann::json metadata;    ///< Additional custom attributes.

    /** @brief Serialise to standard JSON format. */
    [[nodiscard]] nlohmann::json to_json() const;

    /** @brief Deserialise from standard JSON. */
    [[nodiscard]] static auto from_json(const nlohmann::json& j) -> AgentCard;

    /** @brief Serialise to high-performance binary format. */
    [[nodiscard]] auto to_msgpack() const -> std::vector<uint8_t>;

    /** @brief Deserialise from high-performance binary format. */
    [[nodiscard]] static auto from_msgpack(const std::vector<uint8_t>& data) -> AgentCard;
};

/**
 * @brief Validate an agent card against protocol rules.
 * @return std::expected<void, std::string> Empty on success, or error message.
 */
[[nodiscard]] auto validate_card(const AgentCard& card) -> std::expected<void, std::string>;

} // namespace euxis::a2a
