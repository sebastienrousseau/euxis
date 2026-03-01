#pragma once

#include <string_view>

namespace euxis::inference {

/// Scores returned by the quality gate.
struct QualityScore {
    float coherence;         // 0.0-1.0
    float relevance;         // 0.0-1.0
    float repetition_ratio;  // 0.0-1.0 (lower is better)
    bool passed;
};

/// Lightweight heuristic quality gate for local inference output.
///
/// - Coherence: ratio of sentences with >3 words.
/// - Relevance: word-level Jaccard similarity between prompt and response.
/// - Repetition: ratio of repeated bigrams to total bigrams.
class QualityGate {
public:
    explicit QualityGate(float min_coherence = 0.3f,
                         float max_repetition = 0.5f);

    /// Evaluate a prompt/response pair and return a quality score.
    [[nodiscard]] auto evaluate(std::string_view prompt,
                                std::string_view response) -> QualityScore;

private:
    float min_coherence_;
    float max_repetition_;

    auto compute_coherence(std::string_view text) -> float;
    auto compute_relevance(std::string_view prompt,
                           std::string_view response) -> float;
    auto compute_repetition(std::string_view text) -> float;
};

} // namespace euxis::inference
