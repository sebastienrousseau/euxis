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
            {.role = Role::User, .content = "Hello", .agent_id = {}, .model = {}, .timestamp = {}, .duration_ms = 0.0, .decision_trace_hash = {}},
            {.role = Role::System, .content = "System init", .agent_id = {}, .model = {}, .timestamp = {}, .duration_ms = 0.0, .decision_trace_hash = {}},
            {.role = Role::Assistant, .content = "Hi there", .agent_id = {}, .model = "gpt-4", .timestamp = {}, .duration_ms = 42.0, .decision_trace_hash = {}},
        },
    };

    ASSERT_TRUE(store_->save(snap).has_value());
    auto load_result = store_->load("test-1", "main");
    ASSERT_TRUE(load_result.has_value());

    EXPECT_EQ(load_result->messages[0].role, Role::User);
    EXPECT_EQ(load_result->messages[1].role, Role::System);
    EXPECT_EQ(load_result->messages[2].role, Role::Assistant);
}

TEST_F(SessionStoreTest, UnknownRoleDefaultsToUser) {
    auto dir = tmp_ / "custom-role";
    std::filesystem::create_directories(dir);
    auto path = dir / "main.msgp";
    
    nlohmann::json j;
    j["session_id"] = "custom-role";
    j["branch_id"] = "main";
    j["messages"] = {{{"role", "unknown_role_string"}, {"content", "Hello"}}};
    
    auto packed = nlohmann::json::to_msgpack(j);
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(packed.data()), static_cast<std::streamsize>(packed.size()));
    f.close();

    auto load_result = store_->load("custom-role", "main");
    ASSERT_TRUE(load_result.has_value());
    EXPECT_EQ(load_result->messages[0].role, Role::User);
}

TEST_F(SessionStoreTest, FileOpenFailures) {
    EXPECT_FALSE(store_->load("nonexistent", "main").has_value());
}

TEST_F(SessionStoreTest, ListBranchesFiltering) {
    auto dir = tmp_ / "filter-test";
    std::filesystem::create_directories(dir);
    
    // Create valid and invalid files
    std::ofstream(dir / "main.msgp") << "data";
    std::ofstream(dir / "experiment.msgp") << "data";
    std::ofstream(dir / "notes.txt") << "text";
    std::ofstream(dir / "backup.msgp.bak") << "backup";
    
    auto branches = store_->list_branches("filter-test");
    EXPECT_EQ(branches.size(), 2u);
    EXPECT_TRUE(std::ranges::find(branches, "main") != branches.end());
    EXPECT_TRUE(std::ranges::find(branches, "experiment") != branches.end());
    EXPECT_TRUE(std::ranges::find(branches, "notes") == branches.end());
}

TEST_F(SessionStoreTest, CorruptedMessagePackDetailed) {
    auto dir = tmp_ / "corrupt-detail";
    std::filesystem::create_directories(dir);
    auto path = dir / "main.msgp";
    
    // Invalid msgpack start byte
    std::ofstream f(path, std::ios::binary);
    uint8_t invalid_byte = 0xc1; // Reserved in msgpack
    f.write(reinterpret_cast<const char*>(&invalid_byte), 1);
    f.close();

    auto result = store_->load("corrupt-detail", "main");
    EXPECT_FALSE(result.has_value());
    if (!result) {
        EXPECT_EQ(result.error(), "MessagePack decoding failed");
    }
}

TEST_F(SessionStoreTest, ListBranchesEmpty) {
    EXPECT_TRUE(store_->list_branches("nothing").empty());
}

TEST_F(SessionStoreTest, CompactEmpty) {
    EXPECT_FALSE(store_->compact("nothing", 5).has_value());
}

class MemorySessionStoreTest : public ::testing::Test {
protected:
    std::unique_ptr<ISessionStore> store_;
    void SetUp() override { store_ = make_memory_session_store(); }
};

TEST_F(MemorySessionStoreTest, AllPaths) {
    SessionSnapshot snap{.session_id = "s1", .branch_id = "b1", .agent_id = {}, .messages = {}};
    ASSERT_TRUE(store_->save(snap).has_value());
    
    EXPECT_TRUE(store_->load("s1", "b1").has_value());
    EXPECT_FALSE(store_->load("s1", "b2").has_value());
    EXPECT_FALSE(store_->load("s2", "b1").has_value());
    
    EXPECT_EQ(store_->list_branches("s1").size(), 1u);
    EXPECT_EQ(store_->list_branches("s2").size(), 0u);
    
    EXPECT_FALSE(store_->compact("s2", 5).has_value());
}

TEST_F(MemorySessionStoreTest, EpisodicStreaming) {
    SessionSnapshot snap{.session_id = "stream-test", .branch_id = "main", .agent_id = {}, .messages = {}};
    snap.messages.push_back({.role = Role::User, .content = "ep1", .agent_id = {}, .model = {}, .timestamp = {}, .duration_ms = 0.0, .decision_trace_hash = {}});
    snap.messages.push_back({.role = Role::Assistant, .content = "ep2", .agent_id = {}, .model = {}, .timestamp = {}, .duration_ms = 0.0, .decision_trace_hash = {}});
    ASSERT_TRUE(store_->save(snap).has_value());
    
    int count = 0;
    for (auto msg : store_->stream_episodes("stream-test")) {
        count++;
        if (count == 1) { EXPECT_EQ(msg.content, "ep1"); }
        if (count == 2) { EXPECT_EQ(msg.content, "ep2"); }
    }
    EXPECT_EQ(count, 2);
    
    int empty_count = 0;
    for (auto _ [[maybe_unused]] : store_->stream_episodes("nonexistent")) {
        empty_count++;
    }
    EXPECT_EQ(empty_count, 0);
}

} // namespace
} // namespace euxis::runtime
