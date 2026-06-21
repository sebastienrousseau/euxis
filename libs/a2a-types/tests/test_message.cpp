#include <gtest/gtest.h>

#include <string>

#include "euxis/a2a/message.hpp"

// NOLINTBEGIN(bugprone-unchecked-optional-access) — gtest ASSERT_TRUE
// guards are invisible to clang-tidy's dataflow; tests can blanket-
// disable per docs/development/clang-tidy-policy.md.

namespace euxis::a2a {
namespace {

// ---------------------------------------------------------------------------
// A2AMessage to_json / from_json roundtrip
// ---------------------------------------------------------------------------
TEST(MessageTest, MessageRoundtrip) {
    A2AMessage original;
    original.role = "user";
    original.created_at = "2026-03-01T12:00:00Z";

    ContentPart text_part;
    text_part.type = "text";
    text_part.content = "Hello, agent!";
    original.parts.push_back(std::move(text_part));

    ContentPart data_part;
    data_part.type = "data";
    data_part.content = "eyJmb28iOiAiYmFyIn0=";
    data_part.mime_type = "application/json";
    original.parts.push_back(std::move(data_part));

    const auto j = original.to_json();
    const auto restored = A2AMessage::from_json(j);

    EXPECT_EQ(restored.role, "user");
    EXPECT_EQ(restored.created_at, "2026-03-01T12:00:00Z");
    ASSERT_EQ(restored.parts.size(), 2u);

    EXPECT_EQ(restored.parts[0].type, "text");
    EXPECT_EQ(restored.parts[0].content, "Hello, agent!");
    EXPECT_FALSE(restored.parts[0].mime_type.has_value());

    EXPECT_EQ(restored.parts[1].type, "data");
    EXPECT_EQ(restored.parts[1].content, "eyJmb28iOiAiYmFyIn0=");
    ASSERT_TRUE(restored.parts[1].mime_type.has_value());
    EXPECT_EQ(*restored.parts[1].mime_type, "application/json");
}

// ---------------------------------------------------------------------------
// JSON keys use camelCase
// ---------------------------------------------------------------------------
TEST(MessageTest, JsonCamelCase) {
    A2AMessage msg;
    msg.role = "agent";
    msg.created_at = "2026-03-01T12:00:00Z";

    ContentPart part;
    part.type = "file";
    part.content = "base64data";
    part.mime_type = "image/png";
    msg.parts.push_back(std::move(part));

    const auto j = msg.to_json();

    EXPECT_TRUE(j.contains("createdAt"));
    EXPECT_FALSE(j.contains("created_at"));

    const auto& p = j["parts"][0];
    EXPECT_TRUE(p.contains("mimeType"));
    EXPECT_FALSE(p.contains("mime_type"));
}

// ---------------------------------------------------------------------------
// ContentPart types: text, data, file
// ---------------------------------------------------------------------------
TEST(MessageTest, ContentPartTypes) {
    // Text
    ContentPart text;
    text.type = "text";
    text.content = "plain text";
    EXPECT_EQ(text.type, "text");
    EXPECT_FALSE(text.mime_type.has_value());

    // Data
    ContentPart data;
    data.type = "data";
    data.content = "raw bytes";
    data.mime_type = "application/octet-stream";
    EXPECT_EQ(data.type, "data");
    ASSERT_TRUE(data.mime_type.has_value());
    EXPECT_EQ(*data.mime_type, "application/octet-stream");

    // File
    ContentPart file;
    file.type = "file";
    file.content = "aGVsbG8=";
    file.mime_type = "text/plain";
    EXPECT_EQ(file.type, "file");
    ASSERT_TRUE(file.mime_type.has_value());
}

// ---------------------------------------------------------------------------
// Empty message (no parts)
// ---------------------------------------------------------------------------
TEST(MessageTest, EmptyParts) {
    A2AMessage msg;
    msg.role = "user";
    msg.created_at = "2026-01-01T00:00:00Z";

    const auto j = msg.to_json();
    EXPECT_TRUE(j["parts"].empty());

    const auto restored = A2AMessage::from_json(j);
    EXPECT_TRUE(restored.parts.empty());
}

// ---------------------------------------------------------------------------
// Agent role message
// ---------------------------------------------------------------------------
TEST(MessageTest, AgentRole) {
    A2AMessage msg;
    msg.role = "agent";
    msg.created_at = "2026-03-01T15:30:00Z";

    ContentPart part;
    part.type = "text";
    part.content = "I have completed the task.";
    msg.parts.push_back(std::move(part));

    const auto j = msg.to_json();
    EXPECT_EQ(j["role"].get<std::string>(), "agent");
}

// ---------------------------------------------------------------------------
// Artifact to_json / from_json roundtrip
// ---------------------------------------------------------------------------
TEST(MessageTest, ArtifactRoundtrip) {
    Artifact original;
    original.name = "report.pdf";
    original.mime_type = "application/pdf";
    original.data = "JVBERi0xLjQK...";

    const auto j = original.to_json();
    const auto restored = Artifact::from_json(j);

    EXPECT_EQ(restored.name, "report.pdf");
    EXPECT_EQ(restored.mime_type, "application/pdf");
    EXPECT_EQ(restored.data, "JVBERi0xLjQK...");
}

// ---------------------------------------------------------------------------
// Artifact JSON keys are camelCase
// ---------------------------------------------------------------------------
TEST(MessageTest, ArtifactJsonCamelCase) {
    Artifact art;
    art.name = "image.png";
    art.mime_type = "image/png";
    art.data = "iVBORw0KGgo=";

    const auto j = art.to_json();

    EXPECT_TRUE(j.contains("mimeType"));
    EXPECT_FALSE(j.contains("mime_type"));
    EXPECT_TRUE(j.contains("name"));
    EXPECT_TRUE(j.contains("data"));
}

// ---------------------------------------------------------------------------
// BidRequest roundtrip
// ---------------------------------------------------------------------------
TEST(MessageTest, BidRequestRoundtrip) {
    BidRequest original;
    original.task_id              = "task-42";
    original.capability_required  = "code-review";
    original.max_latency_ms       = 250;

    const auto j = original.to_json();
    const auto restored = BidRequest::from_json(j);

    EXPECT_EQ(restored.task_id,             "task-42");
    EXPECT_EQ(restored.capability_required, "code-review");
    EXPECT_EQ(restored.max_latency_ms,      250);
}

// Documents the current convention: BidRequest uses *snake_case* JSON
// keys — inconsistent with A2AMessage / Artifact which use camelCase.
// Test pins the contract; changing the wire format requires updating
// every consumer at the same time.
TEST(MessageTest, BidRequestJsonUsesSnakeCase) {
    BidRequest req;
    req.task_id             = "t1";
    req.capability_required = "deep-audit";
    req.max_latency_ms      = 1500;

    const auto j = req.to_json();
    EXPECT_TRUE(j.contains("task_id"));
    EXPECT_TRUE(j.contains("capability_required"));
    EXPECT_TRUE(j.contains("max_latency_ms"));
}

TEST(MessageTest, BidRequestDefaultLatencyIsZero) {
    BidRequest req;
    req.task_id             = "task";
    req.capability_required = "any";
    EXPECT_EQ(req.max_latency_ms, 0);
}

// ---------------------------------------------------------------------------
// BidResponse roundtrip
// ---------------------------------------------------------------------------
TEST(MessageTest, BidResponseRoundtrip) {
    BidResponse original;
    original.task_id             = "task-42";
    original.agent_id            = "agent-alpha";
    original.estimated_cost      = 0.015;
    original.estimated_latency_ms = 120;
    original.accepted            = true;

    const auto j = original.to_json();
    const auto restored = BidResponse::from_json(j);

    EXPECT_EQ(restored.task_id,              "task-42");
    EXPECT_EQ(restored.agent_id,             "agent-alpha");
    EXPECT_DOUBLE_EQ(restored.estimated_cost, 0.015);
    EXPECT_EQ(restored.estimated_latency_ms, 120);
    EXPECT_TRUE(restored.accepted);
}

TEST(MessageTest, BidResponseRejectedIsRoundTripped) {
    BidResponse original;
    original.task_id  = "task-99";
    original.agent_id = "agent-busy";
    original.accepted = false;  // explicit rejection

    const auto j        = original.to_json();
    const auto restored = BidResponse::from_json(j);
    EXPECT_FALSE(restored.accepted);
}

// Same snake_case convention as BidRequest above.
TEST(MessageTest, BidResponseJsonUsesSnakeCase) {
    BidResponse resp;
    resp.task_id              = "t";
    resp.agent_id             = "a";
    resp.estimated_cost       = 0.25;
    resp.estimated_latency_ms = 75;
    resp.accepted             = true;

    const auto j = resp.to_json();
    EXPECT_TRUE(j.contains("task_id"));
    EXPECT_TRUE(j.contains("agent_id"));
    EXPECT_TRUE(j.contains("estimated_cost"));
    EXPECT_TRUE(j.contains("estimated_latency_ms"));
    EXPECT_TRUE(j.contains("accepted"));
}

TEST(MessageTest, BidResponseDefaultsAreSafe) {
    BidResponse resp;
    EXPECT_DOUBLE_EQ(resp.estimated_cost,       0.0);
    EXPECT_EQ(resp.estimated_latency_ms,        0);
    EXPECT_FALSE(resp.accepted);
}

// ---------------------------------------------------------------------------
// Multiple artifacts
// ---------------------------------------------------------------------------
TEST(MessageTest, MultipleArtifacts) {
    Artifact a1;
    a1.name = "file1.txt";
    a1.mime_type = "text/plain";
    a1.data = "aGVsbG8=";

    Artifact a2;
    a2.name = "file2.json";
    a2.mime_type = "application/json";
    a2.data = "eyJ0ZXN0IjogdHJ1ZX0=";

    // Roundtrip both
    const auto j1 = a1.to_json();
    const auto j2 = a2.to_json();
    const auto r1 = Artifact::from_json(j1);
    const auto r2 = Artifact::from_json(j2);

    EXPECT_EQ(r1.name, "file1.txt");
    EXPECT_EQ(r2.name, "file2.json");
    EXPECT_NE(r1.data, r2.data);
}

} // namespace
} // namespace euxis::a2a

// NOLINTEND(bugprone-unchecked-optional-access)
