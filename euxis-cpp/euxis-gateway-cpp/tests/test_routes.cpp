#include <gtest/gtest.h>
#include <filesystem>

#include "euxis/gateway/state.hpp"

namespace euxis::gateway {
namespace {

TEST(StateTest, TimestampNotEmpty) {
    auto ts = timestamp();
    EXPECT_FALSE(ts.empty());
}

TEST(StateTest, MakeSessionId) {
    auto sid = make_session_id("slack", "C123");
    EXPECT_EQ(sid, "slack:C123");
}

TEST(StateTest, MakeSessionIdWithThread) {
    auto sid = make_session_id("slack", "C123", "t456");
    EXPECT_EQ(sid, "slack:C123:t456");
}

TEST(StateTest, PersistAndLoadSession) {
    auto tmp = std::filesystem::temp_directory_path() / "euxis_state_test";
    std::filesystem::remove_all(tmp);

    // Set EUXIS_HOME for this test
    auto old = std::getenv("EUXIS_HOME");
    setenv("EUXIS_HOME", tmp.c_str(), 1);

    nlohmann::json entry = {{"role", "user"}, {"content", "hello"}};
    persist_message("test-session", entry);
    auto history = load_session_from_disk("test-session");
    ASSERT_EQ(history.size(), 1);
    EXPECT_EQ(history[0]["content"], "hello");

    // Restore
    if (old) setenv("EUXIS_HOME", old, 1);
    else unsetenv("EUXIS_HOME");
    std::filesystem::remove_all(tmp);
}

} // namespace
} // namespace euxis::gateway
