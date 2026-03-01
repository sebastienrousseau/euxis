#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::cli {

/// Agent record from registry.
struct AgentInfo {
    std::string id;
    std::string role;
    std::string version;
    std::string tier;
    std::vector<std::string> tags;
    std::vector<std::string> capabilities;
    std::string prompt_path;
};

/// Squad record from squads.json.
struct SquadInfo {
    std::string id;
    std::string name;
    std::string purpose;
    std::string lead;
    std::vector<std::string> members;
};

/// SQLite + JSON agent/squad registry client.
class RegistryClient {
public:
    explicit RegistryClient(const std::string& data_dir);
    ~RegistryClient();

    // Prevent copy (owns SQLite handle)
    RegistryClient(const RegistryClient&) = delete;
    auto operator=(const RegistryClient&) -> RegistryClient& = delete;
    RegistryClient(RegistryClient&&) noexcept;
    auto operator=(RegistryClient&&) noexcept -> RegistryClient&;

    // --- Agent queries ---
    [[nodiscard]] auto list_agents() const -> std::vector<AgentInfo>;
    [[nodiscard]] auto get_agent(const std::string& agent_id) const -> std::optional<AgentInfo>;
    [[nodiscard]] auto find_by_tag(const std::string& tag) const -> std::vector<AgentInfo>;
    [[nodiscard]] auto find_by_tier(const std::string& tier) const -> std::vector<AgentInfo>;
    [[nodiscard]] auto find_by_capability(const std::string& cap) const -> std::vector<AgentInfo>;
    [[nodiscard]] auto agent_count() const -> int;

    // --- Squad queries ---
    [[nodiscard]] auto list_squads() const -> std::vector<SquadInfo>;
    [[nodiscard]] auto get_squad(const std::string& squad_id) const -> std::optional<SquadInfo>;

    // --- Registry health ---
    [[nodiscard]] auto has_sqlite() const -> bool;
    [[nodiscard]] auto has_json() const -> bool;

    // --- Plugin management ---
    auto register_plugin(const std::string& agent_id, const nlohmann::json& manifest) -> bool;
    auto unregister_plugin(const std::string& agent_id) -> bool;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // JSON fallback
    [[nodiscard]] auto list_agents_json() const -> std::vector<AgentInfo>;
    [[nodiscard]] auto list_squads_json() const -> std::vector<SquadInfo>;
    static auto parse_agent(const nlohmann::json& j) -> AgentInfo;
};

} // namespace euxis::cli
