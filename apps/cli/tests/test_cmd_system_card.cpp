#include <gtest/gtest.h>
#include "euxis/cli/cmd/system_card.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;

class SystemCardTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx_.euxis_home = "/tmp/euxis_test_syscard_" + std::to_string(getpid());
        ctx_.data_dir = ctx_.euxis_home + "/data";
        fs::create_directories(ctx_.data_dir + "/config");
    }
    void TearDown() override {
        fs::remove_all(ctx_.euxis_home);
    }
    Context ctx_;
};

TEST_F(SystemCardTest, ExitsZero) {
    auto code = cmd_system_card(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(SystemCardTest, JsonOutputValid) {
    ctx_.json_output = true;
    std::stringstream buffer;
    auto* old = std::cout.rdbuf(buffer.rdbuf());
    auto code = cmd_system_card(ctx_, {});
    std::cout.rdbuf(old);

    EXPECT_EQ(code, 0);
    auto parsed = nlohmann::json::parse(buffer.str(), nullptr, false);
    ASSERT_FALSE(parsed.is_discarded());
    EXPECT_TRUE(parsed.contains("identity"));
    EXPECT_TRUE(parsed.contains("capabilities"));
}

TEST_F(SystemCardTest, VersionMatches) {
    ctx_.json_output = true;
    std::stringstream buffer;
    auto* old = std::cout.rdbuf(buffer.rdbuf());
    cmd_system_card(ctx_, {});
    std::cout.rdbuf(old);

    auto parsed = nlohmann::json::parse(buffer.str());
    EXPECT_EQ(parsed["identity"]["version"], "0.0.10");
}

TEST_F(SystemCardTest, ProviderListPopulated) {
    ctx_.json_output = true;
    std::stringstream buffer;
    auto* old = std::cout.rdbuf(buffer.rdbuf());
    cmd_system_card(ctx_, {});
    std::cout.rdbuf(old);

    auto parsed = nlohmann::json::parse(buffer.str());
    EXPECT_TRUE(parsed.contains("providers"));
    EXPECT_GE(parsed["providers"].size(), 3u);
}

} // namespace
} // namespace euxis::cli::cmd
