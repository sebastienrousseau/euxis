#include <gtest/gtest.h>
#include "euxis/cli/playbook_schema.hpp"

#include <filesystem>
#include <fstream>

namespace euxis::cli {

TEST(PlaybookSchemaTest, ValidSimplePlaybook) {
    nlohmann::json pb;
    pb["name"] = "test";
    pb["steps"] = nlohmann::json::array({
        {{"name", "step1"}, {"agent", "reviewer"}, {"task", "review code"}}
    });
    auto result = validate_playbook(pb);
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.errors.empty());
}

TEST(PlaybookSchemaTest, MissingStepsAndPhases) {
    nlohmann::json pb;
    pb["name"] = "test";
    auto result = validate_playbook(pb);
    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.errors.empty());
}

TEST(PlaybookSchemaTest, EmptyStepsWarns) {
    nlohmann::json pb;
    pb["steps"] = nlohmann::json::array();
    auto result = validate_playbook(pb);
    EXPECT_TRUE(result.valid);
    EXPECT_FALSE(result.warnings.empty());
}

TEST(PlaybookSchemaTest, IncompatibleVersion) {
    nlohmann::json pb;
    pb["schema_version"] = "9.9";
    pb["steps"] = nlohmann::json::array({{{"agent", "a"}}});
    auto result = validate_playbook(pb);
    EXPECT_FALSE(result.valid);
}

TEST(PlaybookSchemaTest, CompatibleVersion) {
    EXPECT_TRUE(is_compatible_version("1.0"));
    EXPECT_TRUE(is_compatible_version("1.0.0"));
    EXPECT_TRUE(is_compatible_version("1.2"));
    EXPECT_FALSE(is_compatible_version("2.0"));
    EXPECT_FALSE(is_compatible_version("9.9"));
}

TEST(PlaybookSchemaTest, DuplicateStepNameWarns) {
    nlohmann::json pb;
    pb["steps"] = nlohmann::json::array({
        {{"name", "step1"}, {"agent", "a"}},
        {{"name", "step1"}, {"agent", "b"}}
    });
    auto result = validate_playbook(pb);
    EXPECT_TRUE(result.valid);
    bool has_dup_warn = false;
    for (const auto& w : result.warnings) {
        if (w.find("duplicate") != std::string::npos) has_dup_warn = true;
    }
    EXPECT_TRUE(has_dup_warn);
}

TEST(PlaybookSchemaTest, StepWithoutAgent) {
    nlohmann::json pb;
    pb["steps"] = nlohmann::json::array({
        {{"name", "step1"}}
    });
    auto result = validate_playbook(pb);
    EXPECT_TRUE(result.valid); // Warning, not error
    EXPECT_FALSE(result.warnings.empty());
}

TEST(PlaybookSchemaTest, ValidPhasedPlaybook) {
    nlohmann::json pb;
    pb["name"] = "phased";
    pb["phases"] = nlohmann::json::array({
        {{"sequence", 1}, {"delegates", nlohmann::json::array({
            {{"agent", "reviewer"}, {"task_template", "review ${goal}"}}
        })}}
    });
    auto result = validate_playbook(pb);
    EXPECT_TRUE(result.valid);
}

TEST(PlaybookSchemaTest, NonSequentialPhases) {
    nlohmann::json pb;
    pb["phases"] = nlohmann::json::array({
        {{"sequence", 2}},
        {{"sequence", 1}}
    });
    auto result = validate_playbook(pb);
    // Should warn about non-sequential
    bool has_seq_warn = false;
    for (const auto& w : result.warnings) {
        if (w.find("non-sequential") != std::string::npos) has_seq_warn = true;
    }
    EXPECT_TRUE(has_seq_warn);
}

TEST(PlaybookSchemaTest, NonSequentialPhasesWithPhaseKey) {
    // Real playbooks use "phase" key instead of "sequence"
    nlohmann::json pb;
    pb["phases"] = nlohmann::json::array({
        {{"phase", 3}},
        {{"phase", 1}}
    });
    auto result = validate_playbook(pb);
    bool has_seq_warn = false;
    for (const auto& w : result.warnings) {
        if (w.find("non-sequential") != std::string::npos) has_seq_warn = true;
    }
    EXPECT_TRUE(has_seq_warn);
}

TEST(PlaybookSchemaTest, DuplicateGateWarns) {
    nlohmann::json pb;
    pb["phases"] = nlohmann::json::array({
        {{"gate", "quality"}, {"delegates", nlohmann::json::array({{{"agent", "a"}, {"task", "t"}}})}},
        {{"gate", "quality"}, {"delegates", nlohmann::json::array({{{"agent", "b"}, {"task", "t"}}})}}
    });
    auto result = validate_playbook(pb);
    bool has_dup_warn = false;
    for (const auto& w : result.warnings) {
        if (w.find("duplicate gate") != std::string::npos) has_dup_warn = true;
    }
    EXPECT_TRUE(has_dup_warn);
}

TEST(PlaybookSchemaTest, MissingNameAndId) {
    nlohmann::json pb;
    pb["steps"] = nlohmann::json::array({{{"agent", "a"}}});
    auto result = validate_playbook(pb);
    bool has_name_warn = false;
    for (const auto& w : result.warnings) {
        if (w.find("name") != std::string::npos || w.find("id") != std::string::npos) has_name_warn = true;
    }
    EXPECT_TRUE(has_name_warn);
}

TEST(PlaybookSchemaTest, BadIdFormat) {
    nlohmann::json pb;
    pb["id"] = "Bad ID With Spaces!";
    pb["steps"] = nlohmann::json::array({{{"agent", "a"}}});
    auto result = validate_playbook(pb);
    bool has_id_warn = false;
    for (const auto& w : result.warnings) {
        if (w.find("pattern") != std::string::npos) has_id_warn = true;
    }
    EXPECT_TRUE(has_id_warn);
}

// Verify existing playbooks pass validation
TEST(PlaybookSchemaTest, VerifyEverythingPasses) {
    auto path = std::filesystem::path("/home/seb/.euxis/data/config/playbooks/verify-everything.json");
    if (!std::filesystem::exists(path)) {
        GTEST_SKIP() << "verify-everything.json not found";
    }
    std::ifstream f(path);
    auto pb = nlohmann::json::parse(f);
    auto result = validate_playbook(pb);
    EXPECT_TRUE(result.valid) << "verify-everything.json should pass validation";
}

TEST(PlaybookSchemaTest, NonObjectStepFails) {
    nlohmann::json pb;
    pb["steps"] = nlohmann::json::array({42, "not an object"});
    auto result = validate_playbook(pb);
    EXPECT_FALSE(result.valid);
}

TEST(PlaybookSchemaTest, DelegateMissingTask) {
    nlohmann::json pb;
    pb["phases"] = nlohmann::json::array({
        {{"delegates", nlohmann::json::array({
            {{"agent", "a"}}  // missing task_template
        })}}
    });
    auto result = validate_playbook(pb);
    bool has_task_warn = false;
    for (const auto& w : result.warnings) {
        if (w.find("task_template") != std::string::npos) has_task_warn = true;
    }
    EXPECT_TRUE(has_task_warn);
}

} // namespace euxis::cli
