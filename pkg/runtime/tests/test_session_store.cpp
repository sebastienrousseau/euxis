#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "euxis/runtime/session_store.hpp"

namespace euxis::runtime {

auto make_session_store(const std::string& base_dir) -> std::unique_ptr<ISessionStore>;

namespace {

class SessionStoreTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_;
    std::unique_ptr<ISessionStore> store_;

    void SetUp() override {
        tmp_ = std::filesystem::temp_directory_path() / "euxis_store_test";
        std::filesystem::remove_all(tmp_);
        store_ = make_session_store(tmp_.string());
    }

    void TearDown() override {
        std::filesystem::remove_all(tmp_);
    }
};

TEST_F(SessionStoreTest, SaveAndLoadRoundtrip) {
    SessionSnapshot snap{
        .session_id = "test-1",
        .branch_id = "main",
        .agent_id = "agent-01",
        .messages = {
            {.role = Role::User, .content = "Hello",
             .agent_id = {}, .model = {}, .timestamp = {}},
            {.role = Role::Assistant, .content = "Hi there",
             .agent_id = {}, .model = "gpt-4", .timestamp = {},
             .duration_ms = 42.0},
        },
    };

    auto save_result = store_->save(snap);
    ASSERT_TRUE(save_result.has_value()) << save_result.error();

    auto load_result = store_->load("test-1", "main");
    ASSERT_TRUE(load_result.has_value()) << load_result.error();

    EXPECT_EQ(load_result->session_id, "test-1");
    EXPECT_EQ(load_result->agent_id, "agent-01");
    ASSERT_EQ(load_result->messages.size(), 2u);
    EXPECT_EQ(load_result->messages[0].content, "Hello");
    EXPECT_EQ(load_result->messages[1].content, "Hi there");
}

TEST_F(SessionStoreTest, LoadNonexistentFails) {
    auto result = store_->load("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(SessionStoreTest, ListBranches) {
    store_->save({.session_id = "multi", .branch_id = "main", .agent_id = "a", .messages = {}});
    store_->save({.session_id = "multi", .branch_id = "experiment", .agent_id = "a", .messages = {}});

    auto branches = store_->list_branches("multi");
    EXPECT_EQ(branches.size(), 2u);
}

TEST_F(SessionStoreTest, Compaction) {
    SessionSnapshot snap{
        .session_id = "compact",
        .branch_id = "main",
        .agent_id = "a",
        .messages = {},
    };
    for (int i = 0; i < 30; ++i) {
        snap.messages.push_back({.role = Role::User,
                                 .content = "msg-" + std::to_string(i),
                                 .agent_id = {}, .model = {}, .timestamp = {}});
    }
    store_->save(snap);

    auto compact_result = store_->compact("compact", 5);
    ASSERT_TRUE(compact_result.has_value());

    auto loaded = store_->load("compact");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->messages.size(), 5u);
}

// --- Coverage: line 16 (role_to_string for System role) ---
TEST_F(SessionStoreTest, SystemRoleRoundtrip) {
    SessionSnapshot snap{
        .session_id = "role-test",
        .branch_id = "main",
        .agent_id = "agent-role",
        .messages = {
            {.role = Role::System, .content = "You are helpful",
             .agent_id = {}, .model = {}, .timestamp = {}},
        },
    };
    auto save_result = store_->save(snap);
    ASSERT_TRUE(save_result.has_value()) << save_result.error();

    auto load_result = store_->load("role-test", "main");
    ASSERT_TRUE(load_result.has_value()) << load_result.error();
    EXPECT_EQ(load_result->messages[0].role, Role::System);
}

// --- Coverage: line 18 (role_to_string for unknown role => "unknown") ---
// role_from_string("junk") => User  (default)
TEST_F(SessionStoreTest, UnknownRoleDefaultsToUser) {
    // Manually write a session with a non-standard role
    auto dir = tmp_ / "custom-role";
    std::filesystem::create_directories(dir);
    auto path = dir / "main.json";
    nlohmann::json j;
    j["session_id"] = "custom-role";
    j["branch_id"] = "main";
    j["agent_id"] = "agent";
    j["messages"] = {{
        {"role", "moderator"},
        {"content", "Hello"},
        {"agent_id", ""},
        {"model", ""},
        {"timestamp", ""},
        {"duration_ms", 0.0},
    }};
    std::ofstream f(path);
    f << j.dump(2);
    f.close();

    auto load_result = store_->load("custom-role", "main");
    ASSERT_TRUE(load_result.has_value());
    EXPECT_EQ(load_result->messages[0].role, Role::User);
}

// --- Coverage: line 62 (save fails to open path) ---
// This is hard to trigger in test without mocking, but we can test
// the line 75 (load fails to open) and line 79 (parse fails)
TEST_F(SessionStoreTest, LoadCorruptedFileFails) {
    auto dir = tmp_ / "corrupt";
    std::filesystem::create_directories(dir);
    auto path = dir / "main.json";
    std::ofstream f(path);
    f << "this is not json at all";
    f.close();

    auto result = store_->load("corrupt", "main");
    EXPECT_FALSE(result.has_value());
}

// --- Coverage: line 93 (compact on nonexistent session) ---
TEST_F(SessionStoreTest, CompactNonexistentSessionFails) {
    auto result = store_->compact("nonexistent-session", 5);
    EXPECT_FALSE(result.has_value());
}

// --- Coverage: lines 79 (list_branches for nonexistent session) ---
TEST_F(SessionStoreTest, ListBranchesNonexistent) {
    auto branches = store_->list_branches("nonexistent");
    EXPECT_TRUE(branches.empty());
}

} // namespace
} // namespace euxis::runtime
