#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include <sodium.h>

#include "euxis/inference/model_registry.hpp"

namespace euxis::inference {
namespace {

class ModelRegistryTest : public ::testing::Test {
protected:
    std::filesystem::path temp_dir_;

    void SetUp() override {
        ASSERT_GE(sodium_init(), 0)
            << "Failed to initialise libsodium";

        temp_dir_ = std::filesystem::temp_directory_path() /
                    ("euxis_model_registry_test_" +
                     std::to_string(std::hash<std::thread::id>{}(
                         std::this_thread::get_id())) +
                     "_" +
                     std::to_string(
                         std::chrono::steady_clock::now()
                             .time_since_epoch()
                             .count()));
        std::filesystem::create_directories(temp_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(temp_dir_);
    }

    /// Write a fake GGUF file with known content.
    void write_fake_gguf(const std::string& name,
                         const std::string& content) {
        auto path = temp_dir_ / (name + ".gguf");
        std::ofstream out(path, std::ios::binary);
        out << content;
    }
};

// ---------------------------------------------------------------------------
// Discover finds gguf files
// ---------------------------------------------------------------------------
TEST_F(ModelRegistryTest, DiscoverFindsGgufFiles) {
    write_fake_gguf("model-a", "fake gguf data A");
    write_fake_gguf("model-b", "fake gguf data B");

    // Non-gguf file should be ignored
    {
        std::ofstream out(temp_dir_ / "readme.txt");
        out << "not a model";
    }

    ModelRegistry reg(temp_dir_);
    auto models = reg.discover();

    ASSERT_EQ(models.size(), 2u);

    // Collect names
    std::set<std::string> names;
    for (const auto& m : models) {
        names.insert(m.name);
        EXPECT_FALSE(m.sha256.empty());
        EXPECT_GT(m.file_size, 0u);
        EXPECT_FALSE(m.verified);
    }
    EXPECT_TRUE(names.count("model-a"));
    EXPECT_TRUE(names.count("model-b"));
}

// ---------------------------------------------------------------------------
// Discover in empty dir returns empty
// ---------------------------------------------------------------------------
TEST_F(ModelRegistryTest, DiscoverEmptyDir) {
    ModelRegistry reg(temp_dir_);
    auto models = reg.discover();
    EXPECT_TRUE(models.empty());
}

// ---------------------------------------------------------------------------
// Discover in nonexistent dir returns empty
// ---------------------------------------------------------------------------
TEST_F(ModelRegistryTest, DiscoverNonexistentDir) {
    ModelRegistry reg(temp_dir_ / "no_such_dir");
    auto models = reg.discover();
    EXPECT_TRUE(models.empty());
}

// ---------------------------------------------------------------------------
// Verify succeeds for unchanged file
// ---------------------------------------------------------------------------
TEST_F(ModelRegistryTest, VerifyChecksum) {
    write_fake_gguf("test-model", "deterministic content for hashing");

    ModelRegistry reg(temp_dir_);
    auto models = reg.discover();
    ASSERT_EQ(models.size(), 1u);

    // Verification against same file should succeed
    EXPECT_TRUE(reg.verify(models[0]));
}

// ---------------------------------------------------------------------------
// Verify fails for modified file
// ---------------------------------------------------------------------------
TEST_F(ModelRegistryTest, VerifyFailsAfterModification) {
    write_fake_gguf("mutable", "original content");

    ModelRegistry reg(temp_dir_);
    auto models = reg.discover();
    ASSERT_EQ(models.size(), 1u);

    auto info = models[0];

    // Modify the file content
    {
        std::ofstream out(info.path, std::ios::binary);
        out << "tampered content";
    }

    // Create a fake ModelInfo with the old hash
    EXPECT_FALSE(reg.verify(info));
}

// ---------------------------------------------------------------------------
// Find by name
// ---------------------------------------------------------------------------
TEST_F(ModelRegistryTest, FindByName) {
    write_fake_gguf("alpha", "AAA");
    write_fake_gguf("beta", "BBB");

    ModelRegistry reg(temp_dir_);
    (void)reg.discover();

    auto found = reg.find("alpha");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, "alpha");

    auto missing = reg.find("gamma");
    EXPECT_FALSE(missing.has_value());
}

} // anonymous namespace
} // namespace euxis::inference
