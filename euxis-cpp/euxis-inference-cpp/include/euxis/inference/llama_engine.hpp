#pragma once

#include <memory>
#include <string_view>

#include "config.hpp"
#include "engine.hpp"

namespace euxis::inference {

/// llama.cpp-backed inference engine (PIMPL).
///
/// The current implementation is a **stub** that compiles without the llama.cpp
/// library linked.  `generate()` returns an error string `"llama.cpp not linked"`.
/// When the real llama.cpp dependency is available, swap in the concrete Impl.
class LlamaEngine final : public InferenceEngine {
public:
    explicit LlamaEngine(const LocalModelConfig& config);
    ~LlamaEngine() override;

    LlamaEngine(const LlamaEngine&) = delete;
    LlamaEngine& operator=(const LlamaEngine&) = delete;

    LlamaEngine(LlamaEngine&&) noexcept;
    LlamaEngine& operator=(LlamaEngine&&) noexcept;

    [[nodiscard]] auto generate(std::string_view prompt,
                                uint32_t max_tokens = 512)
        -> std::expected<InferenceResult, std::string> override;

    [[nodiscard]] auto supports_model(std::string_view name) -> bool override;
    [[nodiscard]] auto health() -> nlohmann::json override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace euxis::inference
