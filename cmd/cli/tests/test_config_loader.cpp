#include <gtest/gtest.h>
#include "euxis/cli/config_loader.hpp"

#include <filesystem>
#include <fstream>

namespace euxis::cli {
namespace {

class ConfigLoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/euxis_test_config_" + std::to_string(getpid());
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
};

TEST_F(ConfigLoaderTest, LoadMissing) {
    ConfigLoader loader(test_dir_);
    auto result = loader.load("nonexistent.json");
    EXPECT_FALSE(result.has_value());
}

TEST_F(ConfigLoaderTest, LoadValid) {
    auto path = std::filesystem::path(test_dir_) / "test.json";
    std::ofstream(path) << R"({"key": "value"})";

    ConfigLoader loader(test_dir_);
    auto result = loader.load("test.json");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)["key"], "value");
}

TEST_F(ConfigLoaderTest, LoadInvalidJson) {
    auto path = std::filesystem::path(test_dir_) / "bad.json";
    std::ofstream(path) << "not json {{{";

    ConfigLoader loader(test_dir_);
    auto result = loader.load("bad.json");
    EXPECT_FALSE(result.has_value());
}

TEST_F(ConfigLoaderTest, LoadOr) {
    ConfigLoader loader(test_dir_);
    nlohmann::json fallback = {{"default", true}};
    auto result = loader.load_or("missing.json", fallback);
    EXPECT_TRUE(result["default"].get<bool>());
}

TEST_F(ConfigLoaderTest, Exists) {
    std::ofstream(std::filesystem::path(test_dir_) / "exists.json") << "{}";
    ConfigLoader loader(test_dir_);
    EXPECT_TRUE(loader.exists("exists.json"));
    EXPECT_FALSE(loader.exists("nope.json"));
}

TEST_F(ConfigLoaderTest, DataDir) {
    ConfigLoader loader(test_dir_);
    EXPECT_EQ(loader.data_dir(), test_dir_);
}

} // namespace
} // namespace euxis::cli
