#pragma once

#include <expected>
#include <string>

namespace euxis::runtime {

struct ModelSpec {
    std::string provider;
    std::string model;
    std::string tier;
    double estimated_cost_per_1m{0.0};
};

struct ProviderResult {
    bool success{false};
    std::string output;
    std::string error;
    double duration_ms{0.0};
};

/// Abstract provider interface for model routing and execution.
class IProvider {
public:
    virtual ~IProvider() = default;

    virtual auto route(const std::string& agent_tier,
                       const std::string& task) -> ModelSpec = 0;

    virtual auto execute(const ModelSpec& spec,
                         const std::string& prompt,
                         int timeout_seconds = 120) -> ProviderResult = 0;

    virtual auto switch_model(const ModelSpec& new_spec) -> bool = 0;
};

} // namespace euxis::runtime
