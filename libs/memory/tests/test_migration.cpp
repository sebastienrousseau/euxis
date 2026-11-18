// XChaCha20-Poly1305 entries, legacy AES-256-GCM reads, and migration.
#include <gtest/gtest.h>
#include <sodium.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "euxis/crypto/aes_gcm.hpp"
#include "euxis/crypto/key_derivation.hpp"
#include "euxis/crypto/xchacha.hpp"
#include "euxis/memory/entry.hpp"
#include "euxis/memory/store.hpp"
#include "euxis/memory/tier.hpp"

// NOLINTBEGIN(bugprone-unused-return-value)

namespace euxis::memory {
namespace {

constexpr std::string_view kDid = "did:euxis:agent:migration";

auto b64(std::span<const std::byte> data) -> std::string {
    std::string out(sodium_base64_encoded_len(data.size(), sodium_base64_VARIANT_ORIGINAL), '\0');
    sodium_bin2base64(out.data(), out.size(),
                      reinterpret_cast<const unsigned char*>(data.data()), data.size(),
                      sodium_base64_VARIANT_ORIGINAL);
    out.resize(std::strlen(out.c_str()));
    return out;
}

auto unb64(const std::string& s) -> std::vector<std::byte> {
    std::vector<std::byte> out(s.size());
    size_t len = 0;
    sodium_base642bin(reinterpret_cast<unsigned char*>(out.data()), out.size(), s.data(),
                      s.size(), nullptr, &len, nullptr, sodium_base64_VARIANT_ORIGINAL);
    out.resize(len);
    return out;
}

auto bytes(std::string_view s) -> std::span<const std::byte> {
    return {reinterpret_cast<const std::byte*>(s.data()), s.size()};
}

class MemoryMigrationTest : public ::testing::Test {
protected:
    std::filesystem::path dir_;
    std::array<std::byte, 32> master_{};

    void SetUp() override {
        ASSERT_GE(sodium_init(), 0);
        dir_ = std::filesystem::temp_directory_path() /
               ("euxis_migr_" + std::to_string(randombytes_random()));
        std::filesystem::create_directories(dir_);
        randombytes_buf(master_.data(), master_.size());
    }

    void TearDown() override { std::filesystem::remove_all(dir_); }

    /// The agent key EncryptedMemoryStore derives (store.cpp derive_agent_key).
    [[nodiscard]] auto agent_key() const -> std::array<std::byte, 32> {
        std::array<std::byte, 32> did_hash{};
        crypto_hash_sha256(reinterpret_cast<unsigned char*>(did_hash.data()),
                           reinterpret_cast<const unsigned char*>(kDid.data()), kDid.size());
        auto derived = euxis::crypto::derive_key(
            master_, std::span<const std::byte>(did_hash.data(), 16), 100'000, 32);
        EXPECT_TRUE(derived.has_value());
        std::array<std::byte, 32> key{};
        std::memcpy(key.data(), derived->key.data(), key.size());
        return key;
    }

    /// dir / sha256_hex(did) / encrypted_memory.jsonl, as the store lays it out.
    [[nodiscard]] auto store_file() const -> std::filesystem::path {
        std::array<unsigned char, 32> h{};
        crypto_hash_sha256(h.data(), reinterpret_cast<const unsigned char*>(kDid.data()),
                           kDid.size());
        std::string hex(65, '\0');
        sodium_bin2hex(hex.data(), hex.size(), h.data(), h.size());
        hex.resize(64);
        return dir_ / hex / "encrypted_memory.jsonl";
    }

    [[nodiscard]] auto read_file() const -> std::string {
        std::ifstream in(store_file());
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    /// Append a pre-tag entry sealed with AES-256-GCM, as older builds wrote.
    void append_legacy(EncryptedMemoryStore& store, const std::string& id,
                       std::string_view content, MemoryTier tier) {
        auto key = agent_key();
        auto enc = euxis::crypto::encrypt_aad(bytes(content), key, bytes(tier_label(tier)));
        ASSERT_TRUE(enc.has_value());
        std::vector<std::byte> payload(enc->iv.begin(), enc->iv.end());
        payload.insert(payload.end(), enc->ciphertext.begin(), enc->ciphertext.end());
        append_raw(store, legacy_json(id, b64(payload), tier));
    }

    [[nodiscard]] static auto legacy_json(const std::string& id, const std::string& ct,
                                          MemoryTier tier) -> std::string {
        nlohmann::json j{{"entry_id", id}, {"tier", std::string(tier_label(tier))},
                         {"ciphertext", ct}, {"created_at", "2026-01-01T00:00:00.000Z"},
                         {"agent_did", std::string(kDid)}};
        return j.dump(); // no "alg": the pre-tag format
    }

    /// Append a raw JSONL line (the store constructor created the directory).
    void append_raw(EncryptedMemoryStore& /*store*/, const std::string& line) {
        std::ofstream(store_file(), std::ios::app) << line << '\n';
    }

};

/// Tests that write legacy AES-256-GCM entries. GTEST_SKIP must run in
/// SetUp or the test body: in a helper it only returns from the helper.
class MemoryMigrationLegacyTest : public MemoryMigrationTest {
protected:
    void SetUp() override {
        MemoryMigrationTest::SetUp();
        if (!euxis::crypto::aes_gcm_available()) {
            GTEST_SKIP() << "legacy AES-256-GCM needs AES instructions on this CPU";
        }
    }
};

TEST_F(MemoryMigrationTest, NewEntriesAreXChaCha) {
    EncryptedMemoryStore store(dir_, master_, std::string(kDid));
    auto e = store.store("fresh", MemoryTier::Hot);
    ASSERT_TRUE(e.has_value()) << e.error();
    EXPECT_EQ(e->alg, kAlgXChaCha);
    EXPECT_EQ(unb64(e->ciphertext_b64).size(),
              euxis::crypto::kXChaChaNonceBytes + 5 + euxis::crypto::kXChaChaTagBytes);
    EXPECT_NE(read_file().find(R"("alg":"XChaCha20-Poly1305")"), std::string::npos);
    EXPECT_EQ(store.retrieve(e->entry_id).value(), "fresh");
}

TEST_F(MemoryMigrationLegacyTest, LegacyEntryIsReadable) {
    EncryptedMemoryStore store(dir_, master_, std::string(kDid));
    append_legacy(store, "legacy1", "old secret", MemoryTier::Relevant);
    EncryptedMemoryStore reopened(dir_, master_, std::string(kDid));
    auto got = reopened.retrieve("legacy1");
    ASSERT_TRUE(got.has_value()) << got.error();
    EXPECT_EQ(*got, "old secret");
}

TEST_F(MemoryMigrationLegacyTest, MigrateRewritesLegacyAndKeepsEverythingElse) {
    EncryptedMemoryStore store(dir_, master_, std::string(kDid));
    auto current = store.store("current", MemoryTier::Hot);
    ASSERT_TRUE(current.has_value());
    append_legacy(store, "legacy1", "old secret", MemoryTier::Relevant);
    append_raw(store, "not json at all");

    EncryptedMemoryStore reopened(dir_, master_, std::string(kDid));
    auto n = reopened.migrate_legacy_entries();
    ASSERT_TRUE(n.has_value()) << n.error();
    EXPECT_EQ(*n, 1u);

    const auto text = read_file();
    EXPECT_NE(text.find("not json at all"), std::string::npos);
    EXPECT_EQ(text.find(R"("alg":"AES-256-GCM")"), std::string::npos);
    EXPECT_EQ(reopened.retrieve("legacy1").value(), "old secret");
    EXPECT_EQ(reopened.retrieve(current->entry_id).value(), "current");
    for (const auto& e : reopened.export_tier_encrypted(MemoryTier::Relevant)) {
        EXPECT_EQ(e.alg, kAlgXChaCha);
        EXPECT_EQ(e.created_at, "2026-01-01T00:00:00.000Z");
    }
    EXPECT_FALSE(std::filesystem::exists(store_file().string() + ".migrating"));
}

TEST_F(MemoryMigrationLegacyTest, MigrateIsIdempotent) {
    EncryptedMemoryStore store(dir_, master_, std::string(kDid));
    append_legacy(store, "legacy1", "x", MemoryTier::Hot);
    ASSERT_EQ(store.migrate_legacy_entries().value(), 1u);
    const auto after_first = read_file();
    EXPECT_EQ(store.migrate_legacy_entries().value(), 0u);
    EXPECT_EQ(read_file(), after_first);
}

TEST_F(MemoryMigrationTest, UndecryptableLegacyEntryAbortsWithoutWriting) {
    // Any CPU: garbage fails authentication, or AES-GCM is unsupported.
    EncryptedMemoryStore store(dir_, master_, std::string(kDid));
    ASSERT_TRUE(store.store("keep me", MemoryTier::Hot).has_value());
    std::array<std::byte, 40> junk{};
    randombytes_buf(junk.data(), junk.size());
    append_raw(store, legacy_json("broken", b64(junk), MemoryTier::Hot));
    const auto before = read_file();

    auto n = store.migrate_legacy_entries();
    ASSERT_FALSE(n.has_value());
    EXPECT_NE(n.error().find("broken"), std::string::npos);
    EXPECT_EQ(read_file(), before);
}

TEST_F(MemoryMigrationTest, MigrateWithoutStoreFileIsNoOp) {
    EncryptedMemoryStore store(dir_, master_, std::string(kDid));
    EXPECT_EQ(store.migrate_legacy_entries().value(), 0u);
}

TEST_F(MemoryMigrationTest, MigrateAfterKeyDestructionFails) {
    EncryptedMemoryStore store(dir_, master_, std::string(kDid));
    store.destroy_agent_keys();
    EXPECT_FALSE(store.migrate_legacy_entries().has_value());
}

TEST_F(MemoryMigrationTest, UnknownAlgorithmIsRejected) {
    EncryptedMemoryStore store(dir_, master_, std::string(kDid));
    auto j = nlohmann::json::parse(legacy_json("rot13", "AAAA", MemoryTier::Hot));
    j["alg"] = "ROT13";
    append_raw(store, j.dump());
    EncryptedMemoryStore reopened(dir_, master_, std::string(kDid));
    auto got = reopened.retrieve("rot13");
    ASSERT_FALSE(got.has_value());
    EXPECT_NE(got.error().find("Unsupported algorithm: ROT13"), std::string::npos);
}

} // namespace
} // namespace euxis::memory

// NOLINTEND(bugprone-unused-return-value)
