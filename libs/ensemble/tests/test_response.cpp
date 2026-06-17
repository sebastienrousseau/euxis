#include <gtest/gtest.h>

#include "euxis/ensemble/response.hpp"

namespace euxis::ensemble {
namespace {

TEST(Response, ParsesCleanJson) {
    auto out = parse_response(
        R"({"true_positive": true, "confidence": 0.92, "rationale": "tainted"})",
        "test");
    ASSERT_TRUE(out.has_value()) << (out ? "" : out.error().message);
    EXPECT_TRUE(out->vote.true_positive);
    EXPECT_FLOAT_EQ(out->vote.confidence, 0.92F);
    EXPECT_EQ(out->rationale, "tainted");
    EXPECT_EQ(out->vote.provider, "test");
}

TEST(Response, StripsLeadingProse) {
    auto out = parse_response(
        "Here is my analysis: {\"true_positive\": false, \"confidence\": 0.1, "
        "\"rationale\": \"benign\"}",
        "test");
    ASSERT_TRUE(out.has_value());
    EXPECT_FALSE(out->vote.true_positive);
}

TEST(Response, StripsMarkdownCodeFence) {
    auto out = parse_response(
        "```json\n{\"true_positive\": true, \"confidence\": 0.7, "
        "\"rationale\": \"likely\"}\n```",
        "test");
    ASSERT_TRUE(out.has_value());
    EXPECT_TRUE(out->vote.true_positive);
    EXPECT_FLOAT_EQ(out->vote.confidence, 0.7F);
}

TEST(Response, CoercesBooleanFromString) {
    auto out = parse_response(
        R"({"true_positive": "yes", "confidence": 0.5, "rationale": ""})",
        "test");
    ASSERT_TRUE(out.has_value());
    EXPECT_TRUE(out->vote.true_positive);
}

TEST(Response, ClampsConfidenceToZeroToOne) {
    auto over = parse_response(
        R"({"true_positive": true, "confidence": 5.0, "rationale": "x"})",
        "test");
    ASSERT_TRUE(over.has_value());
    EXPECT_FLOAT_EQ(over->vote.confidence, 1.0F);

    auto under = parse_response(
        R"({"true_positive": true, "confidence": -2.0, "rationale": "x"})",
        "test");
    ASSERT_TRUE(under.has_value());
    EXPECT_FLOAT_EQ(under->vote.confidence, 0.0F);
}

TEST(Response, RationaleTruncatedAt200Chars) {
    std::string body = R"({"true_positive": true, "confidence": 0.5, "rationale": ")"
                       + std::string(300, 'A') + "\"}";
    auto out = parse_response(body, "test");
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->rationale.size(), 200U);
}

TEST(Response, MissingTruePositiveIsAnError) {
    auto out = parse_response(R"({"confidence": 0.5, "rationale": "x"})", "test");
    EXPECT_FALSE(out.has_value());
}

TEST(Response, MissingConfidenceDefaultsToZero) {
    auto out = parse_response(
        R"({"true_positive": true, "rationale": "x"})", "test");
    ASSERT_TRUE(out.has_value());
    EXPECT_FLOAT_EQ(out->vote.confidence, 0.0F);
}

TEST(Response, EmptyBodyIsAnError) {
    auto out = parse_response("", "test");
    EXPECT_FALSE(out.has_value());
}

TEST(Response, MalformedJsonIsAnError) {
    auto out = parse_response(R"({"true_positive": tru)", "test");
    EXPECT_FALSE(out.has_value());
}

TEST(Response, NestedJsonIsHandled) {
    auto out = parse_response(
        R"({"true_positive": true, "confidence": 0.6, "rationale": "x", )"
        R"("extra": {"nested": "ok"}})",
        "test");
    ASSERT_TRUE(out.has_value());
    EXPECT_TRUE(out->vote.true_positive);
}

} // namespace
} // namespace euxis::ensemble
