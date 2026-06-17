/// @file
/// @brief Parse an LLM response into a VoteOutcome.
///
/// The prompt builder (prompt.hpp) instructs the model to emit a
/// strict JSON object with `true_positive`, `confidence`, and
/// `rationale`. Real-world responses are messier — models wrap the
/// JSON in Markdown code fences, prepend disclaimers, or emit
/// trailing prose. `parse_response` tolerates these patterns
/// without compromising on the shape contract.
#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "euxis/ensemble/verifier.hpp"

namespace euxis::ensemble {

struct ParseError {
    std::string message;
};

/// Parse an LLM response into a `VoteOutcome`. `provider_id` is
/// stamped into the resulting `EnsembleVote.provider` field.
///
/// The parser:
///   1. Strips any leading prose / Markdown code fences.
///   2. Locates the first top-level JSON object.
///   3. Reads the three required fields with sensible coercion
///      (e.g. accepts both `true` and the string `"true"`).
///   4. Clamps confidence to [0, 1].
///   5. Truncates rationale to 200 chars.
[[nodiscard]] auto parse_response(std::string_view body,
                                  std::string provider_id)
    -> std::expected<VoteOutcome, ParseError>;

} // namespace euxis::ensemble
