/// @file
/// @brief Persistent storage for agent conversation sessions.
#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <generator>

namespace euxis::runtime {

/// @brief roles in a conversation.
enum class Role { User, Assistant, System };

/// @brief A single message in a session.
struct SessionMessage {
    Role role;
    std::string content;
    std::string agent_id;
    std::string model;
    std::string timestamp;
    double duration_ms{0.0};
    std::string decision_trace_hash; ///< Cryptographic hash of the intermediate CoT AST
};

/// @brief Point-in-time state of a conversation branch.
struct SessionSnapshot {
    std::string session_id;
    std::string branch_id;
    std::string agent_id;
    std::vector<SessionMessage> messages;
};

/// @brief Abstract interface for session persistence.
class ISessionStore {
public:
    virtual ~ISessionStore() = default;

    /// @brief Persist a session snapshot.
    virtual auto save(const SessionSnapshot& snapshot)
        -> std::expected<void, std::string> = 0;

    /// @brief Load a session snapshot.
    virtual auto load(const std::string& session_id, const std::string& branch = "main")
        -> std::expected<SessionSnapshot, std::string> = 0;

    /// @brief Lazily stream session episodes for long-horizon reasoning.
    virtual auto stream_episodes(const std::string& session_id, const std::string& branch = "main")
        -> std::generator<SessionMessage> = 0;

    /// @brief List all branches for a given session.
    virtual auto list_branches(const std::string& session_id)
        -> std::vector<std::string> = 0;

    /// @brief Truncate session history.
    virtual auto compact(const std::string& session_id, size_t keep_last_n)
        -> std::expected<void, std::string> = 0;
};

/// @brief Factory function to create a default filesystem-based session store.
auto make_session_store(const std::string& base_dir)
    -> std::unique_ptr<ISessionStore>;

/// @brief Factory function to create a high-performance in-memory session store (Stateless Scaling).
auto make_memory_session_store()
    -> std::unique_ptr<ISessionStore>;

} // namespace euxis::runtime
