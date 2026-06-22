/// @file
/// @brief Implementation of the ACP control-plane logic.

#include "euxis/network/acp.hpp"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace euxis::network::acp {

auto state_name(SessionState s) noexcept -> std::string_view {
    switch (s) {
        case SessionState::Pending:   return "pending";
        case SessionState::Running:   return "running";
        case SessionState::Paused:    return "paused";
        case SessionState::Completed: return "completed";
        case SessionState::Failed:    return "failed";
        case SessionState::Cancelled: return "cancelled";
    }
    return "unknown";
}

auto is_terminal(SessionState s) noexcept -> bool {
    return s == SessionState::Completed
        || s == SessionState::Failed
        || s == SessionState::Cancelled;
}

auto is_valid_transition(SessionState from, SessionState to) noexcept -> bool {
    if (from == to) return false;                  // No self-transitions.
    if (is_terminal(from)) return false;           // Terminal is sticky.
    switch (from) {
        case SessionState::Pending:
            // Pending -> Running, Failed, Cancelled.
            return to == SessionState::Running
                || to == SessionState::Failed
                || to == SessionState::Cancelled;
        case SessionState::Running:
            // Running -> Paused, Completed, Failed, Cancelled.
            return to == SessionState::Paused
                || to == SessionState::Completed
                || to == SessionState::Failed
                || to == SessionState::Cancelled;
        case SessionState::Paused:
            // Paused -> Running, Completed, Failed, Cancelled.
            return to == SessionState::Running
                || to == SessionState::Completed
                || to == SessionState::Failed
                || to == SessionState::Cancelled;
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// AcpEventLedger
// ---------------------------------------------------------------------------

void AcpEventLedger::append(AcpEvent event) {
    const std::scoped_lock lock{mu_};
    events_.push_back(std::move(event));
}

auto AcpEventLedger::events_for(std::string_view session_id) const
    -> std::vector<AcpEvent> {
    const std::scoped_lock lock{mu_};
    std::vector<AcpEvent> out;
    for (const auto& e : events_) {
        if (e.session_id == session_id) out.push_back(e);
    }
    return out;
}

auto AcpEventLedger::all_events() const -> std::vector<AcpEvent> {
    const std::scoped_lock lock{mu_};
    return events_;
}

auto AcpEventLedger::size() const noexcept -> std::size_t {
    const std::scoped_lock lock{mu_};
    return events_.size();
}

void AcpEventLedger::clear() {
    const std::scoped_lock lock{mu_};
    events_.clear();
}

// ---------------------------------------------------------------------------
// AcpPolicy
// ---------------------------------------------------------------------------

AcpPolicy::AcpPolicy(std::unordered_set<std::string> allowed_agents)
    : permissive_{false}, allowed_agents_{std::move(allowed_agents)} {}

auto AcpPolicy::permissive() -> AcpPolicy {
    AcpPolicy p{std::unordered_set<std::string>{}};
    p.permissive_ = true;
    return p;
}

auto AcpPolicy::is_allowed(std::string_view agent_id) const -> bool {
    if (permissive_) return true;
    // unordered_set::find requires a transparent comparator to take
    // string_view; build a std::string for the lookup. Allowlist sizes
    // are small in practice (single digits to dozens) so this is cheap.
    return allowed_agents_.find(std::string{agent_id}) != allowed_agents_.end();
}

auto AcpPolicy::allowed_count() const noexcept -> std::size_t {
    return allowed_agents_.size();
}

// ---------------------------------------------------------------------------
// AcpSessionManager
// ---------------------------------------------------------------------------

AcpSessionManager::AcpSessionManager(AcpPolicy policy, AcpEventLedger& ledger)
    : policy_{std::move(policy)}, ledger_{ledger} {}

auto AcpSessionManager::spawn(SessionSpec spec, std::int64_t at_unix_ms)
    -> std::expected<std::string, std::string> {
    if (spec.id.empty())       return std::unexpected("session id must not be empty");
    if (spec.agent_id.empty()) return std::unexpected("agent_id must not be empty");
    if (!policy_.is_allowed(spec.agent_id)) {
        return std::unexpected("agent_id not allowed by policy: " + spec.agent_id);
    }

    const std::string id_copy = spec.id;
    {
        const std::scoped_lock lock{mu_};
        if (sessions_.contains(id_copy)) {
            return std::unexpected("session id already exists: " + id_copy);
        }
        AcpSession session;
        session.spec               = std::move(spec);
        session.state              = SessionState::Pending;
        session.updated_at_unix_ms = at_unix_ms;
        sessions_.emplace(id_copy, std::move(session));
    }

    AcpEvent ev;
    ev.session_id = id_copy;
    ev.kind       = "spawned";
    ev.payload    = nlohmann::json::object();
    ev.at_unix_ms = at_unix_ms;
    ledger_.append(std::move(ev));

    return id_copy;
}

auto AcpSessionManager::set_state(std::string_view id,
                                   SessionState new_state,
                                   std::int64_t at_unix_ms)
    -> std::expected<void, std::string> {
    SessionState old_state{};
    {
        const std::scoped_lock lock{mu_};
        const auto it = sessions_.find(std::string{id});
        if (it == sessions_.end()) {
            return std::unexpected("session not found: " + std::string{id});
        }
        old_state = it->second.state;
        if (!is_valid_transition(old_state, new_state)) {
            return std::unexpected(
                std::string{"invalid transition "} +
                std::string{state_name(old_state)} +
                " -> " +
                std::string{state_name(new_state)});
        }
        it->second.state              = new_state;
        it->second.updated_at_unix_ms = at_unix_ms;
    }

    AcpEvent ev;
    ev.session_id = std::string{id};
    ev.kind       = "state_changed";
    ev.payload    = {
        {"from", std::string{state_name(old_state)}},
        {"to",   std::string{state_name(new_state)}},
    };
    ev.at_unix_ms = at_unix_ms;
    ledger_.append(std::move(ev));
    return {};
}

auto AcpSessionManager::cancel(std::string_view id, std::int64_t at_unix_ms)
    -> std::expected<void, std::string> {
    return set_state(id, SessionState::Cancelled, at_unix_ms);
}

auto AcpSessionManager::get(std::string_view id) const
    -> std::optional<AcpSession> {
    const std::scoped_lock lock{mu_};
    const auto it = sessions_.find(std::string{id});
    if (it == sessions_.end()) return std::nullopt;
    return it->second;
}

auto AcpSessionManager::list() const -> std::vector<std::string> {
    const std::scoped_lock lock{mu_};
    std::vector<std::string> out;
    out.reserve(sessions_.size());
    for (const auto& [k, _] : sessions_) out.push_back(k);
    // Deterministic order for callers that compare snapshots.
    std::sort(out.begin(), out.end());
    return out;
}

auto AcpSessionManager::size() const noexcept -> std::size_t {
    const std::scoped_lock lock{mu_};
    return sessions_.size();
}

} // namespace euxis::network::acp
