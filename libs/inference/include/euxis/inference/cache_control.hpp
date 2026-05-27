/// @file
/// @brief Anthropic-style prompt cache control helpers.
///
/// Anthropic's Messages API supports prompt caching by attaching a
/// `cache_control` marker (`{"type": "ephemeral", "ttl": "5m"}`) to the
/// *last content block* of a message. Anthropic then caches the prefix
/// up to and including the marked block. Up to four markers are allowed
/// per request.
///
/// Mirrors the Hermes Agent `agent/prompt_caching.py` pattern: the
/// discipline is to apply markers deterministically — same byte layout
/// across calls — so the cache stays warm across turns. Multi-turn
/// reviews can see ~75% input-cost reduction on Anthropic when this is
/// applied consistently.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

/** @namespace euxis::inference */
namespace euxis::inference {

/// @brief Default cache TTL (Anthropic supports "5m" and "1h").
inline constexpr std::string_view kDefaultCacheTtl = "5m";

/// @brief Maximum number of cache breakpoints per request (Anthropic limit).
inline constexpr std::size_t kMaxCacheBreakpoints = 4;

/// @brief Apply Anthropic prompt-cache markers to a messages array.
///
/// Strategy (matches Hermes `apply_anthropic_cache_control`):
///   1. If the array contains a `system` role message, mark the last
///      content block of the *most recent* system message.
///   2. Mark the last content block of the *most recent* user message.
///
/// This places markers at the two highest-value cache boundaries: the
/// system prompt (rarely changes) and the rolling user-context tail
/// (changes every turn but bounded). Two of the four allowed breakpoints
/// are intentionally left unused so callers can add tool-specific
/// markers without exceeding the cap.
///
/// Idempotent: existing `cache_control` markers are not duplicated. If
/// the input is already well-formed it round-trips unchanged.
///
/// String content is normalised into a single-element content array
/// (`[{"type": "text", "text": "..."}]`) so the marker has a definite
/// content block to attach to — required by the Messages API schema.
///
/// @param messages Messages array to mark. Modified in place.
/// @param ttl Cache TTL string ("5m" or "1h"); defaults to "5m".
/// @return Number of cache_control markers added (0..2).
auto apply_anthropic_cache_control(nlohmann::json& messages,
                                   std::string_view ttl = kDefaultCacheTtl)
    -> std::size_t;

/// @brief Functional variant — returns a marked copy, leaving input intact.
[[nodiscard]] auto with_anthropic_cache_control(nlohmann::json messages,
                                                std::string_view ttl = kDefaultCacheTtl)
    -> nlohmann::json;

} // namespace euxis::inference
