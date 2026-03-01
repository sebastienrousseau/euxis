#include <gtest/gtest.h>
#include <euxis/bridge/provenance.hpp>

#include <filesystem>
#include <fstream>

#include <sodium.h>

namespace euxis::bridge {

class ProvenanceTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { sodium_init(); }

    std::filesystem::path tmp_dir_;

    void SetUp() override {
        tmp_dir_ = std::filesystem::temp_directory_path() / "euxis_test_provenance";
        std::filesystem::create_directories(tmp_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(tmp_dir_);
    }
};

TEST_F(ProvenanceTest, HashFile) {
    auto file = tmp_dir_ / "test.txt";
    std::ofstream f(file);
    f << "hello world";
    f.close();

    auto hash = ProvenanceChain::hash_file(file);
    EXPECT_FALSE(hash.empty());
    EXPECT_EQ(hash.size(), 64u);  // SHA-256 hex
}

TEST_F(ProvenanceTest, HashBytes) {
    auto hash = ProvenanceChain::hash_bytes("hello world");
    EXPECT_FALSE(hash.empty());
    EXPECT_EQ(hash.size(), 64u);
}

TEST_F(ProvenanceTest, ConsistentHashing) {
    auto h1 = ProvenanceChain::hash_bytes("test data");
    auto h2 = ProvenanceChain::hash_bytes("test data");
    EXPECT_EQ(h1, h2);
}

TEST_F(ProvenanceTest, DifferentDataDifferentHash) {
    auto h1 = ProvenanceChain::hash_bytes("data a");
    auto h2 = ProvenanceChain::hash_bytes("data b");
    EXPECT_NE(h1, h2);
}

TEST_F(ProvenanceTest, RecordAndVerify) {
    auto file = tmp_dir_ / "artifact.bin";
    std::ofstream f(file, std::ios::binary);
    f << "artifact content";
    f.close();

    ProvenanceChain chain;
    auto entry = chain.record(file, "builder-1", {"mat-hash-1"});

    EXPECT_FALSE(entry.artifact_hash.empty());
    EXPECT_EQ(entry.builder_id, "builder-1");
    EXPECT_EQ(entry.build_type, "https://slsa.dev/provenance/v1");
    EXPECT_EQ(entry.materials.size(), 1u);
    EXPECT_FALSE(entry.timestamp.empty());

    EXPECT_TRUE(chain.verify(file, entry));
}

TEST_F(ProvenanceTest, VerifyFailsAfterModification) {
    auto file = tmp_dir_ / "modified.bin";
    std::ofstream f(file);
    f << "original";
    f.close();

    ProvenanceChain chain;
    auto entry = chain.record(file, "builder");

    // Modify the file
    std::ofstream f2(file);
    f2 << "modified";
    f2.close();

    EXPECT_FALSE(chain.verify(file, entry));
}

TEST_F(ProvenanceTest, ChainGrows) {
    ProvenanceChain chain;

    auto f1 = tmp_dir_ / "a.txt";
    auto f2 = tmp_dir_ / "b.txt";
    std::ofstream(f1) << "a";
    std::ofstream(f2) << "b";

    chain.record(f1, "builder-1");
    chain.record(f2, "builder-2");

    EXPECT_EQ(chain.chain().size(), 2u);
}

TEST_F(ProvenanceTest, HashNonexistentFile) {
    auto hash = ProvenanceChain::hash_file(tmp_dir_ / "nonexistent");
    EXPECT_TRUE(hash.empty());
}

}  // namespace euxis::bridge
