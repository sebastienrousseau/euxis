#include "euxis/inference/config.hpp"

namespace euxis::inference {

// ---------------------------------------------------------------------------
// from_json
// ---------------------------------------------------------------------------
auto LocalModelConfig::from_json(const nlohmann::json& j)
    -> LocalModelConfig {
    LocalModelConfig cfg;

    if (j.contains("model_name")) {
        cfg.model_name = j.at("model_name").get<std::string>();
    }
    if (j.contains("model_path")) {
        cfg.model_path =
            std::filesystem::path(j.at("model_path").get<std::string>());
    }
    if (j.contains("context_size")) {
        cfg.context_size = j.at("context_size").get<uint32_t>();
    }
    if (j.contains("gpu_layers")) {
        cfg.gpu_layers = j.at("gpu_layers").get<uint32_t>();
    }
    if (j.contains("threads")) {
        cfg.threads = j.at("threads").get<uint32_t>();
    }
    if (j.contains("temperature")) {
        cfg.temperature = j.at("temperature").get<float>();
    }
    if (j.contains("top_p")) {
        cfg.top_p = j.at("top_p").get<float>();
    }
    if (j.contains("expected_sha256")) {
        cfg.expected_sha256 = j.at("expected_sha256").get<std::string>();
    }

    return cfg;
}

// ---------------------------------------------------------------------------
// to_json
// ---------------------------------------------------------------------------
nlohmann::json LocalModelConfig::to_json() const {
    return nlohmann::json{
        {"model_name",     model_name},
        {"model_path",     model_path.string()},
        {"context_size",   context_size},
        {"gpu_layers",     gpu_layers},
        {"threads",        threads},
        {"temperature",    temperature},
        {"top_p",          top_p},
        {"expected_sha256", expected_sha256},
    };
}

} // namespace euxis::inference
