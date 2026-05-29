/// @file
/// @brief Tests for the ACP control-plane logic.

#include <gtest/gtest.h>

#include <string>
#include <unordered_set>
#include <vector>

#include "euxis/network/acp.hpp"

namespace euxis::network::acp {
namespace {

// ---------------------------------------------------------------------------
// State helpers
// ---------------------------------------------------------------------------

TEST(AcpStateTest, NameRenderingIsStable) {
    EXPECT_EQ(state_name(SessionState::Pending),   "pending");
    EXPECT_EQ(state_name(SessionState::Running),   "running");
    EXPECT_EQ(state_name(SessionState::Paused),    "paused");
    EXPECT_EQ(state_name(SessionState::Completed), "completed");
    EXPECT_EQ(state_name(SessionState::Failed),    "failed");
    EXPECT_EQ(state_name(SessionState::Cancelled), "cancelled");
}

TEST(AcpStateTest, TerminalStatesAreSticky) {
    EXPECT_FALSE(is_terminal(SessionState::Pending));
    EXPECT_FALSE(is_terminal(SessionState::Running));
    EXPECT_FALSE(is_terminal(SessionState::Paused));
    EXPECT_TRUE(is_terminal(SessionState::Completed));
    EXPECT_TRUE(is_terminal(SessionState::Failed));
    EXPECT_TRUE(is_terminal(SessionState::Cancelled));
}

TEST(AcpStateTest, ValidTransitions) {
    using S = SessionState;
    EXPECT_TRUE(is_valid_transition(S::Pending, S::Running));
    EXPECT_TRUE(is_valid_transition(S::Pending, S::Failed));
    EXPECT_TRUE(is_valid_transition(S::Pending, S::Cancelled));
    EXPECT_TRUE(is_valid_transition(S::Running, S::Paused));
    EXPECT_TRUE(is_valid_transition(S::Running, S::Completed));
    EXPECT_TRUE(is_valid_transition(S::Paused,  S::Running));
    EXPECT_TRUE(is_valid_transition(S::Paused,  S::Completed));
}

TEST(AcpStateTest, InvalidTransitions) {
    using S = SessionState;
    EXPECT_FALSE(is_valid_transition(S::Pending, S::Paused))
        << "cannot pause something never running";
    EXPECT_FALSE(is_valid_transition(S::Pending, S::Completed))
        << "cannot complete before running";
    EXPECT_FALSE(is_valid_transition(S::Running, S::Pending));
    EXPECT_FALSE(is_valid_transition(S::Completed, S::Running))
        << "terminal is sticky";
    EXPECT_FALSE(is_valid_transition(S::Failed,    S::Running));
    EXPECT_FALSE(is_valid_transition(S::Cancelled, S::Running));
    EXPECT_FALSE(is_valid_transition(S::Running, S::Running))
        << "no self-transitions";
}

// ---------------------------------------------------------------------------
// Event ledger
// ---------------------------------------------------------------------------

TEST(AcpEventLedgerTest, AppendAndQueryBySession) {
    AcpEventLedger ledger;
    ledger.append({.session_id = "s1", .kind = "spawned",       .payload = {}, .at_unix_ms = 1});
    ledger.append({.session_id = "s2", .kind = "spawned",       .payload = {}, .at_unix_ms = 2});
    ledger.append({.session_id = "s1", .kind = "state_changed", .payload = {}, .at_unix_ms = 3});

    EXPECT_EQ(ledger.size(), 3u);

    auto s1 = ledger.events_for("s1");
    ASSERT_EQ(s1.size(), 2u);
    EXPECT_EQ(s1[0].kind, "spawned");
    EXPECT_EQ(s1[1].kind, "state_changed");
    EXPECT_EQ(s1[1].at_unix_ms, 3);

    auto s2 = ledger.events_for("s2");
    EXPECT_EQ(s2.size(), 1u);
    EXPECT_EQ(ledger.events_for("unknown").size(), 0u);
}

TEST(AcpEventLedgerTest, AllEventsPreservesInsertionOrder) {
    AcpEventLedger ledger;
    for (int i = 0; i < 5; ++i) {
        ledger.append({
            .session_id = std::to_string(i),
            .kind       = "spawned",
            .payload    = {},
            .at_unix_ms = i,
        });
    }
    auto all = ledger.all_events();
    ASSERT_EQ(all.size(), 5u);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(all[static_cast<std::size_t>(i)].at_unix_ms, i);
    }
}

TEST(AcpEventLedgerTest, ClearEmptiesLedger) {
    AcpEventLedger ledger;
    ledger.append({.session_id = "x", .kind = "y", .payload = {}, .at_unix_ms = 0});
    ledger.clear();
    EXPECT_EQ(ledger.size(), 0u);
    EXPECT_TRUE(ledger.all_events().empty());
}

// ---------------------------------------------------------------------------
// Policy
// ---------------------------------------------------------------------------

TEST(AcpPolicyTest, AllowlistOnlyPermitsListedAgents) {
    AcpPolicy p{std::unordered_set<std::string>{"auditor", "reviewer"}};
    EXPECT_TRUE(p.is_allowed("auditor"));
    EXPECT_TRUE(p.is_allowed("reviewer"));
    EXPECT_FALSE(p.is_allowed("intruder"));
    EXPECT_FALSE(p.is_allowed(""));
    EXPECT_EQ(p.allowed_count(), 2u);
}

TEST(AcpPolicyTest, PermissivePolicyAllowsAnything) {
    auto p = AcpPolicy::permissive();
    EXPECT_TRUE(p.is_allowed("any-id"));
    EXPECT_TRUE(p.is_allowed(""));
    EXPECT_EQ(p.allowed_count(), 0u);
}

// ---------------------------------------------------------------------------
// Session manager
// ---------------------------------------------------------------------------

auto make_spec(std::string id, std::string agent_id) -> SessionSpec {
    SessionSpec s;
    s.id          = std::move(id);
    s.agent_id    = std::move(agent_id);
    s.working_dir = "/tmp";
    s.created_at_unix_ms = 100;
    return s;
}

TEST(AcpSessionManagerTest, SpawnSucceedsForPermittedAgent) {
    AcpEventLedger     ledger;
    AcpSessionManager  mgr{AcpPolicy::permissive(), ledger};

    auto r = mgr.spawn(make_spec("s1", "auditor"), 100);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, "s1");
    EXPECT_EQ(mgr.size(), 1u);
    EXPECT_EQ(ledger.size(), 1u);
    EXPECT_EQ(ledger.all_events()[0].kind, "spawned");

    auto got = mgr.get("s1");
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(got->state, SessionState::Pending);
    EXPECT_EQ(got->updated_at_unix_ms, 100);
}

TEST(AcpSessionManagerTest, SpawnRejectsEmptyFields) {
    AcpEventLedger ledger;
    AcpSessionManager mgr{AcpPolicy::permissive(), ledger};

    EXPECT_FALSE(mgr.spawn(make_spec("",   "auditor"), 0).has_value());
    EXPECT_FALSE(mgr.spawn(make_spec("s2", ""),        0).has_value());
}

TEST(AcpSessionManagerTest, SpawnRejectsDisallowedAgent) {
    AcpEventLedger ledger;
    AcpSessionManager mgr{
        AcpPolicy{std::unordered_set<std::string>{"auditor"}}, ledger};

    auto r = mgr.spawn(make_spec("s1", "intruder"), 0);
    EXPECT_FALSE(r.has_value());
    EXPECT_EQ(mgr.size(), 0u);
    EXPECT_EQ(ledger.size(), 0u);
}

TEST(AcpSessionManagerTest, SpawnRejectsDuplicateId) {
    AcpEventLedger ledger;
    AcpSessionManager mgr{AcpPolicy::permissive(), ledger};

    ASSERT_TRUE(mgr.spawn(make_spec("s1", "auditor"), 0).has_value());
    EXPECT_FALSE(mgr.spawn(make_spec("s1", "auditor"), 0).has_value());
    EXPECT_EQ(mgr.size(), 1u);
}

TEST(AcpSessionManagerTest, SetStateLegalTransition) {
    AcpEventLedger ledger;
    AcpSessionManager mgr{AcpPolicy::permissive(), ledger};

    (void)mgr.spawn(make_spec("s1", "auditor"), 0);
    auto r1 = mgr.set_state("s1", SessionState::Running,   1);
    EXPECT_TRUE(r1.has_value());
    auto r2 = mgr.set_state("s1", SessionState::Completed, 2);
    EXPECT_TRUE(r2.has_value());

    EXPECT_EQ(ledger.size(), 3u);  // spawned + 2 state_changed
    auto events = ledger.events_for("s1");
    EXPECT_EQ(events[1].kind, "state_changed");
    EXPECT_EQ(events[1].payload["from"], "pending");
    EXPECT_EQ(events[1].payload["to"],   "running");
}

TEST(AcpSessionManagerTest, SetStateIllegalTransitionReturnsError) {
    AcpEventLedger ledger;
    AcpSessionManager mgr{AcpPolicy::permissive(), ledger};

    (void)mgr.spawn(make_spec("s1", "auditor"), 0);
    // Pending -> Completed is illegal.
    auto r = mgr.set_state("s1", SessionState::Completed, 1);
    EXPECT_FALSE(r.has_value());
    EXPECT_EQ(mgr.get("s1")->state, SessionState::Pending);
    // Spawn was recorded, illegal transition was NOT.
    EXPECT_EQ(ledger.size(), 1u);
}

TEST(AcpSessionManagerTest, SetStateOnUnknownIdReturnsError) {
    AcpEventLedger ledger;
    AcpSessionManager mgr{AcpPolicy::permissive(), ledger};
    auto r = mgr.set_state("does-not-exist", SessionState::Running, 0);
    EXPECT_FALSE(r.has_value());
}

TEST(AcpSessionManagerTest, CancelTransitionsToTerminal) {
    AcpEventLedger ledger;
    AcpSessionManager mgr{AcpPolicy::permissive(), ledger};
    (void)mgr.spawn(make_spec("s1", "auditor"), 0);
    auto r = mgr.cancel("s1", 1);
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(mgr.get("s1")->state, SessionState::Cancelled);

    // Once cancelled, further transitions fail.
    auto r2 = mgr.set_state("s1", SessionState::Running, 2);
    EXPECT_FALSE(r2.has_value());
}

TEST(AcpSessionManagerTest, ListReturnsSortedIds) {
    AcpEventLedger ledger;
    AcpSessionManager mgr{AcpPolicy::permissive(), ledger};
    (void)mgr.spawn(make_spec("z", "auditor"), 0);
    (void)mgr.spawn(make_spec("a", "auditor"), 0);
    (void)mgr.spawn(make_spec("m", "auditor"), 0);
    auto ids = mgr.list();
    ASSERT_EQ(ids.size(), 3u);
    EXPECT_EQ(ids[0], "a");
    EXPECT_EQ(ids[1], "m");
    EXPECT_EQ(ids[2], "z");
}

} // namespace
} // namespace euxis::network::acp
