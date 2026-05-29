#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <thread>

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

// --- Coverage: line 55 (verify on nonexistent path returns false) ---
TEST_F(ModelRegistryTest, VerifyNonexistentPathReturnsFalse) {
    ModelInfo fake;
    fake.name = "ghost";
    fake.path = temp_dir_ / "nonexistent.gguf";
    fake.sha256 = "abc123";

    ModelRegistry reg(temp_dir_);
    EXPECT_FALSE(reg.verify(fake));
}

// --- Coverage: line 84 (compute_sha256 on unreadable file => empty) ---
TEST_F(ModelRegistryTest, ComputeSha256EmptyOnFailure) {
    ModelInfo fake;
    fake.name = "bad";
    fake.path = temp_dir_ / "no-such-file.gguf";
    fake.sha256 = "";

    ModelRegistry reg(temp_dir_);
    // verify should return false (hash mismatch: both empty but file missing)
    EXPECT_FALSE(reg.verify(fake));
}

// --- Coverage: lines 92-94 (find returns nullopt for undiscovered model) ---
TEST_F(ModelRegistryTest, FindWithoutDiscoverReturnsNullopt) {
    ModelRegistry reg(temp_dir_);
    auto found = reg.find("anything");
    EXPECT_FALSE(found.has_value());
}

// --- Coverage: lines 92-94 (multi-chunk SHA-256 hash loop, file > 64KiB) ---
TEST_F(ModelRegistryTest, ComputeSha256MultiChunk) {
    // Create a file larger than 64 KiB to trigger the multi-chunk while loop
    auto path = temp_dir_ / "large_model.gguf";
    {
        std::ofstream out(path, std::ios::binary);
        // Write 128 KiB of deterministic data (2 full chunks)
        std::string chunk(1024, 'A');
        for (int i = 0; i < 128; ++i) {
            out << chunk;
        }
    }

    ModelRegistry reg(temp_dir_);
    auto models = reg.discover();
    ASSERT_EQ(models.size(), 1u);
    EXPECT_EQ(models[0].name, "large_model");
    EXPECT_FALSE(models[0].sha256.empty());
    EXPECT_EQ(models[0].sha256.size(), 64u); // 32 bytes = 64 hex chars

    // Verify the hash is consistent
    EXPECT_TRUE(reg.verify(models[0]));
}

// --- Coverage: line 84 + 92-94 (discover with file exactly 64 KiB boundary) ---
TEST_F(ModelRegistryTest, ComputeSha256ExactChunkSize) {
    auto path = temp_dir_ / "exact_chunk.gguf";
    {
        std::ofstream out(path, std::ios::binary);
        // Write exactly 64 KiB (one full chunk, no partial read)
        std::string data(64 * 1024, 'B');
        out << data;
    }

    ModelRegistry reg(temp_dir_);
    auto models = reg.discover();
    ASSERT_EQ(models.size(), 1u);
    EXPECT_FALSE(models[0].sha256.empty());
    EXPECT_TRUE(reg.verify(models[0]));
}

// --- Coverage: line 97 (partial read after full chunks) ---
TEST_F(ModelRegistryTest, ComputeSha256PartialFinalRead) {
    auto path = temp_dir_ / "partial_chunk.gguf";
    {
        std::ofstream out(path, std::ios::binary);
        // Write 64 KiB + 100 bytes (triggers partial read after full chunk)
        std::string data(64 * 1024 + 100, 'C');
        out << data;
    }

    ModelRegistry reg(temp_dir_);
    auto models = reg.discover();
    ASSERT_EQ(models.size(), 1u);
    EXPECT_FALSE(models[0].sha256.empty());
    EXPECT_TRUE(reg.verify(models[0]));
}

} // anonymous namespace
} // namespace euxis::inference
