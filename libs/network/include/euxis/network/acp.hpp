/// @file
/// @brief ACP — Agent Control Protocol (inbound control plane).
///
/// Mirrors the OpenClaw src/acp/ pattern: a managed inbound control
/// surface for agent sessions, distinct from MCP (which is outbound,
/// agent -> tools). Where MCP lets the agent call external tools, ACP
/// lets an external client spawn, steer, and observe persistent agent
/// sessions on the gateway.
///
/// For Euxis the strategic angle is *managed audit sessions*: a CI
/// runner or web UI spawns a long-running audit via ACP, polls its
/// status, and collects the evidence pack when it completes — without
/// holding an HTTP connection open for the full duration.
///
/// This header ships the *logic*: session lifecycle, event ledger,
/// policy. The HTTP/WebSocket transport that consumes these types is a
/// separate cycle.
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::network::acp {

/// @brief Lifecycle state of an ACP session.
///
/// State transitions are linear-with-jumps: Pending -> Running -> any
/// terminal state. Paused can be entered from Running and exited back to
/// Running. Once a session reaches a terminal state (Completed / Failed
/// / Cancelled) no further transitions are allowed.
enum class SessionState {
    Pending,    ///< Session is created but the agent has not started work.
    Running,    ///< Agent is actively processing.
    Paused,     ///< Agent is suspended; can resume to Running.
    Completed,  ///< Terminal: success.
    Failed,     ///< Terminal: error during execution.
    Cancelled,  ///< Terminal: cancelled by the caller.
};

/// @brief Stable lowercase rendering for logging / JSON output.
[[nodiscard]] auto state_name(SessionState s) noexcept -> std::string_view;

/// @brief True iff @p s is a terminal state (no further transitions).
[[nodiscard]] auto is_terminal(SessionState s) noexcept -> bool;

/// @brief True iff transitioning @p from -> @p to is legal.
[[nodiscard]] auto is_valid_transition(SessionState from, SessionState to) noexcept
    -> bool;

/// @brief Caller-supplied spec for a new session.
struct SessionSpec {
    std::string id;              ///< Caller-supplied stable id (UUID/ULID).
    std::string agent_id;        ///< Must be in AcpPolicy::allowed_agents.
    std::string working_dir;     ///< Absolute path the agent runs from.
    nlohmann::json metadata{};   ///< Free-form metadata, opaque to ACP.
    std::int64_t created_at_unix_ms{0};
};

/// @brief A running session tracked by AcpSessionManager.
struct AcpSession {
    SessionSpec spec;
    SessionState state{SessionState::Pending};
    std::int64_t updated_at_unix_ms{0};
};

// ---------------------------------------------------------------------------
// Event ledger — append-only audit log.
// ---------------------------------------------------------------------------

/// @brief One row in the ACP event ledger.
struct AcpEvent {
    std::string  session_id;
    std::string  kind;          ///< e.g. "spawned", "state_changed", "tool_called"
    nlohmann::json payload{};   ///< Free-form, opaque to the ledger
    std::int64_t at_unix_ms{0};
};

/// @brief Append-only event log for an entire ACP server.
///
/// Thread-safe. Reads return copies so concurrent appenders can't tear
/// a vector under a reader. Suitable for short-to-medium runs; long-
/// running deployments should drain old events into persistent storage
/// (out of scope for this cycle).
class AcpEventLedger {
public:
    /// @brief Append a single event.
    void append(AcpEvent event);

    /// @brief All events for one session, in append order.
    [[nodiscard]] auto events_for(std::string_view session_id) const
        -> std::vector<AcpEvent>;

    /// @brief All events across all sessions, in append order.
    [[nodiscard]] auto all_events() const -> std::vector<AcpEvent>;

    /// @brief Total event count.
    [[nodiscard]] auto size() const noexcept -> std::size_t;

    /// @brief Clear all events. Used by tests; production use should
    ///        prefer draining into storage.
    void clear();

private:
    mutable std::mutex   mu_;
    std::vector<AcpEvent> events_;
};

// ---------------------------------------------------------------------------
// Policy — allowlist of agents that can be spawned.
// ---------------------------------------------------------------------------

/// @brief Static allowlist of agent ids that may be spawned via ACP.
///
/// Resolved once at startup and consulted on each spawn() call.
class AcpPolicy {
public:
    /// @brief Construct with an explicit set of allowed agent ids.
    explicit AcpPolicy(std::unordered_set<std::string> allowed_agents);

    /// @brief Convenience: a permissive policy that allows any agent id.
    [[nodiscard]] static auto permissive() -> AcpPolicy;

    /// @brief True when @p agent_id is in the allowlist (or policy is
    ///        permissive).
    [[nodiscard]] auto is_allowed(std::string_view agent_id) const noexcept
        -> bool;

    /// @brief Number of allowed ids (0 means permissive — every id passes).
    [[nodiscard]] auto allowed_count() const noexcept -> std::size_t;

private:
    bool permissive_;
    std::unordered_set<std::string> allowed_agents_;
};

// ---------------------------------------------------------------------------
// Session manager — the orchestrator.
// ---------------------------------------------------------------------------

/// @brief Manager for persistent ACP sessions.
///
/// Spawns sessions against a policy, tracks state, transitions through
/// the ACP state machine, and emits events into the shared ledger. The
/// ledger is borrowed (not owned) so multiple managers can share one.
///
/// Thread-safe via a single internal mutex.
class AcpSessionManager {
public:
    AcpSessionManager(AcpPolicy policy, AcpEventLedger& ledger);

    /// @brief Spawn a new session. Returns the session id on success.
    ///
    /// Errors:
    ///   - empty spec.id
    ///   - empty spec.agent_id
    ///   - spec.agent_id not in policy
    ///   - session with that id already exists
    [[nodiscard]] auto spawn(SessionSpec spec, std::int64_t at_unix_ms)
        -> std::expected<std::string, std::string>;

    /// @brief Transition a session to a new state.
    ///
    /// Errors when the session doesn't exist or the transition is
    /// invalid per is_valid_transition().
    [[nodiscard]] auto set_state(std::string_view id,
                                  SessionState new_state,
                                  std::int64_t at_unix_ms)
        -> std::expected<void, std::string>;

    /// @brief Convenience: transition to Cancelled.
    [[nodiscard]] auto cancel(std::string_view id, std::int64_t at_unix_ms)
        -> std::expected<void, std::string>;

    /// @brief Get the current state of @p id; nullopt when unknown.
    [[nodiscard]] auto get(std::string_view id) const -> std::optional<AcpSession>;

    /// @brief List all session ids currently tracked.
    [[nodiscard]] auto list() const -> std::vector<std::string>;

    /// @brief Number of sessions currently tracked.
    [[nodiscard]] auto size() const noexcept -> std::size_t;

private:
    AcpPolicy           policy_;
    AcpEventLedger&     ledger_;
    mutable std::mutex  mu_;
    std::unordered_map<std::string, AcpSession> sessions_;
};

} // namespace euxis::network::acp
