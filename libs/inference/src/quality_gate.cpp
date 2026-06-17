#include "euxis/inference/quality_gate.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace euxis::inference {

// ---------------------------------------------------------------------------
// constructor
// ---------------------------------------------------------------------------
QualityGate::QualityGate(float min_coherence, float max_repetition)
    : min_coherence_(min_coherence), max_repetition_(max_repetition) {}

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
namespace {

/// Split text into whitespace-delimited tokens, lowercased.
auto tokenize(std::string_view text) -> std::vector<std::string> {
    std::vector<std::string> tokens;
    std::string word;
    for (char c : text) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!word.empty()) {
                tokens.push_back(word);
                word.clear();
            }
        } else {
            word += static_cast<char>(std::tolower(
                static_cast<unsigned char>(c)));
        }
    }
    if (!word.empty()) {
        tokens.push_back(word);
    }
    return tokens;
}

/// Split text into sentences (delimited by '.', '!', '?').
auto split_sentences(std::string_view text) -> std::vector<std::string> {
    std::vector<std::string> sentences;
    std::string current;
    for (char c : text) {
        current += c;
        if (c == '.' || c == '!' || c == '?') {
            // Trim leading whitespace
            size_t start = 0;
            while (start < current.size() &&
                   (current[start] == ' ' || current[start] == '\n' ||
                    current[start] == '\r' || current[start] == '\t')) {
                ++start;
            }
            auto trimmed = current.substr(start);
            if (!trimmed.empty()) {
                sentences.push_back(std::move(trimmed));
            }
            current.clear();
        }
    }
    // Remaining text counts as a sentence if non-empty after trim
    if (!current.empty()) {
        size_t start = 0;
        while (start < current.size() &&
               (current[start] == ' ' || current[start] == '\n' ||
                current[start] == '\r' || current[start] == '\t')) {
            ++start;
        }
        auto trimmed = current.substr(start);
        if (!trimmed.empty()) {
            sentences.push_back(std::move(trimmed));
        }
    }
    return sentences;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// compute_coherence — ratio of sentences with >3 words
// ---------------------------------------------------------------------------
auto QualityGate::compute_coherence(std::string_view text) -> float {
    auto sentences = split_sentences(text);
    if (sentences.empty()) {
        return 0.0f;
    }

    size_t coherent = 0;
    for (const auto& s : sentences) {
        auto words = tokenize(s);
        if (words.size() > 3) {
            ++coherent;
        }
    }
    return static_cast<float>(coherent) /
           static_cast<float>(sentences.size());
}

// ---------------------------------------------------------------------------
// compute_relevance — Jaccard similarity of word sets
// ---------------------------------------------------------------------------
auto QualityGate::compute_relevance(std::string_view prompt,
                                    std::string_view response) -> float {
    auto prompt_tokens   = tokenize(prompt);
    auto response_tokens = tokenize(response);

    if (prompt_tokens.empty() && response_tokens.empty()) {
        return 1.0f;
    }
    if (prompt_tokens.empty() || response_tokens.empty()) {
        return 0.0f;
    }

    std::unordered_set<std::string> prompt_set(prompt_tokens.begin(),
                                                prompt_tokens.end());
    std::unordered_set<std::string> response_set(response_tokens.begin(),
                                                  response_tokens.end());

    size_t intersection = 0;
    for (const auto& w : prompt_set) {
        if (response_set.count(w)) {
            ++intersection;
        }
    }

    // Union = |A| + |B| - |A ∩ B|
    size_t union_size =
        prompt_set.size() + response_set.size() - intersection;

    if (union_size == 0) {
        return 0.0f;
    }

    return static_cast<float>(intersection) /
           static_cast<float>(union_size);
}

// ---------------------------------------------------------------------------
// compute_repetition — ratio of repeated bigrams to total bigrams
// ---------------------------------------------------------------------------
auto QualityGate::compute_repetition(std::string_view text) -> float {
    auto tokens = tokenize(text);
    if (tokens.size() < 2) {
        return 0.0f;
    }

    std::unordered_map<std::string, size_t> bigram_counts;
    size_t total_bigrams = tokens.size() - 1;

    for (size_t i = 0; i + 1 < tokens.size(); ++i) {
        std::string bigram = tokens[i] + " " + tokens[i + 1];
        bigram_counts[bigram]++;
    }

    // Count bigrams that appear more than once
    size_t repeated = 0;
    for (const auto& [bigram, count] : bigram_counts) {
        if (count > 1) {
            repeated += count; // all occurrences of repeated bigrams
        }
    }

    return static_cast<float>(repeated) /
           static_cast<float>(total_bigrams);
}

// ---------------------------------------------------------------------------
// evaluate
// ---------------------------------------------------------------------------
auto QualityGate::evaluate(std::string_view prompt,
                           std::string_view response) -> QualityScore {
    QualityScore score{};
    score.coherence        = compute_coherence(response);
    score.relevance        = compute_relevance(prompt, response);
    score.repetition_ratio = compute_repetition(response);
    score.passed           = (score.coherence >= min_coherence_) &&
                             (score.repetition_ratio <= max_repetition_);
    return score;
}

} // namespace euxis::inference
