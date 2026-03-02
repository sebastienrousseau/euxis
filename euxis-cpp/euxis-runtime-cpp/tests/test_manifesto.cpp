#include <gtest/gtest.h>

#include "euxis/runtime/manifesto.hpp"

namespace euxis::runtime {
namespace {

auto make_valid_manifesto() -> AgentManifesto {
    return {
        .identity = {
            .name = "analyzer",
            .role = "code-review",
            .version = "1.0.0",
            .did = "did:euxis:analyzer-01",
            .description = "Code review agent",
            .tags = {"security", "quality"},
        },
        .goal = {
            .primary_objective = "Review code for security issues",
            .success_criteria = "Zero critical vulnerabilities",
            .sub_goals = {"Check OWASP top 10", "Verify input validation"},
        },
        .constraints = {
            .constraints = {
                {.type = "hard", .rule = "Never execute untrusted code", .severity = "critical"},
                {.type = "soft", .rule = "Prefer concise output", .severity = "low"},
            },
            .allowed_tools = {"read_file", "grep"},
            .forbidden_actions = {"delete_file", "deploy"},
            .authorization_scope = "read-only",
            .pii_filtering_required = true,
        },
    };
}

TEST(ManifestoTest, SerializeRoundtrip) {
    auto m = make_valid_manifesto();
    auto j = manifesto_to_json(m);
    auto result = manifesto_from_json(j);
    ASSERT_TRUE(result.has_value()) << result.error();

    EXPECT_EQ(result->identity.name, "analyzer");
    EXPECT_EQ(result->identity.did, "did:euxis:analyzer-01");
    EXPECT_EQ(result->goal.primary_objective, "Review code for security issues");
    EXPECT_EQ(result->constraints.constraints.size(), 2u);
    EXPECT_EQ(result->constraints.allowed_tools.size(), 2u);
    EXPECT_TRUE(result->constraints.pii_filtering_required);
}

TEST(ManifestoTest, ValidateValid) {
    auto m = make_valid_manifesto();
    auto result = validate_manifesto(m);
    EXPECT_TRUE(result.has_value());
}

TEST(ManifestoTest, ValidateMissingName) {
    auto m = make_valid_manifesto();
    m.identity.name.clear();
    auto result = validate_manifesto(m);
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("name"), std::string::npos);
}

TEST(ManifestoTest, ValidateMissingRole) {
    auto m = make_valid_manifesto();
    m.identity.role.clear();
    auto result = validate_manifesto(m);
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("role"), std::string::npos);
}

TEST(ManifestoTest, ValidateMissingObjective) {
    auto m = make_valid_manifesto();
    m.goal.primary_objective.clear();
    auto result = validate_manifesto(m);
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("objective"), std::string::npos);
}

TEST(ManifestoTest, DeserializeMissingBlocks) {
    nlohmann::json j = {{"identity", {{"name", "test"}}}};
    auto result = manifesto_from_json(j);
    EXPECT_FALSE(result.has_value());
}

TEST(ManifestoTest, ConstraintTypes) {
    auto m = make_valid_manifesto();
    auto j = manifesto_to_json(m);

    auto result = manifesto_from_json(j);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->constraints.constraints[0].type, "hard");
    EXPECT_EQ(result->constraints.constraints[1].type, "soft");
    EXPECT_EQ(result->constraints.constraints[0].severity, "critical");
}

} // namespace
} // namespace euxis::runtime
