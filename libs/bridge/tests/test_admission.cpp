#include <gtest/gtest.h>
#include <euxis/bridge/admission.hpp>

#include <filesystem>
#include <fstream>

#include <sodium.h>

namespace euxis::bridge {

class AdmissionTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { [[maybe_unused]] int rc = sodium_init(); }

    std::filesystem::path tmp_dir_;

    void SetUp() override {
        tmp_dir_ = std::filesystem::temp_directory_path() / "euxis_test_admission";
        std::filesystem::create_directories(tmp_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(tmp_dir_);
    }

    auto make_valid_skill() -> BridgedSkill {
        // Create a clean source directory
        auto skill_dir = tmp_dir_ / "clean-skill";
        std::filesystem::create_directories(skill_dir);

        std::ofstream f(skill_dir / "index.js");
        f << "function run() { return 42; }\n";
        f.close();

        BridgedSkill skill;
        skill.name = "clean-skill";
        skill.slug = "clean_skill";
        skill.source_dir = skill_dir;
        skill.runtime = "node";
        skill.entrypoint = skill_dir / "index.js";
        skill.description = "A clean skill";
        return skill;
    }
};

TEST_F(AdmissionTest, ValidSkillAdmitted) {
    auto skill = make_valid_skill();
    AdmissionPipeline pipeline;

    auto result = pipeline.evaluate(skill);
    EXPECT_TRUE(result.admitted);
    EXPECT_FALSE(result.stages_passed.empty());
    EXPECT_TRUE(result.stages_failed.empty());
}

TEST_F(AdmissionTest, EmptyNameRejected) {
    auto skill = make_valid_skill();
    skill.name = "";

    AdmissionPipeline pipeline;
    auto result = pipeline.evaluate(skill);
    EXPECT_FALSE(result.admitted);

    bool structure_failed = false;
    for (const auto& s : result.stages_failed) {
        if (s == "structure") structure_failed = true;
    }
    EXPECT_TRUE(structure_failed);
}

TEST_F(AdmissionTest, MaliciousCodeRejected) {
    auto skill_dir = tmp_dir_ / "evil-skill";
    std::filesystem::create_directories(skill_dir);

    std::ofstream f(skill_dir / "index.js");
    f << "eval('malicious code');\nrequire('child_process').exec('hack');\n";
    f.close();

    BridgedSkill skill;
    skill.name = "evil-skill";
    skill.slug = "evil_skill";
    skill.source_dir = skill_dir;
    skill.runtime = "node";
    skill.entrypoint = skill_dir / "index.js";

    AdmissionPipeline pipeline;
    auto result = pipeline.evaluate(skill);
    EXPECT_FALSE(result.admitted);
    EXPECT_FALSE(result.findings.empty());
}

TEST_F(AdmissionTest, SignatureRequiredButMissing) {
    auto skill = make_valid_skill();

    SkillExecutionPolicy policy;
    policy.require_signature = true;
    // No signature_path on skill

    AdmissionPipeline pipeline;
    auto result = pipeline.evaluate(skill, policy);
    EXPECT_FALSE(result.admitted);

    bool sig_failed = false;
    for (const auto& s : result.stages_failed) {
        if (s == "signature") sig_failed = true;
    }
    EXPECT_TRUE(sig_failed);
}

TEST_F(AdmissionTest, SignatureNotRequired) {
    auto skill = make_valid_skill();

    SkillExecutionPolicy policy;
    policy.require_signature = false;

    AdmissionPipeline pipeline;
    auto result = pipeline.evaluate(skill, policy);
    EXPECT_TRUE(result.admitted);
}

TEST_F(AdmissionTest, NoPolicy) {
    auto skill = make_valid_skill();
    AdmissionPipeline pipeline;

    auto result = pipeline.evaluate(skill);
    EXPECT_TRUE(result.admitted);
}

TEST_F(AdmissionTest, StagesTracked) {
    auto skill = make_valid_skill();
    AdmissionPipeline pipeline;

    auto result = pipeline.evaluate(skill);
    // Should pass structure, static_analysis, and signature
    EXPECT_GE(result.stages_passed.size(), 3u);
}

TEST_F(AdmissionTest, EmptyRuntimeRejected) {
    auto skill = make_valid_skill();
    skill.runtime = "";

    AdmissionPipeline pipeline;
    auto result = pipeline.evaluate(skill);
    EXPECT_FALSE(result.admitted);

    bool structure_failed = false;
    for (const auto& s : result.stages_failed) {
        if (s == "structure") structure_failed = true;
    }
    EXPECT_TRUE(structure_failed);
}

TEST_F(AdmissionTest, EmptyEntrypointRejected) {
    auto skill = make_valid_skill();
    skill.entrypoint = "";

    AdmissionPipeline pipeline;
    auto result = pipeline.evaluate(skill);
    EXPECT_FALSE(result.admitted);

    bool structure_failed = false;
    for (const auto& s : result.stages_failed) {
        if (s == "structure") structure_failed = true;
    }
    EXPECT_TRUE(structure_failed);
}

TEST_F(AdmissionTest, EmptySourceDirRejected) {
    auto skill = make_valid_skill();
    skill.source_dir = "";

    AdmissionPipeline pipeline;
    auto result = pipeline.evaluate(skill);
    EXPECT_FALSE(result.admitted);

    bool structure_failed = false;
    for (const auto& s : result.stages_failed) {
        if (s == "structure") structure_failed = true;
    }
    EXPECT_TRUE(structure_failed);
}

TEST_F(AdmissionTest, SignatureRequiredNoPublicKeyPath) {
    auto skill = make_valid_skill();
    skill.signature_path = tmp_dir_ / "fake.sig";

    SkillExecutionPolicy policy;
    policy.require_signature = true;
    // No public_key_path set

    AdmissionPipeline pipeline;
    auto result = pipeline.evaluate(skill, policy);
    EXPECT_FALSE(result.admitted);

    bool sig_failed = false;
    for (const auto& s : result.stages_failed) {
        if (s == "signature") sig_failed = true;
    }
    EXPECT_TRUE(sig_failed);
}

TEST_F(AdmissionTest, SignatureRequiredWithInvalidKeyPath) {
    auto skill = make_valid_skill();
    skill.signature_path = tmp_dir_ / "fake.sig";

    SkillExecutionPolicy policy;
    policy.require_signature = true;
    policy.public_key_path = tmp_dir_ / "nonexistent.pub";

    AdmissionPipeline pipeline;
    auto result = pipeline.evaluate(skill, policy);
    EXPECT_FALSE(result.admitted);

    bool sig_failed = false;
    for (const auto& s : result.stages_failed) {
        if (s == "signature") sig_failed = true;
    }
    EXPECT_TRUE(sig_failed);
}

TEST_F(AdmissionTest, NonexistentSourceDirPassesStaticAnalysis) {
    // Source dir doesn't exist on filesystem -> static analysis passes (no code to check)
    BridgedSkill skill;
    skill.name = "ghost-skill";
    skill.runtime = "node";
    skill.entrypoint = "/nonexistent/path/index.js";
    skill.source_dir = tmp_dir_ / "nonexistent-dir";

    AdmissionPipeline pipeline;
    auto result = pipeline.evaluate(skill);
    // Structure should pass since fields are non-empty
    // Static analysis should pass (no source to analyze)
    // Check that static_analysis passed
    bool analysis_passed = false;
    for (const auto& s : result.stages_passed) {
        if (s == "static_analysis") analysis_passed = true;
    }
    EXPECT_TRUE(analysis_passed);
}

TEST_F(AdmissionTest, CustomReputationThreshold) {
    AdmissionPipeline pipeline(0.8);
    auto skill = make_valid_skill();
    auto result = pipeline.evaluate(skill);
    // Should still work with custom threshold for structure/analysis/signature
    EXPECT_TRUE(result.admitted);
}

// --- Coverage: lines 91-95 (verify_skill_signature path in check_signature) ---
TEST_F(AdmissionTest, SignatureWithValidKeyButBadSignatureFile) {
    auto skill = make_valid_skill();

    // Create a fake signature file (wrong size)
    auto sig_path = tmp_dir_ / "bad.sig";
    std::ofstream sf(sig_path, std::ios::binary);
    sf << "not-a-valid-64-byte-signature";
    sf.close();
    skill.signature_path = sig_path;

    // Create a fake public key file (32 bytes)
    auto key_path = tmp_dir_ / "test.pub";
    std::ofstream kf(key_path, std::ios::binary);
    std::array<char, 32> key_data{};
    kf.write(key_data.data(), 32);
    kf.close();

    SkillExecutionPolicy policy;
    policy.require_signature = true;
    policy.public_key_path = key_path;

    AdmissionPipeline pipeline;
    auto result = pipeline.evaluate(skill, policy);
    // verify_skill_signature should fail (bad sig size), so signature stage fails
    EXPECT_FALSE(result.admitted);
    bool sig_failed = false;
    for (const auto& s : result.stages_failed) {
        if (s == "signature") sig_failed = true;
    }
    EXPECT_TRUE(sig_failed);
}

}  // namespace euxis::bridge
