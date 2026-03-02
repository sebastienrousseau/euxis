#include "euxis/runtime/session_store.hpp"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

namespace euxis::runtime {

namespace {

auto role_to_string(Role r) -> std::string {
    switch (r) {
        case Role::User: return "user";
        case Role::Assistant: return "assistant";
        case Role::System: return "system";
    }
    return "unknown";
}

auto role_from_string(const std::string& s) -> Role {
    if (s == "assistant") return Role::Assistant;
    if (s == "system") return Role::System;
    return Role::User;
}

} // namespace

class FileSessionStore : public ISessionStore {
public:
    explicit FileSessionStore(std::filesystem::path base_dir)
        : base_dir_(std::move(base_dir))
    {
        std::filesystem::create_directories(base_dir_);
    }

    auto save(const SessionSnapshot& snapshot)
        -> std::expected<void, std::string> override {
        auto dir = base_dir_ / snapshot.session_id;
        std::filesystem::create_directories(dir);
        auto path = dir / (snapshot.branch_id + ".json");

        nlohmann::json j;
        j["session_id"] = snapshot.session_id;
        j["branch_id"] = snapshot.branch_id;
        j["agent_id"] = snapshot.agent_id;
        j["messages"] = nlohmann::json::array();

        for (const auto& msg : snapshot.messages) {
            j["messages"].push_back({
                {"role", role_to_string(msg.role)},
                {"content", msg.content},
                {"agent_id", msg.agent_id},
                {"model", msg.model},
                {"timestamp", msg.timestamp},
                {"duration_ms", msg.duration_ms},
            });
        }

        std::ofstream f(path);
        if (!f.is_open())
            return std::unexpected("Failed to open: " + path.string());
        f << j.dump(2);
        return {};
    }

    auto load(const std::string& session_id, const std::string& branch)
        -> std::expected<SessionSnapshot, std::string> override {
        auto path = base_dir_ / session_id / (branch + ".json");
        if (!std::filesystem::exists(path))
            return std::unexpected("Session not found: " + path.string());

        std::ifstream f(path);
        if (!f.is_open())
            return std::unexpected("Failed to open: " + path.string());

        auto j = nlohmann::json::parse(f, nullptr, false);
        if (j.is_discarded())
            return std::unexpected("Failed to parse: " + path.string());

        SessionSnapshot snap;
        snap.session_id = j.value("session_id", session_id);
        snap.branch_id = j.value("branch_id", branch);
        snap.agent_id = j.value("agent_id", "");

        for (const auto& msg_j : j.value("messages", nlohmann::json::array())) {
            snap.messages.push_back({
                .role = role_from_string(msg_j.value("role", "user")),
                .content = msg_j.value("content", ""),
                .agent_id = msg_j.value("agent_id", ""),
                .model = msg_j.value("model", ""),
                .timestamp = msg_j.value("timestamp", ""),
                .duration_ms = msg_j.value("duration_ms", 0.0),
            });
        }
        return snap;
    }

    auto list_branches(const std::string& session_id)
        -> std::vector<std::string> override {
        std::vector<std::string> branches;
        auto dir = base_dir_ / session_id;
        if (!std::filesystem::exists(dir)) return branches;

        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() == ".json") {
                branches.push_back(entry.path().stem().string());
            }
        }
        return branches;
    }

    auto compact(const std::string& session_id, size_t keep_last_n)
        -> std::expected<void, std::string> override {
        auto result = load(session_id, "main");
        if (!result.has_value()) return std::unexpected(result.error());

        auto& snap = *result;
        if (snap.messages.size() > keep_last_n) {
            snap.messages.erase(
                snap.messages.begin(),
                snap.messages.begin() +
                    static_cast<ptrdiff_t>(snap.messages.size() - keep_last_n));
        }
        return save(snap);
    }

private:
    std::filesystem::path base_dir_;
};

auto make_session_store(const std::string& base_dir)
    -> std::unique_ptr<ISessionStore> {
    return std::make_unique<FileSessionStore>(base_dir);
}

} // namespace euxis::runtime
