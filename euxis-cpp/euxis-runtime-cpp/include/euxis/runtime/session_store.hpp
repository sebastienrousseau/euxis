#pragma once

#include <expected>
#include <string>
#include <vector>

namespace euxis::runtime {

enum class Role { User, Assistant, System };

struct ConversationMessage {
    Role role;
    std::string content;
    std::string agent_id;
    std::string model;
    std::string timestamp;
    double duration_ms{0.0};
};

struct SessionSnapshot {
    std::string session_id;
    std::string branch_id;
    std::string agent_id;
    std::vector<ConversationMessage> messages;
};

/// Abstract session persistence interface.
class ISessionStore {
public:
    virtual ~ISessionStore() = default;

    virtual auto save(const SessionSnapshot& snapshot)
        -> std::expected<void, std::string> = 0;

    virtual auto load(const std::string& session_id,
                      const std::string& branch = "main")
        -> std::expected<SessionSnapshot, std::string> = 0;

    virtual auto list_branches(const std::string& session_id)
        -> std::vector<std::string> = 0;

    virtual auto compact(const std::string& session_id, size_t keep_last_n = 20)
        -> std::expected<void, std::string> = 0;
};

} // namespace euxis::runtime
