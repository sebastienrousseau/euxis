#include <gtest/gtest.h>

#include <string>

#include "euxis/a2a/message.hpp"

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
