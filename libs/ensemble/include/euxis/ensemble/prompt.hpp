/// @file
/// @brief LLM-prompt builder for VoteRequest → text.
///
/// Real LLM-provider verifiers (Claude, OpenAI, Gemini) call this
/// to turn the in-memory VoteRequest into a deterministic prompt
/// string. The prompt is deliberately model-agnostic — every
/// provider can wrap the same text in its own message envelope.
///
/// The prompt template asks the model to return a strict JSON
/// object with `true_positive`, `confidence`, and `rationale`
/// fields so the response parser in response.hpp has a single
/// shape to handle.
///
/// Keeping prompt/response shapes here (not inside the runner)
/// lets the runner stay HTTP-free for tests, and lets real
/// providers ship as small additions that just wire up
/// network + auth.
#pragma once

#include <string>

#include "euxis/ensemble/verifier.hpp"

namespace euxis::ensemble {

struct PromptConfig {
    /// Friendly name surfaced inside the prompt. e.g. "euxis
    /// security reviewer". Empty by default.
    std::string reviewer_name;

    /// Maximum bytes from the request's snippet to include.
    /// Default 4096 — fits inside one Claude / GPT-5 prompt
    /// comfortably.
    std::size_t max_snippet_bytes{4096};
};

/// Build the model-agnostic prompt text for a VoteRequest.
[[nodiscard]] auto build_prompt(const VoteRequest& req,
                                const PromptConfig& cfg = {}) -> std::string;

} // namespace euxis::ensemble
