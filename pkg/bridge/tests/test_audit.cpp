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

}  // namespace euxis::bridge
