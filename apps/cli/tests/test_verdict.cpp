#include <gtest/gtest.h>
#include "euxis/cli/verdict.hpp"

namespace euxis::cli::verdict {

// --- to_string round-trip tests ---

TEST(VerdictTest, ToStringState) {
    EXPECT_EQ(to_string(State::Trusted), "TRUSTED");
    EXPECT_EQ(to_string(State::Blocked), "BLOCKED");
    EXPECT_EQ(to_string(State::Inconclusive), "INCONCLUSIVE");
    EXPECT_EQ(to_string(State::Caution), "CAUTION");
    EXPECT_EQ(to_string(State::TrustedGaps), "TRUSTED WITH GAPS");
}

TEST(VerdictTest, ToStringAgentState) {
    EXPECT_EQ(to_string(AgentState::Pass), "PASS");
    EXPECT_EQ(to_string(AgentState::Failed), "FAILED");
    EXPECT_EQ(to_string(AgentState::Timeout), "TIMEOUT");
    EXPECT_EQ(to_string(AgentState::Degraded), "DEGRADED");
}

// --- classify_agent tests ---

TEST(VerdictTest, ClassifyAgentPass) {
    EXPECT_EQ(classify_agent("PASS", false, false), AgentState::Pass);
}

TEST(VerdictTest, ClassifyAgentTimeout) {
    EXPECT_EQ(classify_agent("PASS", true, false), AgentState::Timeout);
}

TEST(VerdictTest, ClassifyAgentDegraded) {
    EXPECT_EQ(classify_agent("PASS", false, true), AgentState::Degraded);
}

TEST(VerdictTest, ClassifyAgentFail) {
    EXPECT_EQ(classify_agent("FAIL", false, false), AgentState::Failed);
}

// --- is_decisive_negative tests ---

TEST(VerdictTest, DecisiveNegativeFailed) {
    EXPECT_TRUE(is_decisive_negative(AgentState::Failed, "FAIL"));
}

TEST(VerdictTest, DecisiveNegativeTimeoutWithFail) {
    EXPECT_TRUE(is_decisive_negative(AgentState::Timeout, "FAIL"));
}

TEST(VerdictTest, NotDecisiveNegativePass) {
    EXPECT_FALSE(is_decisive_negative(AgentState::Pass, "PASS"));
}

// --- is_valid_verdict exhaustive tests ---

TEST(VerdictTest, TrustedValid) {
    EXPECT_TRUE(is_valid_verdict(State::Trusted, 0, 0, false, 80, 5));
}

TEST(VerdictTest, TrustedInvalidWithContradictions) {
    EXPECT_FALSE(is_valid_verdict(State::Trusted, 0, 0, true, 100, 5));
}

TEST(VerdictTest, TrustedInvalidWithFails) {
    EXPECT_FALSE(is_valid_verdict(State::Trusted, 1, 0, false, 100, 5));
}

TEST(VerdictTest, BlockedValid) {
    EXPECT_TRUE(is_valid_verdict(State::Blocked, 2, 0, false, 30, 5));
}

TEST(VerdictTest, BlockedInvalidNoFails) {
    EXPECT_FALSE(is_valid_verdict(State::Blocked, 0, 0, false, 50, 5));
}

TEST(VerdictTest, InconclusiveValid) {
    EXPECT_TRUE(is_valid_verdict(State::Inconclusive, 0, 0, true, 30, 5));
}

// --- exit_code tests ---

TEST(VerdictTest, ExitCodeTrusted) {
    EXPECT_EQ(exit_code(State::Trusted), 0);
    EXPECT_EQ(exit_code(State::TrustedGaps), 0);
}

TEST(VerdictTest, ExitCodeBlocked) {
    EXPECT_EQ(exit_code(State::Blocked), 1);
    EXPECT_EQ(exit_code(State::Caution), 1);
}

} // namespace euxis::cli::verdict
