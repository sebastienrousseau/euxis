#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#include "euxis/runtime/lifecycle.hpp"

namespace euxis::runtime {
namespace {

class LifecycleTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_;

    void SetUp() override {
        tmp_ = std::filesystem::temp_directory_path() / "euxis_lifecycle_test";
        std::filesystem::remove_all(tmp_);
    }

    void TearDown() override { std::filesystem::remove_all(tmp_); }
};

TEST_F(LifecycleTest, StateRoundTrip) {
    EXPECT_EQ(state_to_string(AgentState::Idle), "idle");
    EXPECT_EQ(state_to_string(AgentState::Ready), "ready");
    EXPECT_EQ(state_to_string(AgentState::Running), "running");
    EXPECT_EQ(state_to_string(AgentState::Paused), "paused");
    EXPECT_EQ(state_to_string(AgentState::Completed), "completed");
    EXPECT_EQ(state_to_string(AgentState::Failed), "failed");
    EXPECT_EQ(state_to_string(AgentState::Terminated), "terminated");

    EXPECT_EQ(state_from_string("running"), AgentState::Running);
    EXPECT_EQ(state_from_string("junk"), AgentState::Idle);
}

TEST_F(LifecycleTest, ValidTransitions) {
    auto from_idle = valid_transitions(AgentState::Idle);
    EXPECT_TRUE(from_idle.contains(AgentState::Ready));
    EXPECT_FALSE(from_idle.contains(AgentState::Running));

    auto from_running = valid_transitions(AgentState::Running);
    EXPECT_TRUE(from_running.contains(AgentState::Paused));
    EXPECT_TRUE(from_running.contains(AgentState::Completed));
    EXPECT_TRUE(from_running.contains(AgentState::Failed));

    auto from_terminated = valid_transitions(AgentState::Terminated);
    EXPECT_TRUE(from_terminated.empty());
}

TEST_F(LifecycleTest, NewAgentStartsIdle) {
    LifecycleManager mgr(tmp_);
    EXPECT_EQ(mgr.get_state("agent1"), AgentState::Idle);
}

TEST_F(LifecycleTest, ValidTransitionSucceeds) {
    LifecycleManager mgr(tmp_);
    EXPECT_TRUE(mgr.transition("agent1", AgentState::Ready, "s1"));
    EXPECT_EQ(mgr.get_state("agent1"), AgentState::Ready);
}

TEST_F(LifecycleTest, InvalidTransitionFails) {
    LifecycleManager mgr(tmp_);
    // Idle -> Running is invalid (must go through Ready)
    EXPECT_FALSE(mgr.transition("agent1", AgentState::Running));
    EXPECT_EQ(mgr.get_state("agent1"), AgentState::Idle);
}

TEST_F(LifecycleTest, FullLifecycle) {
    LifecycleManager mgr(tmp_);
    EXPECT_TRUE(mgr.transition("a", AgentState::Ready));
    EXPECT_TRUE(mgr.transition("a", AgentState::Running));
    EXPECT_TRUE(mgr.transition("a", AgentState::Completed));
    EXPECT_TRUE(mgr.transition("a", AgentState::Idle));
    EXPECT_EQ(mgr.get_state("a"), AgentState::Idle);
}

TEST_F(LifecycleTest, PauseResume) {
    LifecycleManager mgr(tmp_);
    EXPECT_TRUE(mgr.transition("a", AgentState::Ready));
    EXPECT_TRUE(mgr.transition("a", AgentState::Running));
    EXPECT_TRUE(mgr.transition("a", AgentState::Paused));
    EXPECT_TRUE(mgr.transition("a", AgentState::Running));
    EXPECT_EQ(mgr.get_state("a"), AgentState::Running);
}

TEST_F(LifecycleTest, TerminatedIsFinal) {
    LifecycleManager mgr(tmp_);
    EXPECT_TRUE(mgr.transition("a", AgentState::Ready));
    EXPECT_TRUE(mgr.transition("a", AgentState::Terminated));
    EXPECT_FALSE(mgr.transition("a", AgentState::Idle));
    EXPECT_EQ(mgr.get_state("a"), AgentState::Terminated);
}

TEST_F(LifecycleTest, TransitionHistory) {
    LifecycleManager mgr(tmp_);
    mgr.transition("a", AgentState::Ready, "s1");
    mgr.transition("a", AgentState::Running, "s1");
    auto hist = mgr.history("a");
    ASSERT_EQ(hist.size(), 2);
    EXPECT_EQ(hist[0].to, AgentState::Ready);
    EXPECT_EQ(hist[1].to, AgentState::Running);
}

TEST_F(LifecycleTest, PersistAndLoad) {
    {
        LifecycleManager mgr(tmp_);
        mgr.transition("agent1", AgentState::Ready);
        mgr.transition("agent1", AgentState::Running);
        mgr.transition("agent2", AgentState::Ready);
    }

    LifecycleManager mgr2(tmp_);
    mgr2.load_from_disk();
    EXPECT_EQ(mgr2.get_state("agent1"), AgentState::Running);
    EXPECT_EQ(mgr2.get_state("agent2"), AgentState::Ready);
    EXPECT_EQ(mgr2.agents().size(), 2);
}

TEST_F(LifecycleTest, MultipleAgents) {
    LifecycleManager mgr(tmp_);
    mgr.transition("a1", AgentState::Ready);
    mgr.transition("a2", AgentState::Ready);
    mgr.transition("a3", AgentState::Ready);
    auto names = mgr.agents();
    EXPECT_EQ(names.size(), 3);
}

TEST_F(LifecycleTest, FailedCanRecoverOrTerminate) {
    LifecycleManager mgr(tmp_);
    mgr.transition("a", AgentState::Ready);
    mgr.transition("a", AgentState::Running);
    mgr.transition("a", AgentState::Failed);
    // Can recover to Idle
    EXPECT_TRUE(mgr.transition("a", AgentState::Idle));
    EXPECT_EQ(mgr.get_state("a"), AgentState::Idle);
}

// --- Coverage: line 21 (default return in state_to_string switch) ---
TEST_F(LifecycleTest, StateToStringDefaultFallback) {
    // Cast an out-of-range int to AgentState to trigger the default return
    auto bad = static_cast<AgentState>(99);
    EXPECT_EQ(state_to_string(bad), "idle");
}

// --- Coverage: line 52 (default return in valid_transitions switch) ---
TEST_F(LifecycleTest, ValidTransitionsDefaultFallback) {
    auto bad = static_cast<AgentState>(99);
    auto result = valid_transitions(bad);
    EXPECT_TRUE(result.empty());
}

// --- Coverage: lines 130-131 (catch block in load_from_disk for malformed jsonl) ---
TEST_F(LifecycleTest, LoadFromDiskMalformedTransitionLine) {
    // Set up a valid lifecycle directory with a state file and malformed transitions
    LifecycleManager mgr(tmp_);
    mgr.transition("agent1", AgentState::Ready);

    // Write a malformed line into the transitions file
    auto transitions_path = tmp_ / "lifecycle" / "transitions.jsonl";
    std::ofstream f(transitions_path, std::ios::app);
    f << "this is not valid json\n";
    f.close();

    // Load should succeed, skipping the malformed line
    LifecycleManager mgr2(tmp_);
    EXPECT_NO_THROW(mgr2.load_from_disk());
    EXPECT_EQ(mgr2.get_state("agent1"), AgentState::Ready);
}

// --- Coverage: line 150 (agents() returning sorted list) ---
TEST_F(LifecycleTest, AgentsReturnsSorted) {
    LifecycleManager mgr(tmp_);
    mgr.transition("zebra", AgentState::Ready);
    mgr.transition("alpha", AgentState::Ready);
    mgr.transition("middle", AgentState::Ready);
    auto names = mgr.agents();
    ASSERT_EQ(names.size(), 3u);
    EXPECT_EQ(names[0], "alpha");
    EXPECT_EQ(names[1], "middle");
    EXPECT_EQ(names[2], "zebra");
}

// --- Coverage: line 159 (history returns empty for unknown agent) ---
TEST_F(LifecycleTest, HistoryForUnknownAgentIsEmpty) {
    LifecycleManager mgr(tmp_);
    mgr.transition("a", AgentState::Ready);
    auto hist = mgr.history("nonexistent");
    EXPECT_TRUE(hist.empty());
}

} // namespace
} // namespace euxis::runtime
