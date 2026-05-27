/// @file
/// @brief Implementation of Anthropic prompt-cache helpers.

#include "euxis/inference/cache_control.hpp"

#include <cstddef>
#include <string>

namespace euxis::inference {

namespace {

[[nodiscard]] auto has_cache_control(const nlohmann::json& block) -> bool {
    return block.is_object() && block.contains("cache_control");
}

// Normalise a message's `content` to a JSON array so we can address its
// last block without ambiguity. A string body becomes
//   [{"type": "text", "text": "<body>"}].
// A null/missing body becomes an empty array. An object becomes a
// single-element array containing that object. Existing arrays are left
// alone.
void normalise_content_to_array(nlohmann::json& message) {
    if (!message.contains("content")) {
        message["content"] = nlohmann::json::array();
        return;
    }
    auto& content = message["content"];
    if (content.is_array()) return;
    if (content.is_string()) {
        std::string text = content.get<std::string>();
        content = nlohmann::json::array();
        content.push_back({{"type", "text"}, {"text", std::move(text)}});
        return;
    }
    if (content.is_object()) {
        nlohmann::json wrapped = std::move(content);
        content = nlohmann::json::array();
        content.push_back(std::move(wrapped));
        return;
    }
    // Null or anything else collapses to empty.
    content = nlohmann::json::array();
}

// Attach a cache_control marker to the last block of @p message's content.
// Returns true if a marker was added (false if the message has no content
// blocks, the last block is non-object, or a marker is already present).
[[nodiscard]] auto mark_last_block(nlohmann::json& message,
                                   std::string_view ttl) -> bool {
    normalise_content_to_array(message);
    auto& content = message["content"];
    if (content.empty()) return false;

    auto& last = content.back();
    if (!last.is_object()) return false;
    if (has_cache_control(last)) return false;  // idempotent

    last["cache_control"] = {
        {"type", "ephemeral"},
        {"ttl",  std::string{ttl}},
    };
    return true;
}

// Find the index of the last message with role @p role, or SIZE_MAX
// if none. Linear scan from the back; messages arrays are short.
[[nodiscard]] auto last_index_with_role(const nlohmann::json& messages,
                                        std::string_view role) -> std::size_t {
    if (!messages.is_array()) return static_cast<std::size_t>(-1);
    for (std::size_t i = messages.size(); i-- > 0; ) {
        const auto& m = messages[i];
        if (m.is_object() && m.value("role", "") == role) {
            return i;
        }
    }
    return static_cast<std::size_t>(-1);
}

} // namespace

auto apply_anthropic_cache_control(nlohmann::json& messages,
                                   std::string_view ttl) -> std::size_t {
    if (!messages.is_array() || messages.empty()) return 0;

    std::size_t added = 0;

    // 1. Mark the last system message, if any.
    if (auto idx = last_index_with_role(messages, "system");
        idx != static_cast<std::size_t>(-1)) {
        if (mark_last_block(messages[idx], ttl)) ++added;
    }

    // 2. Mark the last user message, if any.
    if (auto idx = last_index_with_role(messages, "user");
        idx != static_cast<std::size_t>(-1)) {
        if (mark_last_block(messages[idx], ttl)) ++added;
    }

    return added;
}

auto with_anthropic_cache_control(nlohmann::json messages, std::string_view ttl)
    -> nlohmann::json {
    (void)apply_anthropic_cache_control(messages, ttl);
    return messages;
}

} // namespace euxis::inference
