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
            {.role = Role::System, .content = "System init"},
            {.role = Role::Assistant, .content = "Hi there", .model = "gpt-4", .duration_ms = 42.0},
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
    f.write(reinterpret_cast<const char*>(packed.data()), packed.size());
    f.close();

    auto load_result = store_->load("custom-role", "main");
    ASSERT_TRUE(load_result.has_value());
    EXPECT_EQ(load_result->messages[0].role, Role::User);
}

TEST_F(SessionStoreTest, FileOpenFailures) {
    // Attempt to load from a non-existent file
    EXPECT_FALSE(store_->load("nonexistent", "main").has_value());

    // Force a save failure by creating a directory where the file should be
    auto dir = tmp_ / "fail-save" / "main.msgp";
    std::filesystem::create_directories(dir);
    
    SessionSnapshot snap{.session_id = "fail-save", .branch_id = "main"};
    EXPECT_FALSE(store_->save(snap).has_value());
    
    // Force a load failure by pointing load to that directory
    EXPECT_FALSE(store_->load("fail-save", "main").has_value());
}

TEST_F(SessionStoreTest, CorruptedMessagePack) {
    auto dir = tmp_ / "corrupt";
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "main.msgp", std::ios::binary) << "not msgpack";
    
    EXPECT_FALSE(store_->load("corrupt", "main").has_value());
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
    SessionSnapshot snap{.session_id = "s1", .branch_id = "b1"};
    store_->save(snap);
    
    EXPECT_TRUE(store_->load("s1", "b1").has_value());
    EXPECT_FALSE(store_->load("s1", "b2").has_value());
    EXPECT_FALSE(store_->load("s2", "b1").has_value());
    
    EXPECT_EQ(store_->list_branches("s1").size(), 1u);
    EXPECT_EQ(store_->list_branches("s2").size(), 0u);
    
    EXPECT_FALSE(store_->compact("s2", 5).has_value());
}

} // namespace
} // namespace euxis::runtime
