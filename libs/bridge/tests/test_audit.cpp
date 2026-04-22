#include <gtest/gtest.h>
#include <euxis/bridge/audit.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace euxis::bridge {

class AuditTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_dir_;

    void SetUp() override {
        tmp_dir_ = std::filesystem::temp_directory_path() / "euxis_test_audit";
        std::filesystem::create_directories(tmp_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(tmp_dir_);
    }
};

TEST_F(AuditTest, LogCreatesFile) {
    auto log_path = tmp_dir_ / "audit.jsonl";
    AuditLogger logger(log_path);

    logger.log("test_event", "test-skill");

    EXPECT_TRUE(std::filesystem::exists(log_path));
}

TEST_F(AuditTest, LogAppends) {
    auto log_path = tmp_dir_ / "audit.jsonl";
    AuditLogger logger(log_path);

    logger.log("event1", "skill-a");
    logger.log("event2", "skill-b");

    std::ifstream f(log_path);
    int lines = 0;
    std::string line;
    while (std::getline(f, line)) {
        ++lines;
    }
    EXPECT_EQ(lines, 2);
}

TEST_F(AuditTest, LogAdmission) {
    auto log_path = tmp_dir_ / "audit.jsonl";
    AuditLogger logger(log_path);

    logger.log_admission("test-skill", true, {{"reason", "passed"}});

    std::ifstream f(log_path);
    std::string line;
    std::getline(f, line);
    auto j = nlohmann::json::parse(line);

    EXPECT_EQ(j["event_type"], "admission");
    EXPECT_EQ(j["skill_name"], "test-skill");
    EXPECT_TRUE(j["admitted"]);
}

TEST_F(AuditTest, LogExecution) {
    auto log_path = tmp_dir_ / "audit.jsonl";
    AuditLogger logger(log_path);

    logger.log_execution("test-skill", 0, std::chrono::milliseconds{150});

    std::ifstream f(log_path);
    std::string line;
    std::getline(f, line);
    auto j = nlohmann::json::parse(line);

    EXPECT_EQ(j["event_type"], "execution");
    EXPECT_EQ(j["exit_code"], 0);
    EXPECT_EQ(j["duration_ms"], 150);
}

TEST_F(AuditTest, HasTimestamp) {
    auto log_path = tmp_dir_ / "audit.jsonl";
    AuditLogger logger(log_path);

    logger.log("event", "skill");

    std::ifstream f(log_path);
    std::string line;
    std::getline(f, line);
    auto j = nlohmann::json::parse(line);

    EXPECT_TRUE(j.contains("timestamp"));
    EXPECT_FALSE(j["timestamp"].get<std::string>().empty());
}

TEST_F(AuditTest, Path) {
    auto log_path = tmp_dir_ / "audit.jsonl";
    AuditLogger logger(log_path);
    EXPECT_EQ(logger.path(), log_path);
}

TEST_F(AuditTest, CreateParentDirectories) {
    auto log_path = tmp_dir_ / "nested" / "dir" / "audit.jsonl";
    AuditLogger logger(log_path);

    logger.log("event", "skill");
    EXPECT_TRUE(std::filesystem::exists(log_path));
}

// --- P0-1: Hash-chained audit log tests ---

TEST_F(AuditTest, EntryContainsPrevHash) {
    auto log_path = tmp_dir_ / "chain.jsonl";
    AuditLogger logger(log_path);

    logger.log("event", "skill");

    std::ifstream f(log_path);
    std::string line;
    std::getline(f, line);
    auto j = nlohmann::json::parse(line);

    EXPECT_TRUE(j.contains("prev_hash"));
    // First entry has empty prev_hash (chain root)
    EXPECT_EQ(j["prev_hash"], "");
}

TEST_F(AuditTest, ChainLinksCorrectly) {
    auto log_path = tmp_dir_ / "chain.jsonl";
    AuditLogger logger(log_path);

    logger.log("event1", "skill-a");
    logger.log("event2", "skill-b");

    std::ifstream f(log_path);
    std::string line1, line2;
    std::getline(f, line1);
    std::getline(f, line2);

    auto j2 = nlohmann::json::parse(line2);
    // Second entry's prev_hash should be non-empty (hash of first line)
    EXPECT_FALSE(j2["prev_hash"].get<std::string>().empty());
}

TEST_F(AuditTest, VerifyChainValid) {
    auto log_path = tmp_dir_ / "chain.jsonl";
    AuditLogger logger(log_path);

    logger.log("event1", "skill-a");
    logger.log("event2", "skill-b");
    logger.log("event3", "skill-c");

    EXPECT_TRUE(AuditLogger::verify_chain(log_path));
}

TEST_F(AuditTest, VerifyChainDetectsTampering) {
    auto log_path = tmp_dir_ / "chain.jsonl";
    {
        AuditLogger logger(log_path);
        logger.log("event1", "skill-a");
        logger.log("event2", "skill-b");
        logger.log("event3", "skill-c");
    }

    // Tamper with the second line
    std::vector<std::string> lines;
    {
        std::ifstream f(log_path);
        std::string line;
        while (std::getline(f, line)) {
            lines.push_back(line);
        }
    }
    ASSERT_GE(lines.size(), 2u);
    auto j = nlohmann::json::parse(lines[1]);
    j["skill_name"] = "tampered";
    lines[1] = j.dump();

    {
        std::ofstream f(log_path, std::ios::trunc);
        for (const auto& l : lines) {
            f << l << '\n';
        }
    }

    EXPECT_FALSE(AuditLogger::verify_chain(log_path));
}

TEST_F(AuditTest, VerifyEmptyFileIsValid) {
    auto log_path = tmp_dir_ / "empty.jsonl";
    // File doesn't exist — valid chain
    EXPECT_TRUE(AuditLogger::verify_chain(log_path));

    // Create empty file — also valid
    { std::ofstream f(log_path); }
    EXPECT_TRUE(AuditLogger::verify_chain(log_path));
}

TEST_F(AuditTest, ChainHeadUpdates) {
    auto log_path = tmp_dir_ / "chain.jsonl";
    AuditLogger logger(log_path);

    EXPECT_TRUE(logger.chain_head_hash().empty());

    logger.log("event1", "skill-a");
    auto hash1 = logger.chain_head_hash();
    EXPECT_FALSE(hash1.empty());
    EXPECT_EQ(hash1.size(), 64u);  // SHA-256 hex = 64 chars

    logger.log("event2", "skill-b");
    auto hash2 = logger.chain_head_hash();
    EXPECT_NE(hash1, hash2);
}

TEST_F(AuditTest, WriteFailureKeepsHashUnchanged) {
    // Create a read-only directory so the log file cannot be written
    auto ro_dir = tmp_dir_ / "readonly";
    std::filesystem::create_directories(ro_dir);
    auto log_path = ro_dir / "audit.jsonl";

    // Write one entry successfully first
    AuditLogger logger(log_path);
    logger.log("event1", "skill-a");
    auto hash_after_first = logger.chain_head_hash();
    EXPECT_FALSE(hash_after_first.empty());

    // Make the directory read-only
    std::filesystem::permissions(ro_dir, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);

    // Remove the file so the open() will fail (dir is read-only)
    // Actually the file exists but let's try to write — the dir perms
    // prevent creating new files but existing file might still be writable.
    // Instead, remove the file and try to write a new one.
    // The open(O_CREAT) should fail because the directory is read-only.
    auto log_path2 = ro_dir / "audit2.jsonl";
    AuditLogger logger2(log_path2);
    logger2.log("should_fail", "fail-skill");

    // Hash should remain empty (initial state) since write failed
    EXPECT_TRUE(logger2.chain_head_hash().empty());

    // Restore permissions for cleanup
    std::filesystem::permissions(ro_dir, std::filesystem::perms::owner_all);
}

}  // namespace euxis::bridge
