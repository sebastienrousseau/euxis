#include "euxis/cli/provider_router.hpp"
#include "euxis/cli/i18n.hpp"
#include "euxis/cli/provider_executor.hpp"
#include "euxis/cli/process.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <spdlog/spdlog.h>

namespace euxis::cli {

using euxis::cli::i18n::tr;

auto tier_label(Tier t) -> std::string {
    switch (t) {
        case Tier::Routine: return "routine";
        case Tier::Data:    return "data";
        case Tier::Code:    return "code";
        case Tier::Reason:  return "reason";
    }
    return "code";
}

auto parse_tier(const std::string& s) -> Tier {
    if (s == "routine") return Tier::Routine;
    if (s == "data")    return Tier::Data;
    if (s == "reason")  return Tier::Reason;
    return Tier::Code;
}

// --- ProviderRouter ---

ProviderRouter::ProviderRouter(const std::string& data_dir)
    : data_dir_(data_dir) {
    // Defaults (Feb 2026 pricing)
    models_.routine = "gemini-2.0-flash-lite";
    models_.data    = "deepseek-v3";
    models_.code    = "claude-sonnet-4-6";
    models_.reason  = "claude-opus-4-6";

    load_config();
    load_env_overrides();
}

void ProviderRouter::load_config() {
    auto config_path = std::filesystem::path(data_dir_) / "config" / "router.json";
    if (!std::filesystem::exists(config_path)) return;

    try {
        std::ifstream f(config_path);
        config_ = nlohmann::json::parse(f);

        if (config_.contains("models")) {
            auto& m = config_["models"];
            if (m.contains("routine")) models_.routine = m["routine"].get<std::string>();
            if (m.contains("data"))    models_.data = m["data"].get<std::string>();
            if (m.contains("code"))    models_.code = m["code"].get<std::string>();
            if (m.contains("reason"))  models_.reason = m["reason"].get<std::string>();
        }
    } catch (const std::exception& e) {
        spdlog::warn("router.json parse error: {}", e.what());
    }
}

void ProviderRouter::load_env_overrides() {
    auto env_or = [](const char* name) -> std::optional<std::string> {
        const char* v = std::getenv(name);
        if (v && v[0]) return std::string(v);
        return std::nullopt;
    };

    if (auto v = env_or("EUXIS_MODEL_ROUTINE")) models_.routine = *v;
    if (auto v = env_or("EUXIS_MODEL_DATA"))    models_.data = *v;
    if (auto v = env_or("EUXIS_MODEL_CODE"))    models_.code = *v;
    if (auto v = env_or("EUXIS_MODEL_REASON"))  models_.reason = *v;
}

auto ProviderRouter::detect_provider() const -> std::string {
    // Check environment overrides
    const char* override_var = std::getenv("EUXIS_DEFAULT_PROVIDER");
    if (override_var && override_var[0]) return override_var;

    // Detect active provider sessions / auth tokens
    if (std::getenv("ANTHROPIC_API_KEY") ||
        !ProviderExecutor::resolve_anthropic_token().empty())
        return "anthropic";
    if (std::getenv("OPENAI_API_KEY"))
        return "openai";
    if (std::getenv("GEMINI_API_KEY") || std::getenv("GOOGLE_API_KEY"))
        return "gemini";

    // Check CLI availability
    static const std::vector<std::string> providers = {
        "claude", "gemini", "openai", "ollama", "goose"
    };
    for (const auto& p : providers) {
        if (Process::available(p)) {
            if (p == "claude") return "anthropic";
            return p;
        }
    }
    return "anthropic";  // default
}

auto ProviderRouter::select_model(Tier tier) const -> ModelSelection {
    // Check global override
    const char* override_model = std::getenv("EUXIS_MODEL_OVERRIDE");
    if (override_model && override_model[0]) {
        return {detect_provider(), override_model, tier, 0.0};
    }

    // Check local-only mode
    const char* local_only = std::getenv("EUXIS_LOCAL_ONLY");
    if (local_only && std::string_view(local_only) == "true" && local_available()) {
        const char* local_model = std::getenv("EUXIS_LOCAL_MODEL");
        return {"ollama", local_model ? local_model : "qwen3:32b", tier, 0.0};
    }

    ModelSelection sel;
    sel.provider = detect_provider();
    sel.tier = tier;

    switch (tier) {
        case Tier::Routine:
            sel.model = models_.routine;
            sel.estimated_cost_per_1m = 0.10;
            break;
        case Tier::Data:
            sel.model = models_.data;
            sel.estimated_cost_per_1m = 0.27;
            break;
        case Tier::Code:
            sel.model = models_.code;
            sel.estimated_cost_per_1m = 3.00;
            break;
        case Tier::Reason:
            sel.model = models_.reason;
            sel.estimated_cost_per_1m = 15.00;
            break;
    }
    return sel;
}

auto ProviderRouter::analyze_task_tier(const std::string& task) const -> Tier {
    // Keywords that suggest higher tiers
    static const std::vector<std::string> reason_keywords = {
        "architect", "design", "plan", "strategy", "complex", "critical",
        "security", "audit", "review"
    };
    static const std::vector<std::string> code_keywords = {
        "code", "implement", "refactor", "debug", "test", "build",
        "fix", "optimize", "develop"
    };
    static const std::vector<std::string> data_keywords = {
        "analyze", "extract", "parse", "summarize", "data", "report",
        "classify", "filter"
    };

    std::string lower = task;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    for (const auto& kw : reason_keywords) {
        if (lower.find(kw) != std::string::npos) return Tier::Reason;
    }
    for (const auto& kw : code_keywords) {
        if (lower.find(kw) != std::string::npos) return Tier::Code;
    }
    for (const auto& kw : data_keywords) {
        if (lower.find(kw) != std::string::npos) return Tier::Data;
    }
    return Tier::Routine;
}

auto ProviderRouter::route(const std::string& agent_tier,
                            const std::string& task) const -> ModelSelection {
    Tier at = parse_tier(agent_tier);
    Tier tt = analyze_task_tier(task);
    // Use the higher of agent tier and task tier
    Tier effective = std::max(at, tt);
    return select_model(effective);
}

auto ProviderRouter::local_available() const -> bool {
    return Process::available("ollama");
}

auto ProviderRouter::available_providers() const -> std::vector<std::string> {
    std::vector<std::string> result;
    static const std::vector<std::string> candidates = {
        "claude", "gemini", "openai", "ollama", "goose", "kiro-cli", "qwen", "crush"
    };
    for (const auto& p : candidates) {
        if (Process::available(p)) result.push_back(p);
    }
    return result;
}

void ProviderRouter::print_status() const {
    auto provider = detect_provider();
    std::cout << tr("Provider Router Status") << "\n"
              << "  " << tr("Active provider:") << " " << provider << "\n"
              << "  " << tr("Models:") << "\n"
              << "    routine: " << models_.routine << "\n"
              << "    data:    " << models_.data << "\n"
              << "    code:    " << models_.code << "\n"
              << "    reason:  " << models_.reason << "\n"
              << "  " << tr("Local (Ollama):") << " " << (local_available() ? tr("available") : tr("not found")) << "\n"
              << "  " << tr("Providers on PATH:") << " ";
    auto avail = available_providers();
    for (size_t i = 0; i < avail.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << avail[i];
    }
    std::cout << "\n";
}

auto ProviderRouter::model_fallback_chain(const std::string& model) const
    -> std::vector<ModelSelection> {
    std::vector<ModelSelection> chain;

    // Check config for explicit model fallback mappings
    if (config_.contains("model_fallback")) {
        auto& fb = config_["model_fallback"];
        if (fb.contains(model)) {
            for (const auto& entry : fb[model]) {
                ModelSelection sel;
                sel.provider = entry.value("provider", "");
                sel.model = entry.value("model", "");
                sel.tier = Tier::Code;
                if (!sel.provider.empty() && !sel.model.empty()) {
                    chain.push_back(sel);
                }
            }
            return chain;
        }
    }

    // Default fallback chains based on provider families
    // Anthropic models → OpenAI → Gemini → Ollama
    if (model.find("claude") != std::string::npos) {
        chain.push_back({"openai", "gpt-4o", Tier::Code, 0.0});
        chain.push_back({"gemini", "gemini-2.0-flash", Tier::Code, 0.0});
        chain.push_back({"ollama", "qwen3:32b", Tier::Code, 0.0});
    } else if (model.find("gpt") != std::string::npos) {
        chain.push_back({"anthropic", "claude-sonnet-4-6", Tier::Code, 0.0});
        chain.push_back({"gemini", "gemini-2.0-flash", Tier::Code, 0.0});
        chain.push_back({"ollama", "qwen3:32b", Tier::Code, 0.0});
    } else if (model.find("gemini") != std::string::npos) {
        chain.push_back({"anthropic", "claude-sonnet-4-6", Tier::Code, 0.0});
        chain.push_back({"openai", "gpt-4o", Tier::Code, 0.0});
        chain.push_back({"ollama", "qwen3:32b", Tier::Code, 0.0});
    } else {
        // Generic: try cloud providers
        chain.push_back({"anthropic", "claude-sonnet-4-6", Tier::Code, 0.0});
        chain.push_back({"openai", "gpt-4o", Tier::Code, 0.0});
        chain.push_back({"gemini", "gemini-2.0-flash", Tier::Code, 0.0});
    }

    return chain;
}

} // namespace euxis::cli
