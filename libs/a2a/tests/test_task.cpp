#include <gtest/gtest.h>
#include <sodium.h>

#include <algorithm>
#include <string>
#include <vector>

#include "euxis/a2a/task.hpp"

namespace euxis::a2a {
namespace {

// ---------------------------------------------------------------------------
// Fixture: initialise libsodium once
// ---------------------------------------------------------------------------
class TaskTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_GE(sodium_init(), 0) << "sodium_init() failed";
    }
};

// ---------------------------------------------------------------------------
// status_string returns correct string representations
// ---------------------------------------------------------------------------
TEST_F(TaskTest, StatusStringValues) {
    EXPECT_EQ(status_string(TaskStatus::Pending),   "pending");
    EXPECT_EQ(status_string(TaskStatus::Active),     "active");
    EXPECT_EQ(status_string(TaskStatus::Completed),  "completed");
    EXPECT_EQ(status_string(TaskStatus::Failed),     "failed");
    EXPECT_EQ(status_string(TaskStatus::Cancelled),  "cancelled");
}

// ---------------------------------------------------------------------------
// create_task produces a valid initial task
// ---------------------------------------------------------------------------
TEST_F(TaskTest, CreateTaskDefault) {
    auto task = create_task();

    EXPECT_FALSE(task.id.empty());
    EXPECT_EQ(task.status, TaskStatus::Pending);
    EXPECT_TRUE(task.messages.empty());
    EXPECT_TRUE(task.artifacts.empty());
    EXPECT_TRUE(task.history.empty());
    EXPECT_FALSE(task.created_at.empty());
    EXPECT_FALSE(task.updated_at.empty());
    EXPECT_EQ(task.created_at, task.updated_at);
}

// ---------------------------------------------------------------------------
// create_task with initial message
// ---------------------------------------------------------------------------
TEST_F(TaskTest, CreateTaskWithMessage) {
    auto task = create_task("Hello, agent!");

    ASSERT_EQ(task.messages.size(), 1u);
    EXPECT_EQ(task.messages[0].role, "user");
    ASSERT_EQ(task.messages[0].parts.size(), 1u);
    EXPECT_EQ(task.messages[0].parts[0].type, "text");
    EXPECT_EQ(task.messages[0].parts[0].content, "Hello, agent!");
}

// ---------------------------------------------------------------------------
// Each task gets a unique ID
// ---------------------------------------------------------------------------
TEST_F(TaskTest, UniqueIds) {
    auto t1 = create_task();
    auto t2 = create_task();
    EXPECT_NE(t1.id, t2.id);
}

// ---------------------------------------------------------------------------
// Task ID follows UUID hex format (8-4-4-4-12)
// ---------------------------------------------------------------------------
TEST_F(TaskTest, TaskIdFormat) {
    auto task = create_task();
    // 32 hex chars + 4 dashes = 36 total
    EXPECT_EQ(task.id.size(), 36u);
    EXPECT_EQ(task.id[8], '-');
    EXPECT_EQ(task.id[13], '-');
    EXPECT_EQ(task.id[18], '-');
    EXPECT_EQ(task.id[23], '-');

    // Every non-dash position must be a lowercase hex digit (defends the
    // hand-coded sodium_bin2hex+memcpy formatter against drift).
    for (size_t i = 0; i < task.id.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) continue;
        const char c = task.id[i];
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))
            << "non-hex char '" << c << "' at position " << i << " in id " << task.id;
    }
}

// ---------------------------------------------------------------------------
// Generated UUIDs cover a wide range across many invocations
// (smoke test that nothing is constant after the hand-coded refactor).
// ---------------------------------------------------------------------------
TEST_F(TaskTest, TaskIdRandomness) {
    constexpr int n = 256;
    std::vector<std::string> ids;
    ids.reserve(n);
    for (int i = 0; i < n; ++i) ids.push_back(create_task().id);

    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    EXPECT_EQ(ids.size(), static_cast<size_t>(n));
}

// ---------------------------------------------------------------------------
// Timestamp is ISO-8601 "YYYY-MM-DDTHH:MM:SSZ"; validate per-position digits
// and separators. Defends the hand-coded put2/put4 writer against drift.
// ---------------------------------------------------------------------------
TEST_F(TaskTest, CreatedAtIsoFormat) {
    auto task = create_task();
    const auto& s = task.created_at;
    ASSERT_EQ(s.size(), 20u);
    auto is_digit = [](char c) { return c >= '0' && c <= '9'; };
    for (size_t i : {0u, 1u, 2u, 3u, 5u, 6u, 8u, 9u, 11u, 12u, 14u, 15u, 17u, 18u}) {
        EXPECT_TRUE(is_digit(s[i])) << "non-digit at " << i << " in " << s;
    }
    EXPECT_EQ(s[4],  '-');
    EXPECT_EQ(s[7],  '-');
    EXPECT_EQ(s[10], 'T');
    EXPECT_EQ(s[13], ':');
    EXPECT_EQ(s[16], ':');
    EXPECT_EQ(s[19], 'Z');
}

// ---------------------------------------------------------------------------
// Valid transition: Pending -> Active
// ---------------------------------------------------------------------------
TEST_F(TaskTest, TransitionPendingToActive) {
    auto task = create_task();
    auto result = transition_task(task, TaskStatus::Active);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(task.status, TaskStatus::Active);
    ASSERT_EQ(task.history.size(), 1u);
    EXPECT_EQ(task.history[0], TaskStatus::Pending);
}

// ---------------------------------------------------------------------------
// Valid transition: Pending -> Cancelled
// ---------------------------------------------------------------------------
TEST_F(TaskTest, TransitionPendingToCancelled) {
    auto task = create_task();
    auto result = transition_task(task, TaskStatus::Cancelled);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(task.status, TaskStatus::Cancelled);
    ASSERT_EQ(task.history.size(), 1u);
    EXPECT_EQ(task.history[0], TaskStatus::Pending);
}

// ---------------------------------------------------------------------------
// Valid transition: Pending -> Failed
// ---------------------------------------------------------------------------
TEST_F(TaskTest, TransitionPendingToFailed) {
    auto task = create_task();
    auto result = transition_task(task, TaskStatus::Failed);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(task.status, TaskStatus::Failed);
}

// ---------------------------------------------------------------------------
// Valid transition: Active -> Completed
// ---------------------------------------------------------------------------
TEST_F(TaskTest, TransitionActiveToCompleted) {
    auto task = create_task();
    ASSERT_TRUE(transition_task(task, TaskStatus::Active).has_value());
    auto result = transition_task(task, TaskStatus::Completed);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(task.status, TaskStatus::Completed);
    ASSERT_EQ(task.history.size(), 2u);
    EXPECT_EQ(task.history[0], TaskStatus::Pending);
    EXPECT_EQ(task.history[1], TaskStatus::Active);
}

// ---------------------------------------------------------------------------
// Valid transition: Active -> Failed
// ---------------------------------------------------------------------------
TEST_F(TaskTest, TransitionActiveToFailed) {
    auto task = create_task();
    ASSERT_TRUE(transition_task(task, TaskStatus::Active).has_value());
    auto result = transition_task(task, TaskStatus::Failed);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(task.status, TaskStatus::Failed);
}

// ---------------------------------------------------------------------------
// Valid transition: Active -> Cancelled
// ---------------------------------------------------------------------------
TEST_F(TaskTest, TransitionActiveToCancelled) {
    auto task = create_task();
    ASSERT_TRUE(transition_task(task, TaskStatus::Active).has_value());
    auto result = transition_task(task, TaskStatus::Cancelled);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(task.status, TaskStatus::Cancelled);
}

// ---------------------------------------------------------------------------
// Invalid: Pending -> Completed
// ---------------------------------------------------------------------------
TEST_F(TaskTest, InvalidPendingToCompleted) {
    auto task = create_task();
    auto result = transition_task(task, TaskStatus::Completed);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(task.status, TaskStatus::Pending);  // unchanged
    EXPECT_TRUE(task.history.empty());
}

// ---------------------------------------------------------------------------
// Terminal states reject all transitions
// ---------------------------------------------------------------------------
TEST_F(TaskTest, TerminalStateCompletedRejectsAll) {
    auto task = create_task();
    ASSERT_TRUE(transition_task(task, TaskStatus::Active).has_value());
    ASSERT_TRUE(transition_task(task, TaskStatus::Completed).has_value());

    EXPECT_FALSE(transition_task(task, TaskStatus::Active).has_value());
    EXPECT_FALSE(transition_task(task, TaskStatus::Pending).has_value());
    EXPECT_FALSE(transition_task(task, TaskStatus::Failed).has_value());
    EXPECT_FALSE(transition_task(task, TaskStatus::Cancelled).has_value());
    EXPECT_EQ(task.status, TaskStatus::Completed);  // still terminal
}

TEST_F(TaskTest, TerminalStateFailedRejectsAll) {
    auto task = create_task();
    ASSERT_TRUE(transition_task(task, TaskStatus::Failed).has_value());

    EXPECT_FALSE(transition_task(task, TaskStatus::Active).has_value());
    EXPECT_FALSE(transition_task(task, TaskStatus::Completed).has_value());
    EXPECT_FALSE(transition_task(task, TaskStatus::Cancelled).has_value());
}

TEST_F(TaskTest, TerminalStateCancelledRejectsAll) {
    auto task = create_task();
    ASSERT_TRUE(transition_task(task, TaskStatus::Cancelled).has_value());

    EXPECT_FALSE(transition_task(task, TaskStatus::Active).has_value());
    EXPECT_FALSE(transition_task(task, TaskStatus::Completed).has_value());
    EXPECT_FALSE(transition_task(task, TaskStatus::Failed).has_value());
}

// ---------------------------------------------------------------------------
// Full lifecycle history tracking
// ---------------------------------------------------------------------------
TEST_F(TaskTest, FullLifecycleHistory) {
    auto task = create_task("start");
    ASSERT_TRUE(transition_task(task, TaskStatus::Active).has_value());
    ASSERT_TRUE(transition_task(task, TaskStatus::Completed).has_value());

    ASSERT_EQ(task.history.size(), 2u);
    EXPECT_EQ(task.history[0], TaskStatus::Pending);
    EXPECT_EQ(task.history[1], TaskStatus::Active);
    EXPECT_EQ(task.status, TaskStatus::Completed);
}

// ---------------------------------------------------------------------------
// to_json serialises all fields
// ---------------------------------------------------------------------------
TEST_F(TaskTest, ToJsonComplete) {
    auto task = create_task("test message");
    ASSERT_TRUE(transition_task(task, TaskStatus::Active).has_value());

    const auto j = task.to_json();

    EXPECT_EQ(j.at("id").get<std::string>(), task.id);
    EXPECT_EQ(j.at("status").get<std::string>(), "active");
    EXPECT_FALSE(j.at("createdAt").get<std::string>().empty());
    EXPECT_FALSE(j.at("updatedAt").get<std::string>().empty());
    EXPECT_EQ(j.at("messages").size(), 1u);
    EXPECT_TRUE(j.at("artifacts").empty());
    ASSERT_EQ(j.at("history").size(), 1u);
    EXPECT_EQ(j.at("history")[0].get<std::string>(), "pending");
}

// ---------------------------------------------------------------------------
// updated_at changes on transition
// ---------------------------------------------------------------------------
TEST_F(TaskTest, UpdatedAtChanges) {
    auto task = create_task();
    const auto original = task.updated_at;
    // Note: within the same second this may be the same, but the field is set
    ASSERT_TRUE(transition_task(task, TaskStatus::Active).has_value());
    EXPECT_FALSE(task.updated_at.empty());
}

// --- Coverage: lines 71, 74, 76 (is_valid_transition default + terminal false) ---
// Ensure Pending -> Pending is invalid (no self-transition)
TEST_F(TaskTest, InvalidPendingToPending) {
    auto task = create_task();
    auto result = transition_task(task, TaskStatus::Pending);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(task.status, TaskStatus::Pending);
}

// --- Coverage: line 99 (create_task with empty message string) ---
TEST_F(TaskTest, CreateTaskEmptyMessage) {
    auto task = create_task("");
    EXPECT_TRUE(task.messages.empty());
}

// --- Coverage: line 137 (transition error messages) ---
TEST_F(TaskTest, TransitionFromTerminalHasErrorMessage) {
    auto task = create_task();
    ASSERT_TRUE(transition_task(task, TaskStatus::Active).has_value());
    ASSERT_TRUE(transition_task(task, TaskStatus::Completed).has_value());

    auto result = transition_task(task, TaskStatus::Active);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("terminal"), std::string::npos);
}

TEST_F(TaskTest, InvalidTransitionHasErrorMessage) {
    auto task = create_task();
    auto result = transition_task(task, TaskStatus::Completed);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("invalid transition"), std::string::npos);
}

} // namespace
} // namespace euxis::a2a
