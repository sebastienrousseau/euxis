/// @file
/// @brief CLI omnigraph
#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace euxis::cli {

class GraphAdapter {
public:
    static auto format_for_provider(const nlohmann::json& graph,
                                    const std::string& provider) -> std::string;

private:
    static auto format_system_prompt(const nlohmann::json& graph) -> std::string;
    static auto format_json(const nlohmann::json& graph) -> std::string;
    static auto format_minimal(const nlohmann::json& graph) -> std::string;
};

class TokenBudgeter {
public:
    explicit TokenBudgeter(int max_tokens = 100000);

    [[nodiscard]] auto estimate_tokens(const std::string& text) const -> int;
    auto optimize_graph(nlohmann::json graph,
                        const std::string& target_format) -> nlohmann::json;

private:
    int max_tokens_;
    static constexpr double kCharsPerToken = 4.0;
};

} // namespace euxis::cli
