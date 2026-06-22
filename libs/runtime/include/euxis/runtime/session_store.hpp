/// @file
/// @brief Persistent storage for agent conversation sessions.
#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "streaming.hpp"  // EUXIS_HAS_STD_GENERATOR

namespace euxis::runtime {

/// @brief roles in a conversation.
enum class Role { User, Assistant, System };

/// @brief A single message in a session.
struct SessionMessage {
    Role role{Role::User};
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

    /// @brief Stream session episodes for long-horizon reasoning.
    ///
    /// Lazy return shape under `EUXIS_HAS_STD_GENERATOR` (GCC 14's
    /// libstdc++ defines `__cpp_lib_generator`; libc++ does not as
    /// of 2026-06). On toolchains without `<generator>` the same
    /// method returns a fully-materialised vector instead. Range-for
    /// at the call site is identical either way.
    ///
    /// Note: under EUXIS_HAS_STD_GENERATOR the params are taken by
    /// value, not by const&. C++ coroutines do NOT extend the
    /// lifetime of reference parameters across suspensions, so a
    /// reference into the caller's stack would dangle the first time
    /// the generator suspends and resumes (caught as ASan stack-
    /// use-after-scope on Ubuntu GCC 14, see commit 73ed3b6). The
    /// non-coroutine vector overload keeps `const&` for parity.
#if defined(EUXIS_HAS_STD_GENERATOR)
    virtual auto stream_episodes(std::string session_id, std::string branch = "main")
        -> std::generator<SessionMessage> = 0;
#else
    virtual auto stream_episodes(const std::string& session_id, const std::string& branch = "main")
        -> std::vector<SessionMessage> = 0;
#endif

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
