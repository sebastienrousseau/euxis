#include "euxis/runtime/session_store.hpp"

#include <filesystem>
#include <fstream>
#include <map>
#include <string_view>

#include <nlohmann/json.hpp>

#ifndef _WIN32
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

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

auto role_from_string(std::string_view s) -> Role {
    if (s == "assistant") return Role::Assistant;
    if (s == "system") return Role::System;
    return Role::User;
}

auto snapshot_to_json(const SessionSnapshot& snapshot) -> nlohmann::json {
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
    return j;
}

auto snapshot_from_json(const nlohmann::json& j, std::string_view session_id, std::string_view branch) -> SessionSnapshot {
    SessionSnapshot snap;
    snap.session_id = j.value("session_id", std::string(session_id));
    snap.branch_id = j.value("branch_id", std::string(branch));
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

} // namespace

class MemorySessionStore : public ISessionStore {
public:
    auto save(const SessionSnapshot& snapshot) -> std::expected<void, std::string> override {
        sessions_[snapshot.session_id][snapshot.branch_id] = snapshot;
        return {};
    }

    auto load(const std::string& session_id, const std::string& branch) 
        -> std::expected<SessionSnapshot, std::string> override {
        if (!sessions_.contains(session_id) || !sessions_.at(session_id).contains(branch)) {
            return std::unexpected("Session not found in memory");
        }
        return sessions_.at(session_id).at(branch);
    }

    auto list_branches(const std::string& session_id) -> std::vector<std::string> override {
        std::vector<std::string> res;
        if (sessions_.contains(session_id)) {
            for (const auto& [name, _] : sessions_.at(session_id)) res.push_back(name);
        }
        return res;
    }

    auto compact(const std::string& session_id, size_t keep_last_n) -> std::expected<void, std::string> override {
        return load(session_id, "main")
            .and_then([&](SessionSnapshot&& snap) -> std::expected<void, std::string> {
                if (snap.messages.size() > keep_last_n) {
                    snap.messages.erase(snap.messages.begin(), snap.messages.begin() + (snap.messages.size() - keep_last_n));
                }
                return save(snap);
            });
    }

private:
    std::map<std::string, std::map<std::string, SessionSnapshot>> sessions_;
};

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
        auto path = dir / (snapshot.branch_id + ".msgp");

        const auto packed = nlohmann::json::to_msgpack(snapshot_to_json(snapshot));
        std::ofstream f(path, std::ios::binary);
        if (!f.is_open()) return std::unexpected("Failed to open path for binary write");
        
        f.write(reinterpret_cast<const char*>(packed.data()), static_cast<std::streamsize>(packed.size()));
        return {};
    }

    auto load(const std::string& session_id, const std::string& branch)
        -> std::expected<SessionSnapshot, std::string> override {
        auto path = base_dir_ / session_id / (branch + ".msgp");
        if (!std::filesystem::exists(path)) [[unlikely]] return std::unexpected("Session not found");

#ifndef _WIN32
        int fd = open(path.c_str(), O_RDONLY);
        if (fd != -1) {
            struct stat sb;
            if (fstat(fd, &sb) == 0 && sb.st_size > 0) {
                void* mapped = mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
                if (mapped != MAP_FAILED) {
                    try {
                        std::span<const uint8_t> data(static_cast<const uint8_t*>(mapped), sb.st_size);
                        auto snap = snapshot_from_json(nlohmann::json::from_msgpack(data), session_id, branch);
                        munmap(mapped, sb.st_size);
                        close(fd);
                        return snap;
                    } catch (...) {
                        munmap(mapped, sb.st_size);
                        close(fd);
                        return std::unexpected("MessagePack decoding failed");
                    }
                }
            }
            close(fd);
        }
#endif
        // Fallback for Windows or mmap failure
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) return std::unexpected("Failed to open binary stream");

        std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        try {
            return snapshot_from_json(nlohmann::json::from_msgpack(buffer), session_id, branch);
        } catch (...) {
            return std::unexpected("MessagePack decoding failed");
        }
    }

    auto list_branches(const std::string& session_id) -> std::vector<std::string> override {
        std::vector<std::string> branches;
        auto dir = base_dir_ / session_id;
        if (!std::filesystem::exists(dir)) return branches;
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() == ".msgp") branches.push_back(entry.path().stem().string());
        }
        return branches;
    }

    auto compact(const std::string& session_id, size_t keep_last_n) -> std::expected<void, std::string> override {
        return load(session_id, "main")
            .and_then([&](SessionSnapshot&& snap) -> std::expected<void, std::string> {
                if (snap.messages.size() > keep_last_n) {
                    snap.messages.erase(snap.messages.begin(), snap.messages.begin() + (snap.messages.size() - keep_last_n));
                }
                return save(snap);
            });
    }

private:
    std::filesystem::path base_dir_;
};

auto make_session_store(const std::string& base_dir) -> std::unique_ptr<ISessionStore> {
    return std::make_unique<FileSessionStore>(base_dir);
}

auto make_memory_session_store() -> std::unique_ptr<ISessionStore> {
    return std::make_unique<MemorySessionStore>();
}

} // namespace euxis::runtime
