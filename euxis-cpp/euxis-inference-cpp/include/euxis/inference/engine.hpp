#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

/** @namespace euxis::inference
 * @brief LLM engine implementations and model orchestration.
 */
namespace euxis::inference {

/**
 * @brief Abstract interface for low-level LLM inference engines.
 */
class IInferenceEngine {
public:
    virtual ~IInferenceEngine() = default;

    /**
     * @brief Generate tokens for a given prompt.
     * @param model Model name or path.
     * @param prompt Text instructions.
     * @param params Sampling parameters.
     * @return std::string The generated text.
     */
    virtual auto generate(const std::string& model, 
                          const std::string& prompt, 
                          const nlohmann::json& params = {}) -> std::string = 0;
};

} // namespace euxis::inference
