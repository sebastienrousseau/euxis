#include <gtest/gtest.h>

#include "euxis/inference/quality_gate.hpp"

namespace euxis::inference {
namespace {

// ---------------------------------------------------------------------------
// Coherent text passes the gate
// ---------------------------------------------------------------------------
TEST(QualityGateTest, CoherentTextPasses) {
    QualityGate gate(0.3f, 0.5f);

    auto score = gate.evaluate(
        "What is the capital of France?",
        "The capital of France is Paris. It is a beautiful city known for "
        "its art and culture. The Eiffel Tower is one of its most famous "
        "landmarks.");

    EXPECT_GE(score.coherence, 0.3f);
    EXPECT_LE(score.repetition_ratio, 0.5f);
    EXPECT_TRUE(score.passed);
}

// ---------------------------------------------------------------------------
// Gibberish text fails coherence
// ---------------------------------------------------------------------------
TEST(QualityGateTest, GibberishFails) {
    QualityGate gate(0.5f, 0.5f);

    // Very short "sentences" separated by periods — each with <= 3 words
    auto score = gate.evaluate(
        "Tell me about science.",
        "Ah. Oh. Hmm. Yes. No. Ok.");

    EXPECT_LT(score.coherence, 0.5f);
    EXPECT_FALSE(score.passed);
}

// ---------------------------------------------------------------------------
// Repetitive text is flagged
// ---------------------------------------------------------------------------
TEST(QualityGateTest, RepetitiveTextFlagged) {
    QualityGate gate(0.0f, 0.3f); // low coherence threshold, strict repetition

    // Highly repetitive bigrams
    auto score = gate.evaluate(
        "Say something.",
        "the cat the cat the cat the cat the cat the cat the cat the cat.");

    EXPECT_GT(score.repetition_ratio, 0.3f);
    EXPECT_FALSE(score.passed);
}

// ---------------------------------------------------------------------------
// Relevance: overlapping words produce higher score
// ---------------------------------------------------------------------------
TEST(QualityGateTest, RelevanceWithOverlap) {
    QualityGate gate;

    auto high = gate.evaluate(
        "What is machine learning?",
        "Machine learning is a subset of artificial intelligence that "
        "involves learning from data.");

    auto low = gate.evaluate(
        "What is machine learning?",
        "The weather today is sunny with a chance of rain in the "
        "afternoon.");

    EXPECT_GT(high.relevance, low.relevance);
}

// ---------------------------------------------------------------------------
// Empty response
// ---------------------------------------------------------------------------
TEST(QualityGateTest, EmptyResponse) {
    QualityGate gate;
    auto score = gate.evaluate("Hello?", "");

    EXPECT_FLOAT_EQ(score.coherence, 0.0f);
    EXPECT_FLOAT_EQ(score.repetition_ratio, 0.0f);
    EXPECT_FLOAT_EQ(score.relevance, 0.0f);
}

// ---------------------------------------------------------------------------
// Single long sentence scores coherence 1.0
// ---------------------------------------------------------------------------
TEST(QualityGateTest, SingleLongSentenceIsCoherent) {
    QualityGate gate(0.3f, 0.5f);

    auto score = gate.evaluate(
        "prompt",
        "This is a single long sentence with many words that should be "
        "considered coherent by the heuristic.");

    EXPECT_FLOAT_EQ(score.coherence, 1.0f);
}

// ---------------------------------------------------------------------------
// Scores are in [0, 1]
// ---------------------------------------------------------------------------
TEST(QualityGateTest, ScoresInRange) {
    QualityGate gate;

    auto score = gate.evaluate(
        "a b c d e f",
        "a b c d e f g h i j k l m n o p.");

    EXPECT_GE(score.coherence, 0.0f);
    EXPECT_LE(score.coherence, 1.0f);
    EXPECT_GE(score.relevance, 0.0f);
    EXPECT_LE(score.relevance, 1.0f);
    EXPECT_GE(score.repetition_ratio, 0.0f);
    EXPECT_LE(score.repetition_ratio, 1.0f);
}

// --- Coverage: lines 67-77 (split_sentences with trailing text, no punctuation) ---
TEST(QualityGateTest, TextWithoutPunctuation) {
    QualityGate gate(0.0f, 1.0f);
    auto score = gate.evaluate(
        "prompt",
        "This is a sentence without any terminal punctuation marks at all");
    // Without '.', '!', '?' the whole text is one "sentence"
    EXPECT_GE(score.coherence, 0.0f);
    EXPECT_LE(score.coherence, 1.0f);
}

// --- Coverage: line 112 (compute_relevance: both prompts empty -> relevance=1.0) ---
TEST(QualityGateTest, BothEmptyRelevanceIsOne) {
    QualityGate gate(0.0f, 1.0f);
    // Both prompt and response empty -> relevance = 1.0
    auto score = gate.evaluate("", "");
    EXPECT_FLOAT_EQ(score.relevance, 1.0f);
}

// --- Coverage: line 135 (compute_relevance: one empty, one not -> relevance=0.0) ---
TEST(QualityGateTest, OneEmptyRelevanceIsZero) {
    QualityGate gate(0.0f, 1.0f);
    auto score1 = gate.evaluate("hello world", "");
    EXPECT_FLOAT_EQ(score1.relevance, 0.0f);
    auto score2 = gate.evaluate("", "hello world");
    EXPECT_FLOAT_EQ(score2.relevance, 0.0f);
}

// --- Coverage: lines 67-77 (sentence trimming with leading whitespace) ---
TEST(QualityGateTest, SentencesWithLeadingWhitespace) {
    QualityGate gate(0.0f, 1.0f);
    auto score = gate.evaluate(
        "prompt",
        "  First sentence here.   Second one right there.  \n Third sentence too.");
    EXPECT_GE(score.coherence, 0.0f);
}

// --- Coverage: single word text (repetition = 0) ---
TEST(QualityGateTest, SingleWordRepetitionIsZero) {
    QualityGate gate(0.0f, 1.0f);
    auto score = gate.evaluate("prompt", "hello");
    EXPECT_FLOAT_EQ(score.repetition_ratio, 0.0f);
}

} // anonymous namespace
} // namespace euxis::inference
