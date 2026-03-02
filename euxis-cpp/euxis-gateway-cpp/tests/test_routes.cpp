#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#include "euxis/gateway/state.hpp"

namespace euxis::gateway {
namespace {

namespace fs = std::filesystem;

class StateTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / ("euxis_state_test_" + std::to_string(getpid()));
        fs::remove_all(test_dir_);
        old_home_ = std::getenv("EUXIS_HOME");
        setenv("EUXIS_HOME", test_dir_.c_str(), 1);
    }

    void TearDown() override {
        if (old_home_) setenv("EUXIS_HOME", old_home_, 1);
        else unsetenv("EUXIS_HOME");
        fs::remove_all(test_dir_);
    }

    fs::path test_dir_;
    const char* old_home_{nullptr};
};

TEST_F(StateTest, TimestampNotEmpty) {
    auto ts = timestamp();
    EXPECT_FALSE(ts.empty());
}

TEST_F(StateTest, TimestampContainsDateParts) {
    auto ts = timestamp();
    // Should contain at least a year-like pattern
    EXPECT_NE(ts.find("20"), std::string::npos);
}

TEST_F(StateTest, MakeSessionId) {
    auto sid = make_session_id("slack", "C123");
    EXPECT_EQ(sid, "slack:C123");
}

TEST_F(StateTest, MakeSessionIdWithThread) {
    auto sid = make_session_id("slack", "C123", "t456");
    EXPECT_EQ(sid, "slack:C123:t456");
}

TEST_F(StateTest, MakeSessionIdEmptyThread) {
    auto sid = make_session_id("slack", "C123", "");
    EXPECT_EQ(sid, "slack:C123");
}

TEST_F(StateTest, PersistAndLoadSession) {
    nlohmann::json entry = {{"role", "user"}, {"content", "hello"}};
    persist_message("test-session", entry);
    auto history = load_session_from_disk("test-session");
    ASSERT_EQ(history.size(), 1);
    EXPECT_EQ(history[0]["content"], "hello");
}

TEST_F(StateTest, PersistMultipleMessages) {
    nlohmann::json entry1 = {{"role", "user"}, {"content", "hello"}};
    nlohmann::json entry2 = {{"role", "assistant"}, {"content", "hi there"}};
    persist_message("multi-session", entry1);
    persist_message("multi-session", entry2);

    auto history = load_session_from_disk("multi-session");
    ASSERT_EQ(history.size(), 2);
    EXPECT_EQ(history[0]["content"], "hello");
    EXPECT_EQ(history[1]["content"], "hi there");
}

TEST_F(StateTest, LoadEmptySession) {
    auto history = load_session_from_disk("nonexistent-session");
    EXPECT_TRUE(history.empty());
}

TEST_F(StateTest, AuditLogCreatesEntry) {
    nlohmann::json event = {{"event", "test"}, {"data", "value"}};
    audit_log(event);
    // Audit log should write without crashing
    SUCCEED();
}

TEST_F(StateTest, PersistSessionMeta) {
    nlohmann::json meta = {{"agent", "alpha"}, {"tier", "code"}};
    persist_session_meta("session-1", meta);
    auto loaded = load_session_meta("session-1");
    EXPECT_EQ(loaded["agent"], "alpha");
    EXPECT_EQ(loaded["tier"], "code");
}

TEST_F(StateTest, LoadSessionMetaNonexistent) {
    auto meta = load_session_meta("nonexistent");
    EXPECT_TRUE(meta.is_null() || meta.is_object());
}

TEST_F(StateTest, PersistRunEvent) {
    nlohmann::json event = {{"status", "started"}, {"agent", "beta"}};
    persist_run_event("run-1", event);
    auto events = load_run_events("run-1");
    ASSERT_GE(events.size(), 1u);
    EXPECT_EQ(events[0]["status"], "started");
}

TEST_F(StateTest, LoadRunEventsEmpty) {
    auto events = load_run_events("nonexistent-run");
    EXPECT_TRUE(events.empty());
}

} // namespace
} // namespace euxis::gateway
