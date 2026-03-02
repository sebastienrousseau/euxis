#include <gtest/gtest.h>
#include <filesystem>
#include <array>
#include <cstring>

#include "euxis/crypto/aes_gcm.hpp"
#include "euxis/crypto/key_derivation.hpp"
#include "euxis/memory/store.hpp"

namespace euxis::integration {
namespace {

class CryptoMemoryRoundtripTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_;
    std::array<std::byte, 32> master_key_{};

    void SetUp() override {
        tmp_ = std::filesystem::temp_directory_path() / "euxis_integ_crypto_mem";
        std::filesystem::remove_all(tmp_);
        std::filesystem::create_directories(tmp_);
        // Fill master key with deterministic bytes for testing
        for (size_t i = 0; i < 32; ++i)
            master_key_[i] = static_cast<std::byte>(i ^ 0xAB);
    }

    void TearDown() override {
        std::filesystem::remove_all(tmp_);
    }
};

TEST_F(CryptoMemoryRoundtripTest, EncryptStoreRetrieveDecrypt) {
    memory::EncryptedMemoryStore store(tmp_, master_key_, "did:euxis:test-agent-01");

    const std::string original = "Sensitive agent memory content — plans for Q2 2026.";
    auto entry_result = store.store(original, memory::MemoryTier::Hot);
    ASSERT_TRUE(entry_result.has_value()) << entry_result.error();
    auto entry_id = entry_result->entry_id;
    EXPECT_FALSE(entry_id.empty());

    auto retrieved = store.retrieve(entry_id);
    ASSERT_TRUE(retrieved.has_value()) << retrieved.error();
    EXPECT_EQ(*retrieved, original);
}

TEST_F(CryptoMemoryRoundtripTest, MultipleTiersRoundtrip) {
    memory::EncryptedMemoryStore store(tmp_, master_key_, "did:euxis:multi-tier");

    auto hot = store.store("hot data", memory::MemoryTier::Hot);
    ASSERT_TRUE(hot.has_value());
    auto relevant = store.store("relevant data", memory::MemoryTier::Relevant);
    ASSERT_TRUE(relevant.has_value());
    auto cross = store.store("cross-agent data", memory::MemoryTier::CrossAgent);
    ASSERT_TRUE(cross.has_value());

    EXPECT_EQ(*store.retrieve(hot->entry_id), "hot data");
    EXPECT_EQ(*store.retrieve(relevant->entry_id), "relevant data");
    EXPECT_EQ(*store.retrieve(cross->entry_id), "cross-agent data");
}

TEST_F(CryptoMemoryRoundtripTest, RetrieveTierReturnsCorrectEntries) {
    memory::EncryptedMemoryStore store(tmp_, master_key_, "did:euxis:tier-filter");

    (void)store.store("hot-1", memory::MemoryTier::Hot);
    (void)store.store("hot-2", memory::MemoryTier::Hot);
    (void)store.store("relevant-1", memory::MemoryTier::Relevant);

    auto hot_entries = store.retrieve_tier(memory::MemoryTier::Hot);
    EXPECT_EQ(hot_entries.size(), 2u);
    auto relevant_entries = store.retrieve_tier(memory::MemoryTier::Relevant);
    EXPECT_EQ(relevant_entries.size(), 1u);
}

TEST_F(CryptoMemoryRoundtripTest, DestroyAgentKeysInvalidatesRetrieval) {
    memory::EncryptedMemoryStore store(tmp_, master_key_, "did:euxis:destroy-test");

    auto entry = store.store("secret data", memory::MemoryTier::Hot);
    ASSERT_TRUE(entry.has_value());

    store.destroy_agent_keys();

    // After key destruction, retrieval should fail
    auto result = store.retrieve(entry->entry_id);
    EXPECT_FALSE(result.has_value());
}

} // namespace
} // namespace euxis::integration
