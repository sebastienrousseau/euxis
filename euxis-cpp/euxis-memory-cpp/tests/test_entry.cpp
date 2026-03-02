#include "euxis/memory/entry.hpp"

#include <gtest/gtest.h>

namespace euxis::memory {
namespace {

TEST(EncryptedMemoryEntryTest, ToJsonRoundTrip) {
    EncryptedMemoryEntry original;
    original.entry_id = "abc123";
    original.tier = MemoryTier::Hot;
    original.ciphertext_b64 = "Y2lwaGVydGV4dA==";
    original.created_at = "2026-03-01T00:00:00Z";
    original.agent_did = "did:euxis:agent-1";

    auto j = original.to_json();
    auto restored = EncryptedMemoryEntry::from_json(j);

    EXPECT_EQ(restored.entry_id, original.entry_id);
    EXPECT_EQ(restored.tier, original.tier);
    EXPECT_EQ(restored.ciphertext_b64, original.ciphertext_b64);
    EXPECT_EQ(restored.created_at, original.created_at);
    EXPECT_EQ(restored.agent_did, original.agent_did);
}

TEST(EncryptedMemoryEntryTest, ToJsonFieldNames) {
    EncryptedMemoryEntry e;
    e.entry_id = "id1";
    e.tier = MemoryTier::Relevant;
    e.ciphertext_b64 = "ct";
    e.created_at = "ts";
    e.agent_did = "did";

    auto j = e.to_json();
    EXPECT_EQ(j["entry_id"], "id1");
    EXPECT_EQ(j["tier"], "relevant");
    EXPECT_EQ(j["ciphertext"], "ct");
    EXPECT_EQ(j["created_at"], "ts");
    EXPECT_EQ(j["agent_did"], "did");
}

TEST(EncryptedMemoryEntryTest, CrossAgentTierRoundTrip) {
    EncryptedMemoryEntry e;
    e.entry_id = "x";
    e.tier = MemoryTier::CrossAgent;
    e.ciphertext_b64 = "c";
    e.created_at = "t";
    e.agent_did = "d";

    auto j = e.to_json();
    EXPECT_EQ(j["tier"], "cross_agent");

    auto restored = EncryptedMemoryEntry::from_json(j);
    EXPECT_EQ(restored.tier, MemoryTier::CrossAgent);
}

TEST(EncryptedMemoryEntryTest, FromJsonInvalidTierThrows) {
    nlohmann::json j = {
        {"entry_id", "x"},
        {"tier", "invalid_tier"},
        {"ciphertext", "c"},
        {"created_at", "t"},
        {"agent_did", "d"},
    };
    EXPECT_THROW([[maybe_unused]] auto e = EncryptedMemoryEntry::from_json(j),
                 std::invalid_argument);
}

TEST(EncryptedMemoryEntryTest, FromJsonMissingFieldThrows) {
    nlohmann::json j = {{"entry_id", "x"}};
    EXPECT_THROW([[maybe_unused]] auto e = EncryptedMemoryEntry::from_json(j),
                 nlohmann::json::out_of_range);
}

TEST(TierLabelTest, HotLabel) {
    auto label = tier_label(MemoryTier::Hot);
    EXPECT_EQ(label, "hot");
}

TEST(TierLabelTest, RelevantLabel) {
    auto label = tier_label(MemoryTier::Relevant);
    EXPECT_EQ(label, "relevant");
}

TEST(TierLabelTest, CrossAgentLabel) {
    auto label = tier_label(MemoryTier::CrossAgent);
    EXPECT_EQ(label, "cross_agent");
}

} // namespace
} // namespace euxis::memory
