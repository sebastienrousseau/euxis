#include "euxis/gateway/state.hpp"

#include <gtest/gtest.h>
#include <filesystem>

namespace euxis::gateway {
namespace {

class StatePersistenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmp_ = std::filesystem::temp_directory_path() / "euxis_state_test_full";
        std::filesystem::remove_all(tmp_);
        old_home_ = std::getenv("EUXIS_HOME");
        setenv("EUXIS_HOME", tmp_.c_str(), 1);
    }

    void TearDown() override {
        if (old_home_ != nullptr) {
            setenv("EUXIS_HOME", old_home_, 1);
        } else {
            unsetenv("EUXIS_HOME");
        }
        std::filesystem::remove_all(tmp_);
    }

    std::filesystem::path tmp_;
    const char* old_home_{nullptr};
};

TEST_F(StatePersistenceTest, TimestampFormat) {
    auto ts = timestamp();
    EXPECT_FALSE(ts.empty());
    // Should contain date-like pattern
    EXPECT_NE(ts.find('-'), std::string::npos);
    EXPECT_NE(ts.find('T'), std::string::npos);
}

TEST_F(StatePersistenceTest, MakeSessionIdNoThread) {
    EXPECT_EQ(make_session_id("ch", "chat"), "ch:chat");
}

TEST_F(StatePersistenceTest, MakeSessionIdWithThread) {
    EXPECT_EQ(make_session_id("ch", "chat", "th"), "ch:chat:th");
}

TEST_F(StatePersistenceTest, MakeSessionIdEmptyThread) {
    EXPECT_EQ(make_session_id("ch", "chat", ""), "ch:chat");
}

TEST_F(StatePersistenceTest, PersistAndLoadMessages) {
    nlohmann::json msg1 = {{"role", "user"}, {"content", "hello"}};
    nlohmann::json msg2 = {{"role", "assistant"}, {"content", "hi"}};

    persist_message("test-s1", msg1);
    persist_message("test-s1", msg2);

    auto history = load_session_from_disk("test-s1");
    ASSERT_EQ(history.size(), 2);
    EXPECT_EQ(history[0]["role"], "user");
    EXPECT_EQ(history[1]["role"], "assistant");
}

TEST_F(StatePersistenceTest, LoadNonexistentSessionReturnsEmpty) {
    auto history = load_session_from_disk("nonexistent-session");
    EXPECT_TRUE(history.empty());
}

TEST_F(StatePersistenceTest, PersistAndLoadSessionMeta) {
    nlohmann::json meta = {{"agent", "code-agent"}, {"model", "claude-3.5-sonnet"}};
    persist_session_meta("meta-s1", meta);

    auto loaded = load_session_meta("meta-s1");
    EXPECT_EQ(loaded["agent"], "code-agent");
    EXPECT_EQ(loaded["model"], "claude-3.5-sonnet");
}

TEST_F(StatePersistenceTest, LoadNonexistentMetaReturnsEmpty) {
    auto meta = load_session_meta("nonexistent-meta");
    EXPECT_TRUE(meta.empty());
}

TEST_F(StatePersistenceTest, AuditLogWritesEntry) {
    nlohmann::json event = {{"event", "test"}, {"data", "value"}};
    audit_log(event);

    auto path = audit_dir() / "gateway_audit.jsonl";
    EXPECT_TRUE(std::filesystem::exists(path));
}

TEST_F(StatePersistenceTest, PersistAndLoadRunEvents) {
    nlohmann::json ev1 = {{"step", 1}, {"status", "running"}};
    nlohmann::json ev2 = {{"step", 2}, {"status", "done"}};

    persist_run_event("run-1", ev1);
    persist_run_event("run-1", ev2);

    auto events = load_run_events("run-1");
    ASSERT_EQ(events.size(), 2);
    EXPECT_EQ(events[0]["step"], 1);
    EXPECT_EQ(events[1]["status"], "done");
}

TEST_F(StatePersistenceTest, LoadNonexistentRunReturnsEmpty) {
    auto events = load_run_events("nonexistent-run");
    EXPECT_TRUE(events.empty());
}

TEST_F(StatePersistenceTest, DirectoryCreation) {
    auto gd = gateway_data_dir();
    EXPECT_TRUE(std::filesystem::exists(gd));

    auto sd = sessions_dir();
    EXPECT_TRUE(std::filesystem::exists(sd));

    auto rd = runs_dir();
    EXPECT_TRUE(std::filesystem::exists(rd));

    auto ad = approvals_dir();
    EXPECT_TRUE(std::filesystem::exists(ad));

    auto aud = audit_dir();
    EXPECT_TRUE(std::filesystem::exists(aud));
}

// --- Coverage: lines 14-15 (euxis_home when EUXIS_HOME not set, HOME fallback) ---
TEST_F(StatePersistenceTest, EuxisHomeWithoutEnvVar) {
    unsetenv("EUXIS_HOME");
    // gateway_data_dir() will use HOME env or /tmp/.euxis as fallback
    auto dir = gateway_data_dir();
    EXPECT_TRUE(std::filesystem::exists(dir));
}

// --- Coverage: lines 24, 30, 36, 42, 48 (directory creation functions) ---
TEST_F(StatePersistenceTest, AllDirsCreatedCorrectly) {
    auto gd = gateway_data_dir();
    auto sd = sessions_dir();
    auto rd = runs_dir();
    auto ad = approvals_dir();
    auto aud = audit_dir();

    EXPECT_TRUE(std::filesystem::is_directory(gd));
    EXPECT_TRUE(std::filesystem::is_directory(sd));
    EXPECT_TRUE(std::filesystem::is_directory(rd));
    EXPECT_TRUE(std::filesystem::is_directory(ad));
    EXPECT_TRUE(std::filesystem::is_directory(aud));
}

} // namespace
} // namespace euxis::gateway
