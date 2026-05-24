/// @file
/// @brief Automated quality evaluation for LLM responses.
#pragma once

#include <string_view>
#include <vector>

namespace euxis::inference {

/// @brief Scorecard representing the quality metrics of a response.
struct QualityScore {
    float coherence = 0.0F;
    float relevance = 0.0F;
    float repetition_ratio = 0.0F;
    bool passed = false;
};

/// @brief Evaluates LLM responses against safety and quality standards.
class QualityGate {
public:
    /// @brief Construct gate with specific thresholds.
    explicit QualityGate(float min_coherence = 0.5f,
                         float max_repetition = 0.3f);

    /// @brief Run full quality evaluation on a prompt/response pair.
    auto evaluate(std::string_view prompt,
                  std::string_view response) -> QualityScore;

    auto compute_coherence(std::string_view text) -> float;
    auto compute_relevance(std::string_view prompt,
                           std::string_view response) -> float;
    auto compute_repetition(std::string_view text) -> float;

private:
    float min_coherence_;
    float max_repetition_;
};

} // namespace euxis::inference
