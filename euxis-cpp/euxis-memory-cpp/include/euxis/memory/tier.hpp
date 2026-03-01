#pragma once

#include <cstdint>
#include <string_view>

namespace euxis::memory {

enum class MemoryTier : uint8_t {
    Hot,
    Relevant,
    CrossAgent
};

[[nodiscard]] constexpr auto tier_label(MemoryTier t) -> std::string_view {
    switch (t) {
        case MemoryTier::Hot:        return "hot";
        case MemoryTier::Relevant:   return "relevant";
        case MemoryTier::CrossAgent: return "cross_agent";
    }
    return "unknown";
}

} // namespace euxis::memory
