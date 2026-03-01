#include "euxis/inference/llama_engine.hpp"

#include <spdlog/spdlog.h>

namespace euxis::inference {

// ---------------------------------------------------------------------------
// Impl — PIMPL internals  (stub: no real llama.cpp linkage)
// ---------------------------------------------------------------------------
struct LlamaEngine::Impl {
    LocalModelConfig config;
    bool loaded = false;

    explicit Impl(const LocalModelConfig& cfg) : config(cfg) {
        // In a real build with llama.cpp linked, this would call
        // llama_backend_init(), llama_model_load(), etc.
        spdlog::info("LlamaEngine stub: config for model '{}'",
                     cfg.model_name);
    }

    ~Impl() {
        // Would call llama_free() / llama_backend_free() here.
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
};

// ---------------------------------------------------------------------------
// constructors / assignment / destructor
// ---------------------------------------------------------------------------
LlamaEngine::LlamaEngine(const LocalModelConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

LlamaEngine::~LlamaEngine() = default;

LlamaEngine::LlamaEngine(LlamaEngine&&) noexcept = default;
LlamaEngine& LlamaEngine::operator=(LlamaEngine&&) noexcept = default;

// ---------------------------------------------------------------------------
// generate — stub returns error
// ---------------------------------------------------------------------------
auto LlamaEngine::generate(std::string_view /*prompt*/,
                            uint32_t /*max_tokens*/)
    -> std::expected<InferenceResult, std::string> {
    return std::unexpected(std::string("llama.cpp not linked"));
}

// ---------------------------------------------------------------------------
// supports_model — check against config
// ---------------------------------------------------------------------------
auto LlamaEngine::supports_model(std::string_view name) -> bool {
    return impl_->config.model_name == name;
}

// ---------------------------------------------------------------------------
// health
// ---------------------------------------------------------------------------
auto LlamaEngine::health() -> nlohmann::json {
    return nlohmann::json{
        {"engine",  "llama.cpp"},
        {"status",  "stub"},
        {"loaded",  impl_->loaded},
        {"model",   impl_->config.model_name},
        {"context_size", impl_->config.context_size},
    };
}

} // namespace euxis::inference
