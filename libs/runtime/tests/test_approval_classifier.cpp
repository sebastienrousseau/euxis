/// @file
/// @brief Tests for the ACP-style approval classifier in tool_manifest.

#include <gtest/gtest.h>

#include "euxis/runtime/tool_manifest.hpp"

namespace euxis::runtime {
namespace {

auto make_decl(std::string name) -> ToolDeclaration_v2 {
    ToolDeclaration_v2 d;
    d.name = std::move(name);
    d.description = "test tool";
    return d;
}

// ---------------------------------------------------------------------------
// Readonly verbs are recognised by exact prefix + word boundary
// ---------------------------------------------------------------------------
TEST(ApprovalClassifierTest, ReadonlyVerbsClassifyReadonly) {
    EXPECT_EQ(classify_approval(make_decl("list")),            ApprovalClass::Readonly);
    EXPECT_EQ(classify_approval(make_decl("list_files")),      ApprovalClass::Readonly);
    EXPECT_EQ(classify_approval(make_decl("get_status")),      ApprovalClass::Readonly);
    EXPECT_EQ(classify_approval(make_decl("show_diff")),       ApprovalClass::Readonly);
    EXPECT_EQ(classify_approval(make_decl("query_repo")),      ApprovalClass::Readonly);
    EXPECT_EQ(classify_approval(make_decl("validate_input")),  ApprovalClass::Readonly);
    EXPECT_EQ(classify_approval(make_decl("describe_agent")),  ApprovalClass::Readonly);
    EXPECT_EQ(classify_approval(make_decl("scan")),            ApprovalClass::Readonly);
}

// ---------------------------------------------------------------------------
// Case-insensitive matching
// ---------------------------------------------------------------------------
TEST(ApprovalClassifierTest, ReadonlyMatchingIsCaseInsensitive) {
    EXPECT_EQ(classify_approval(make_decl("LIST_users")),  ApprovalClass::Readonly);
    EXPECT_EQ(classify_approval(make_decl("Get_status")),  ApprovalClass::Readonly);
}

// ---------------------------------------------------------------------------
// False-positive guard: read-verb embedded in a different word does NOT
// classify as Readonly (e.g. "getup_destructive_action").
// ---------------------------------------------------------------------------
TEST(ApprovalClassifierTest, EmbeddedVerbDoesNotMatch) {
    EXPECT_EQ(classify_approval(make_decl("getup_destructive_action")),
              ApprovalClass::ExecCapable);
    EXPECT_EQ(classify_approval(make_decl("listenize")),
              ApprovalClass::ExecCapable);
    EXPECT_EQ(classify_approval(make_decl("validateandwrite")),
              ApprovalClass::ExecCapable);
}

// ---------------------------------------------------------------------------
// Conservative fallback for unknown verbs
// ---------------------------------------------------------------------------
TEST(ApprovalClassifierTest, UnknownVerbsFallBackToExecCapable) {
    EXPECT_EQ(classify_approval(make_decl("write_file")),    ApprovalClass::ExecCapable);
    EXPECT_EQ(classify_approval(make_decl("delete_branch")), ApprovalClass::ExecCapable);
    EXPECT_EQ(classify_approval(make_decl("exec_shell")),    ApprovalClass::ExecCapable);
    EXPECT_EQ(classify_approval(make_decl("run_pipeline")),  ApprovalClass::ExecCapable);
    EXPECT_EQ(classify_approval(make_decl("create_session")), ApprovalClass::ExecCapable);
    EXPECT_EQ(classify_approval(make_decl("")),               ApprovalClass::ExecCapable);
}

// ---------------------------------------------------------------------------
// Scope override beats the name-based heuristic
// ---------------------------------------------------------------------------
TEST(ApprovalClassifierTest, AdminScopeForcesControlPlane) {
    EXPECT_EQ(classify_approval(make_decl("list_sessions"), "admin:sessions"),
              ApprovalClass::ControlPlane);
    EXPECT_EQ(classify_approval(make_decl("get_policy"), "policy:read"),
              ApprovalClass::ControlPlane);
    EXPECT_EQ(classify_approval(make_decl("query_runtime"), "control:agent"),
              ApprovalClass::ControlPlane);
}

// ---------------------------------------------------------------------------
// Empty / benign scope does NOT promote to ControlPlane
// ---------------------------------------------------------------------------
TEST(ApprovalClassifierTest, BenignScopeKeepsNameClassification) {
    EXPECT_EQ(classify_approval(make_decl("write_file"), "fs:write"),
              ApprovalClass::ExecCapable);
    EXPECT_EQ(classify_approval(make_decl("list_files"), "fs:read"),
              ApprovalClass::Readonly);
    EXPECT_EQ(classify_approval(make_decl("list_files"), ""),
              ApprovalClass::Readonly);
}

// ---------------------------------------------------------------------------
// Stable string rendering for telemetry
// ---------------------------------------------------------------------------
TEST(ApprovalClassifierTest, NameRendersStableLowerCase) {
    EXPECT_EQ(approval_class_name(ApprovalClass::Readonly),     "readonly");
    EXPECT_EQ(approval_class_name(ApprovalClass::ExecCapable),  "exec_capable");
    EXPECT_EQ(approval_class_name(ApprovalClass::ControlPlane), "control_plane");
}

} // namespace
} // namespace euxis::runtime
