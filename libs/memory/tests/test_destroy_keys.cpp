#include <gtest/gtest.h>
#include <sodium.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <string>

#include "euxis/memory/store.hpp"
#include "euxis/memory/tier.hpp"

// NOLINTBEGIN(bugprone-unused-return-value) — gtest-style discards on
// EXPECT_*/ASSERT_* helpers are intentional; tests can blanket-disable
// per docs/development/clang-tidy-policy.md.

namespace euxis::memory {
namespace {

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class DestroyKeysTest : public ::testing::Test {
protected:
    std::filesystem::path test_dir_;

    void SetUp() override {
        ASSERT_GE(sodium_init(), 0) << "sodium_init() failed";

        std::array<std::byte, 8> rand_bytes{};
        randombytes_buf(rand_bytes.data(), rand_bytes.size());
        std::string suffix;
        suffix.resize(17, '\0');
        sodium_bin2hex(suffix.data(), suffix.size(),
                       reinterpret_cast<const unsigned char*>(rand_bytes.data()),
                       rand_bytes.size());
        suffix.resize(16);

        test_dir_ = std::filesystem::temp_directory_path() /
                    ("euxis_test_destroy_" + suffix);
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    static auto random_master_key() -> std::array<std::byte, 32> {
        std::array<std::byte, 32> key{};
        randombytes_buf(key.data(), key.size());
        return key;
    }
};

// ---------------------------------------------------------------------------
// After destroy_agent_keys(), agent_key_ is zeroed
// ---------------------------------------------------------------------------
TEST_F(DestroyKeysTest, AgentKeyIsZeroedAfterDestroy) {
    auto master = random_master_key();
    EncryptedMemoryStore store(test_dir_, master, "did:euxis:agent:destroy");

    // Store something first to prove the key was valid.
    auto result = store.store("data before destroy", MemoryTier::Hot);
    ASSERT_TRUE(result.has_value()) << result.error();

    // Destroy keys.
    store.destroy_agent_keys();

    // Verify the key is zeroed by checking that the store reports failure.
    // The agent_key_ member is private, but we can infer it is zeroed because:
    //   1. sodium_memzero is called on it
    //   2. subsequent operations fail with "keys destroyed" error
    // This is the intended public API verification pattern.

    auto store_result = store.store("data after destroy", MemoryTier::Hot);
    ASSERT_FALSE(store_result.has_value());
    EXPECT_TRUE(store_result.error().find("destroyed") != std::string::npos);
}

// ---------------------------------------------------------------------------
// After destroy, store() returns error
// ---------------------------------------------------------------------------
TEST_F(DestroyKeysTest, StoreFailsAfterDestroy) {
    auto master = random_master_key();
    EncryptedMemoryStore store(test_dir_, master, "did:euxis:agent:no-store");

    store.destroy_agent_keys();

    auto result = store.store("should fail", MemoryTier::Hot);
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("destroyed") != std::string::npos);
}

// ---------------------------------------------------------------------------
// After destroy, retrieve() returns error
// ---------------------------------------------------------------------------
TEST_F(DestroyKeysTest, RetrieveFailsAfterDestroy) {
    auto master = random_master_key();
    EncryptedMemoryStore store(test_dir_, master, "did:euxis:agent:no-retrieve");

    // Store an entry first.
    auto result = store.store("retrievable data", MemoryTier::Hot);
    ASSERT_TRUE(result.has_value()) << result.error();
    auto entry_id = result->entry_id;

    // Destroy keys.
    store.destroy_agent_keys();

    // Retrieve should fail.
    auto retrieved = store.retrieve(entry_id);
    ASSERT_FALSE(retrieved.has_value());
    EXPECT_TRUE(retrieved.error().find("destroyed") != std::string::npos);
}

// ---------------------------------------------------------------------------
// After destroy, retrieve_tier() returns empty
// ---------------------------------------------------------------------------
TEST_F(DestroyKeysTest, RetrieveTierReturnsEmptyAfterDestroy) {
    auto master = random_master_key();
    EncryptedMemoryStore store(test_dir_, master, "did:euxis:agent:no-tier");

    (void)store.store("hot memory", MemoryTier::Hot);
    (void)store.store("relevant memory", MemoryTier::Relevant);

    store.destroy_agent_keys();

    auto hot = store.retrieve_tier(MemoryTier::Hot);
    EXPECT_TRUE(hot.empty());

    auto relevant = store.retrieve_tier(MemoryTier::Relevant);
    EXPECT_TRUE(relevant.empty());
}

// ---------------------------------------------------------------------------
// export_tier_encrypted still works after destroy (returns encrypted data)
// ---------------------------------------------------------------------------
TEST_F(DestroyKeysTest, ExportStillWorksAfterDestroy) {
    auto master = random_master_key();
    EncryptedMemoryStore store(test_dir_, master, "did:euxis:agent:export-ok");

    (void)store.store("export data", MemoryTier::Hot);

    store.destroy_agent_keys();

    // export_tier_encrypted only reads the JSONL file; it does NOT decrypt.
    auto entries = store.export_tier_encrypted(MemoryTier::Hot);
    EXPECT_EQ(entries.size(), 1u);
}

// ---------------------------------------------------------------------------
// Double destroy is safe (idempotent)
// ---------------------------------------------------------------------------
TEST_F(DestroyKeysTest, DoubleDestroyIsSafe) {
    auto master = random_master_key();
    EncryptedMemoryStore store(test_dir_, master, "did:euxis:agent:double");

    store.destroy_agent_keys();
    store.destroy_agent_keys(); // Should not crash or throw.

    auto result = store.store("after double destroy", MemoryTier::Hot);
    EXPECT_FALSE(result.has_value());
}

} // namespace
} // namespace euxis::memory

// NOLINTEND(bugprone-unused-return-value)
