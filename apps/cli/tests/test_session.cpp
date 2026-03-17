#include <gtest/gtest.h>
#include "euxis/cli/session.hpp"

#include <filesystem>

namespace euxis::cli {
namespace {

class SessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_home_ = "/tmp/euxis_test_session_" + std::to_string(getpid());
        std::filesystem::create_directories(test_home_);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_home_);
    }

    std::string test_home_;
};

TEST_F(SessionTest, ProjectName) {
    Session s(test_home_);
    auto name = s.project_name();
    EXPECT_FALSE(name.empty());
}

TEST_F(SessionTest, SessionId) {
    Session s(test_home_);
    auto id = s.session_id();
    EXPECT_FALSE(id.empty());
    // Should be numeric (timestamp)
    EXPECT_TRUE(std::all_of(id.begin(), id.end(), ::isdigit));
}

TEST_F(SessionTest, EnsureProjectDirs) {
    Session s(test_home_);
    auto dir = s.ensure_project_dirs("test-agent");
    EXPECT_TRUE(std::filesystem::is_directory(dir));
    EXPECT_TRUE(std::filesystem::exists(dir + "/audit.md"));
    EXPECT_TRUE(std::filesystem::exists(dir + "/memory.md"));
    EXPECT_TRUE(std::filesystem::is_directory(dir + "/output"));
}

TEST_F(SessionTest, GetMemoryContextEmpty) {
    Session s(test_home_);
    auto ctx = s.get_memory_context("nonexistent-agent");
    EXPECT_TRUE(ctx.empty());
}

TEST_F(SessionTest, GetMemoryContextAfterCreate) {
    Session s(test_home_);
    [[maybe_unused]] auto dir2 = s.ensure_project_dirs("test-agent");
    auto ctx = s.get_memory_context("test-agent");
    EXPECT_FALSE(ctx.empty());
    EXPECT_NE(ctx.find("Memory"), std::string::npos);
}

} // namespace
} // namespace euxis::cli
