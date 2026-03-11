#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "euxis/runtime/session_store.hpp"

namespace euxis::runtime {

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
            {.role = Role::User, .content = "Hello"},
            {.role = Role::Assistant, .content = "Hi there", .model = "gpt-4", .duration_ms = 42.0},
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
    SessionSnapshot snap{.session_id = "compact", .branch_id = "main", .agent_id = "a"};
    for (int i = 0; i < 30; ++i) {
        snap.messages.push_back({.role = Role::User, .content = "msg-" + std::to_string(i)});
    }
    store_->save(snap);

    auto compact_result = store_->compact("compact", 5);
    ASSERT_TRUE(compact_result.has_value());

    auto loaded = store_->load("compact");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->messages.size(), 5u);
}

class MemorySessionStoreTest : public ::testing::Test {
protected:
    std::unique_ptr<ISessionStore> store_;
    void SetUp() override { store_ = make_memory_session_store(); }
};

TEST_F(MemorySessionStoreTest, MemoryRoundtrip) {
    SessionSnapshot snap{.session_id = "mem-1", .branch_id = "main", .agent_id = "a"};
    snap.messages.push_back({.role = Role::User, .content = "In-memory test"});
    
    ASSERT_TRUE(store_->save(snap).has_value());
    auto loaded = store_->load("mem-1");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->messages[0].content, "In-memory test");
    
    auto branches = store_->list_branches("mem-1");
    EXPECT_EQ(branches.size(), 1u);
}

TEST_F(MemorySessionStoreTest, MemoryCompaction) {
    SessionSnapshot snap{.session_id = "mem-compact", .branch_id = "main"};
    for (int i = 0; i < 10; ++i) snap.messages.push_back({.role = Role::User, .content = "msg"});
    store_->save(snap);
    
    ASSERT_TRUE(store_->compact("mem-compact", 3).has_value());
    auto loaded = store_->load("mem-compact");
    EXPECT_EQ(loaded->messages.size(), 3u);
}

TEST_F(SessionStoreTest, UnknownRoleDefaultsToUser) {
    auto dir = tmp_ / "custom-role";
    std::filesystem::create_directories(dir);
    auto path = dir / "main.msgp";
    
    nlohmann::json j;
    j["session_id"] = "custom-role";
    j["branch_id"] = "main";
    j["messages"] = {{{"role", "moderator"}, {"content", "Hello"}}};
    
    std::vector<uint8_t> packed = nlohmann::json::to_msgpack(j);
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(packed.data()), packed.size());
    f.close();

    auto load_result = store_->load("custom-role", "main");
    ASSERT_TRUE(load_result.has_value());
    EXPECT_EQ(load_result->messages[0].role, Role::User);
}

TEST_F(SessionStoreTest, LoadCorruptedFileFails) {
    auto dir = tmp_ / "corrupt";
    std::filesystem::create_directories(dir);
    auto path = dir / "main.msgp";
    std::ofstream f(path, std::ios::binary);
    f << "this is not messagepack at all";
    f.close();

    auto result = store_->load("corrupt", "main");
    EXPECT_FALSE(result.has_value());
}

} // namespace
} // namespace euxis::runtime
