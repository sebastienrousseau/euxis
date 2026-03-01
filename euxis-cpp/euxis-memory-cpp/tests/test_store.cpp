#include <gtest/gtest.h>
#include <sodium.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <random>
#include <string>

#include "euxis/memory/entry.hpp"
#include "euxis/memory/store.hpp"
#include "euxis/memory/tier.hpp"

namespace euxis::memory {
namespace {

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class MemoryStoreTest : public ::testing::Test {
protected:
    std::filesystem::path test_dir_;

    void SetUp() override {
        ASSERT_GE(sodium_init(), 0) << "sodium_init() failed";

        // Create a unique temp directory for each test.
        std::array<std::byte, 8> rand_bytes{};
        randombytes_buf(rand_bytes.data(), rand_bytes.size());
        std::string suffix;
        suffix.resize(17, '\0');
        sodium_bin2hex(suffix.data(), suffix.size(),
                       reinterpret_cast<const unsigned char*>(rand_bytes.data()),
                       rand_bytes.size());
        suffix.resize(16);

        test_dir_ = std::filesystem::temp_directory_path() /
                    ("euxis_test_" + suffix);
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
// Store and retrieve roundtrip
// ---------------------------------------------------------------------------
TEST_F(MemoryStoreTest, StoreAndRetrieveRoundtrip) {
    auto master = random_master_key();
    EncryptedMemoryStore store(test_dir_, master, "did:euxis:agent:alice");

    const std::string content = "Hello, encrypted memory world!";
    auto result = store.store(content, MemoryTier::Hot);
    ASSERT_TRUE(result.has_value()) << result.error();

    EXPECT_FALSE(result->entry_id.empty());
    EXPECT_EQ(result->tier, MemoryTier::Hot);
    EXPECT_FALSE(result->ciphertext_b64.empty());
    EXPECT_FALSE(result->created_at.empty());
    EXPECT_EQ(result->agent_did, "did:euxis:agent:alice");

    auto retrieved = store.retrieve(result->entry_id);
    ASSERT_TRUE(retrieved.has_value()) << retrieved.error();
    EXPECT_EQ(*retrieved, content);
}

// ---------------------------------------------------------------------------
// Store multiple entries and retrieve each
// ---------------------------------------------------------------------------
TEST_F(MemoryStoreTest, StoreMultipleRetrieveEach) {
    auto master = random_master_key();
    EncryptedMemoryStore store(test_dir_, master, "did:euxis:agent:bob");

    std::vector<std::string> contents = {
        "First memory entry",
        "Second memory entry with more data",
        "Third entry: special chars !@#$%^&*()"
    };

    std::vector<std::string> ids;
    for (const auto& c : contents) {
        auto result = store.store(c, MemoryTier::Hot);
        ASSERT_TRUE(result.has_value()) << result.error();
        ids.push_back(result->entry_id);
    }

    // Retrieve each and verify content matches.
    for (size_t i = 0; i < contents.size(); ++i) {
        auto retrieved = store.retrieve(ids[i]);
        ASSERT_TRUE(retrieved.has_value()) << retrieved.error();
        EXPECT_EQ(*retrieved, contents[i]);
    }
}

// ---------------------------------------------------------------------------
// Retrieve non-existent entry returns error
// ---------------------------------------------------------------------------
TEST_F(MemoryStoreTest, RetrieveNonExistentReturnsError) {
    auto master = random_master_key();
    EncryptedMemoryStore store(test_dir_, master, "did:euxis:agent:charlie");

    auto result = store.retrieve("deadbeefcafebabe0123456789abcdef");
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("not found") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Store with different tiers
// ---------------------------------------------------------------------------
TEST_F(MemoryStoreTest, StoreWithDifferentTiers) {
    auto master = random_master_key();
    EncryptedMemoryStore store(test_dir_, master, "did:euxis:agent:diana");

    auto hot = store.store("hot data", MemoryTier::Hot);
    ASSERT_TRUE(hot.has_value()) << hot.error();
    EXPECT_EQ(hot->tier, MemoryTier::Hot);

    auto relevant = store.store("relevant data", MemoryTier::Relevant);
    ASSERT_TRUE(relevant.has_value()) << relevant.error();
    EXPECT_EQ(relevant->tier, MemoryTier::Relevant);

    auto cross = store.store("cross-agent data", MemoryTier::CrossAgent);
    ASSERT_TRUE(cross.has_value()) << cross.error();
    EXPECT_EQ(cross->tier, MemoryTier::CrossAgent);

    // All should be retrievable.
    auto r1 = store.retrieve(hot->entry_id);
    ASSERT_TRUE(r1.has_value()) << r1.error();
    EXPECT_EQ(*r1, "hot data");

    auto r2 = store.retrieve(relevant->entry_id);
    ASSERT_TRUE(r2.has_value()) << r2.error();
    EXPECT_EQ(*r2, "relevant data");

    auto r3 = store.retrieve(cross->entry_id);
    ASSERT_TRUE(r3.has_value()) << r3.error();
    EXPECT_EQ(*r3, "cross-agent data");
}

// ---------------------------------------------------------------------------
// Export tier entries
// ---------------------------------------------------------------------------
TEST_F(MemoryStoreTest, ExportTierEntries) {
    auto master = random_master_key();
    EncryptedMemoryStore store(test_dir_, master, "did:euxis:agent:eve");

    // Store entries across tiers.
    store.store("hot1", MemoryTier::Hot);
    store.store("hot2", MemoryTier::Hot);
    store.store("relevant1", MemoryTier::Relevant);
    store.store("cross1", MemoryTier::CrossAgent);

    auto hot_entries = store.export_tier_encrypted(MemoryTier::Hot);
    EXPECT_EQ(hot_entries.size(), 2u);
    for (const auto& e : hot_entries) {
        EXPECT_EQ(e.tier, MemoryTier::Hot);
        EXPECT_FALSE(e.ciphertext_b64.empty());
    }

    auto relevant_entries = store.export_tier_encrypted(MemoryTier::Relevant);
    EXPECT_EQ(relevant_entries.size(), 1u);

    auto cross_entries = store.export_tier_encrypted(MemoryTier::CrossAgent);
    EXPECT_EQ(cross_entries.size(), 1u);
}

// ---------------------------------------------------------------------------
// retrieve_tier returns correct entries
// ---------------------------------------------------------------------------
TEST_F(MemoryStoreTest, RetrieveTierReturnsCorrectEntries) {
    auto master = random_master_key();
    EncryptedMemoryStore store(test_dir_, master, "did:euxis:agent:frank");

    store.store("hot memory 1", MemoryTier::Hot);
    store.store("hot memory 2", MemoryTier::Hot);
    store.store("relevant memory", MemoryTier::Relevant);

    auto hot_results = store.retrieve_tier(MemoryTier::Hot);
    EXPECT_EQ(hot_results.size(), 2u);
    EXPECT_EQ(hot_results[0], "hot memory 1");
    EXPECT_EQ(hot_results[1], "hot memory 2");

    auto relevant_results = store.retrieve_tier(MemoryTier::Relevant);
    EXPECT_EQ(relevant_results.size(), 1u);
    EXPECT_EQ(relevant_results[0], "relevant memory");
}

// ---------------------------------------------------------------------------
// retrieve_tier respects limit
// ---------------------------------------------------------------------------
TEST_F(MemoryStoreTest, RetrieveTierRespectsLimit) {
    auto master = random_master_key();
    EncryptedMemoryStore store(test_dir_, master, "did:euxis:agent:grace");

    for (int i = 0; i < 10; ++i) {
        store.store("entry " + std::to_string(i), MemoryTier::Hot);
    }

    auto results = store.retrieve_tier(MemoryTier::Hot, 3);
    EXPECT_EQ(results.size(), 3u);
}

// ---------------------------------------------------------------------------
// Entry JSON serialisation roundtrip
// ---------------------------------------------------------------------------
TEST_F(MemoryStoreTest, EntryJsonRoundtrip) {
    EncryptedMemoryEntry entry;
    entry.entry_id       = "abcdef0123456789abcdef0123456789";
    entry.tier           = MemoryTier::Relevant;
    entry.ciphertext_b64 = "SGVsbG8gV29ybGQ=";
    entry.created_at     = "2026-03-01T12:00:00.000Z";
    entry.agent_did      = "did:euxis:agent:test";

    auto j = entry.to_json();
    auto restored = EncryptedMemoryEntry::from_json(j);

    EXPECT_EQ(restored.entry_id, entry.entry_id);
    EXPECT_EQ(restored.tier, entry.tier);
    EXPECT_EQ(restored.ciphertext_b64, entry.ciphertext_b64);
    EXPECT_EQ(restored.created_at, entry.created_at);
    EXPECT_EQ(restored.agent_did, entry.agent_did);
}

// ---------------------------------------------------------------------------
// Persistence: entries survive store destruction and reload
// ---------------------------------------------------------------------------
TEST_F(MemoryStoreTest, PersistenceAcrossInstances) {
    auto master = random_master_key();
    const std::string did = "did:euxis:agent:persist";

    std::string entry_id;
    {
        EncryptedMemoryStore store(test_dir_, master, did);
        auto result = store.store("persistent data", MemoryTier::Hot);
        ASSERT_TRUE(result.has_value()) << result.error();
        entry_id = result->entry_id;
    }

    // Re-create with the same master key and DID.
    EncryptedMemoryStore store2(test_dir_, master, did);
    auto retrieved = store2.retrieve(entry_id);
    ASSERT_TRUE(retrieved.has_value()) << retrieved.error();
    EXPECT_EQ(*retrieved, "persistent data");
}

} // namespace
} // namespace euxis::memory
