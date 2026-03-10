#pragma once

#include <string>

namespace euxis::runtime {

/**
 * @brief Error categories for the agent runtime.
 */
enum class RuntimeError {
    SessionNotFound,    ///< Requested session ID is missing from store.
    InvalidManifesto,   ///< Agent definition file is corrupt or invalid.
    ProviderError,      ///< Backend LLM provider (Ollama, etc.) failed.
    ValidatorFailed,    ///< Output did not match required JSON Schema.
    Timeout,            ///< Execution exceeded the allocated time.
};

/**
 * @brief Convert RuntimeError enum to human string.
 */
[[nodiscard]] auto to_string(RuntimeError err) -> std::string;

} // namespace euxis::runtime
