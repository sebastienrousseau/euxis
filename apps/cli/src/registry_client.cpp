#include "euxis/cli/registry_client.hpp"

#include <filesystem>
#include <fstream>

#include <spdlog/spdlog.h>
#include <sqlite3.h>

namespace euxis::cli {

// --- Impl (PIMPL for SQLite handle) ---

struct RegistryClient::Impl {
    std::string data_dir;
    sqlite3* db{nullptr};
    nlohmann::json registry_json;
    nlohmann::json squads_json;

    ~Impl() {
        if (db) sqlite3_close(db);
    }
};

// --- Construction ---

RegistryClient::RegistryClient(const std::string& data_dir)
    : impl_(std::make_unique<Impl>()) {
    impl_->data_dir = data_dir;

    // Try SQLite first
    auto db_path = std::filesystem::path(data_dir) / "agents" / "registry.db";
    if (std::filesystem::exists(db_path)) {
        int rc = sqlite3_open_v2(db_path.c_str(), &impl_->db,
                                 SQLITE_OPEN_READONLY, nullptr);
        if (rc != SQLITE_OK) {
            spdlog::warn("SQLite open failed: {}", sqlite3_errmsg(impl_->db));
            sqlite3_close(impl_->db);
            impl_->db = nullptr;
        }
    }

    // Load JSON fallback
    auto json_path = std::filesystem::path(data_dir) / "agents" / "registry.json";
    if (std::filesystem::exists(json_path)) {
        try {
            std::ifstream f(json_path);
            impl_->registry_json = nlohmann::json::parse(f);
        } catch (const std::exception& e) {
            spdlog::warn("registry.json parse error: {}", e.what());
        }
    }

    // Load squads
    auto squads_path = std::filesystem::path(data_dir) / "../euxis-core/agents/squads.json";
    auto alt_squads = std::filesystem::path(data_dir) / "agents" / "squads.json";
    for (const auto& p : {squads_path, alt_squads}) {
        if (std::filesystem::exists(p)) {
            try {
                std::ifstream f(p);
                impl_->squads_json = nlohmann::json::parse(f);
                break;
            } catch (...) {}
        }
    }
}

RegistryClient::~RegistryClient() = default;
RegistryClient::RegistryClient(RegistryClient&&) noexcept = default;
auto RegistryClient::operator=(RegistryClient&&) noexcept -> RegistryClient& = default;

// --- Agent queries ---

auto RegistryClient::parse_agent(const nlohmann::json& j) -> AgentInfo {
    AgentInfo a;
    a.id = j.value("agent_id", j.value("id", ""));
    a.role = j.value("role", "");
    a.version = j.value("version", "");
    a.tier = j.value("tier", "code");
    if (j.contains("tags") && j["tags"].is_array()) {
        for (const auto& t : j["tags"]) a.tags.push_back(t.get<std::string>());
    }
    if (j.contains("capabilities") && j["capabilities"].is_array()) {
        for (const auto& c : j["capabilities"]) a.capabilities.push_back(c.get<std::string>());
    }
    a.prompt_path = j.value("prompt_path", j.value("path", ""));
    return a;
}

void RegistryClient::resolve_prompt_path(AgentInfo& a) const {
    if (a.prompt_path.empty()) return;
    if (a.prompt_path[0] == '/') return;

    std::vector<std::string> prefixes = {"", "data/agents/", "agents/"};
    for (const auto& prefix : prefixes) {
        auto p = std::filesystem::path(impl_->data_dir) / prefix / a.prompt_path;
        if (std::filesystem::exists(p)) {
            a.prompt_path = (std::filesystem::path(prefix) / a.prompt_path).string();
            return;
        }
    }
}

auto RegistryClient::list_agents() const -> std::vector<AgentInfo> {
    std::vector<AgentInfo> agents;
    if (impl_->db) {
        const char* sql = "SELECT id, role, version, tier, path FROM agents ORDER BY id";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                AgentInfo a;
                a.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                auto* role_col = sqlite3_column_text(stmt, 1);
                if (role_col) a.role = reinterpret_cast<const char*>(role_col);
                auto* ver_col = sqlite3_column_text(stmt, 2);
                if (ver_col) a.version = reinterpret_cast<const char*>(ver_col);
                auto* tier_col = sqlite3_column_text(stmt, 3);
                if (tier_col) a.tier = reinterpret_cast<const char*>(tier_col);
                auto* path_col = sqlite3_column_text(stmt, 4);
                if (path_col) a.prompt_path = reinterpret_cast<const char*>(path_col);
                
                resolve_prompt_path(a);
                agents.push_back(std::move(a));
            }
            sqlite3_finalize(stmt);
            return agents;
        }
        if (stmt) sqlite3_finalize(stmt);
    }
    
    agents = list_agents_json();
    for (auto& a : agents) resolve_prompt_path(a);
    return agents;
}

auto RegistryClient::get_agent(const std::string& agent_id) const -> std::optional<AgentInfo> {
    if (impl_->db) {
        const char* sql = "SELECT id, role, version, tier, path FROM agents WHERE id = ?";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, agent_id.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                AgentInfo a;
                auto* id_col = sqlite3_column_text(stmt, 0);
                if (id_col) a.id = reinterpret_cast<const char*>(id_col);
                auto* role_col = sqlite3_column_text(stmt, 1);
                if (role_col) a.role = reinterpret_cast<const char*>(role_col);
                auto* ver_col = sqlite3_column_text(stmt, 2);
                if (ver_col) a.version = reinterpret_cast<const char*>(ver_col);
                auto* tier_col = sqlite3_column_text(stmt, 3);
                if (tier_col) a.tier = reinterpret_cast<const char*>(tier_col);
                auto* path_col = sqlite3_column_text(stmt, 4);
                if (path_col) a.prompt_path = reinterpret_cast<const char*>(path_col);

                resolve_prompt_path(a);
                sqlite3_finalize(stmt);
                return a;
            }
            sqlite3_finalize(stmt);
        } else {
            if (stmt) sqlite3_finalize(stmt);
        }
    }
    // JSON fallback
    if (impl_->registry_json.contains("agents") && impl_->registry_json["agents"].is_array()) {
        for (const auto& j : impl_->registry_json["agents"]) {
            if (j.value("agent_id", j.value("id", "")) == agent_id) {
                auto a = parse_agent(j);
                resolve_prompt_path(a);
                return a;
            }
        }
    }
    return std::nullopt;
}

auto RegistryClient::find_by_tag(const std::string& tag) const -> std::vector<AgentInfo> {
    if (impl_->db) {
        std::vector<AgentInfo> agents;
        const char* sql =
            "SELECT a.id, a.role, a.version, a.tier, a.path "
            "FROM agents a JOIN agent_tags at ON a.id = at.agent_id "
            "JOIN tags t ON at.tag_id = t.tag_id WHERE t.name = ?";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, tag.c_str(), -1, SQLITE_TRANSIENT);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                AgentInfo a;
                auto* id_col = sqlite3_column_text(stmt, 0);
                if (id_col) a.id = reinterpret_cast<const char*>(id_col);
                auto* role_col = sqlite3_column_text(stmt, 1);
                if (role_col) a.role = reinterpret_cast<const char*>(role_col);
                auto* ver_col = sqlite3_column_text(stmt, 2);
                if (ver_col) a.version = reinterpret_cast<const char*>(ver_col);
                auto* tier_col = sqlite3_column_text(stmt, 3);
                if (tier_col) a.tier = reinterpret_cast<const char*>(tier_col);
                auto* path_col = sqlite3_column_text(stmt, 4);
                if (path_col) a.prompt_path = reinterpret_cast<const char*>(path_col);
                agents.push_back(std::move(a));
            }
            sqlite3_finalize(stmt);
            return agents;
        }
        if (stmt) sqlite3_finalize(stmt);
    }
    // JSON fallback: filter agents by tag
    std::vector<AgentInfo> result;
    for (const auto& a : list_agents_json()) {
        for (const auto& t : a.tags) {
            if (t == tag) { result.push_back(a); break; }
        }
    }
    return result;
}

auto RegistryClient::find_by_tier(const std::string& tier) const -> std::vector<AgentInfo> {
    std::vector<AgentInfo> result;
    for (const auto& a : list_agents()) {
        if (a.tier == tier) result.push_back(a);
    }
    return result;
}

auto RegistryClient::find_by_capability(const std::string& cap) const -> std::vector<AgentInfo> {
    std::vector<AgentInfo> result;
    for (const auto& a : list_agents()) {
        for (const auto& c : a.capabilities) {
            if (c == cap) { result.push_back(a); break; }
        }
    }
    return result;
}

auto RegistryClient::agent_count() const -> int {
    if (impl_->db) {
        const char* sql = "SELECT COUNT(*) FROM agents";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                int count = sqlite3_column_int(stmt, 0);
                sqlite3_finalize(stmt);
                return count;
            }
            sqlite3_finalize(stmt);
        } else {
            if (stmt) sqlite3_finalize(stmt);
        }
    }
    return static_cast<int>(list_agents_json().size());
}

// --- Squad queries ---

auto RegistryClient::list_squads() const -> std::vector<SquadInfo> {
    return list_squads_json();
}

auto RegistryClient::get_squad(const std::string& squad_id) const -> std::optional<SquadInfo> {
    for (const auto& s : list_squads()) {
        if (s.id == squad_id) return s;
    }
    return std::nullopt;
}

// --- Playbook queries ---

auto RegistryClient::list_playbooks() const -> std::vector<PlaybookInfo> {
    std::vector<PlaybookInfo> result;
    auto pb_dir = std::filesystem::path(impl_->data_dir) / "config" / "playbooks";
    if (!std::filesystem::exists(pb_dir)) {
        // Try fallback location
        pb_dir = std::filesystem::path(impl_->data_dir) / "data" / "config" / "playbooks";
    }

    if (std::filesystem::exists(pb_dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(pb_dir)) {
            if (entry.path().extension() == ".json") {
                try {
                    std::ifstream f(entry.path());
                    auto j = nlohmann::json::parse(f);
                    PlaybookInfo pb;
                    pb.id = j.value("id", entry.path().stem().string());
                    pb.name = j.value("name", pb.id);
                    pb.description = j.value("description", "");
                    pb.file_path = entry.path().string();
                    result.push_back(std::move(pb));
                } catch (...) {}
            }
        }
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.id < b.id;
    });
    return result;
}

auto RegistryClient::get_playbook(const std::string& playbook_id) const -> std::optional<PlaybookInfo> {
    for (const auto& pb : list_playbooks()) {
        if (pb.id == playbook_id) return pb;
    }
    return std::nullopt;
}

// --- Registry health ---

auto RegistryClient::has_sqlite() const -> bool { return impl_->db != nullptr; }
auto RegistryClient::has_json() const -> bool { return !impl_->registry_json.is_null(); }

// --- Plugin management ---

auto RegistryClient::register_plugin(const std::string& agent_id,
                                      const nlohmann::json& manifest) -> bool {
    // Validate agent_id (alphanumeric + dash/underscore only)
    for (char c : agent_id) {
        if (!std::isalnum(c) && c != '-' && c != '_') {
            spdlog::error("Invalid agent_id: {}", agent_id);
            return false;
        }
    }
    auto plugin_dir = std::filesystem::path(impl_->data_dir) / "config" / "plugins";
    std::filesystem::create_directories(plugin_dir);
    auto meta_path = plugin_dir / (agent_id + ".json");
    std::ofstream f(meta_path);
    f << manifest.dump(2);
    return f.good();
}

auto RegistryClient::unregister_plugin(const std::string& agent_id) -> bool {
    auto meta_path = std::filesystem::path(impl_->data_dir) / "config" / "plugins" /
                     (agent_id + ".json");
    if (std::filesystem::exists(meta_path)) {
        return std::filesystem::remove(meta_path);
    }
    return false;
}

// --- JSON fallback ---

auto RegistryClient::list_agents_json() const -> std::vector<AgentInfo> {
    std::vector<AgentInfo> agents;
    if (impl_->registry_json.contains("agents") && impl_->registry_json["agents"].is_array()) {
        for (const auto& j : impl_->registry_json["agents"]) {
            agents.push_back(parse_agent(j));
        }
    }
    return agents;
}

auto RegistryClient::list_squads_json() const -> std::vector<SquadInfo> {
    std::vector<SquadInfo> squads;
    const auto& j = impl_->squads_json;
    if (j.contains("squads") && j["squads"].is_array()) {
        for (const auto& s : j["squads"]) {
            SquadInfo sq;
            sq.id = s.value("id", "");
            sq.name = s.value("name", "");
            sq.purpose = s.value("purpose", "");
            sq.lead = s.value("lead", "");
            if (s.contains("members") && s["members"].is_array()) {
                for (const auto& m : s["members"]) {
                    sq.members.push_back(m.get<std::string>());
                }
            }
            squads.push_back(std::move(sq));
        }
    }
    return squads;
}

} // namespace euxis::cli
