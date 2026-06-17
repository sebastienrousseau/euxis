#include "euxis/cli/omnigraph.hpp"

#include <spdlog/spdlog.h>

namespace euxis::cli {

auto GraphAdapter::format_for_provider(const nlohmann::json& graph,
                                       const std::string& provider)
    -> std::string {
    auto p = provider;
    std::ranges::transform(p, p.begin(),
                           [](unsigned char c) { return std::tolower(c); });

    if (p == "claude" || p == "gemini" || p == "anthropic")
        return format_system_prompt(graph);
    if (p == "openai" || p == "gpt-4" || p == "o1")
        return format_json(graph);
    if (p == "ollama" || p == "llama" || p == "local")
        return format_minimal(graph);
    return format_system_prompt(graph);
}

auto GraphAdapter::format_system_prompt(const nlohmann::json& graph)
    -> std::string {
    std::string output = "# EUXIS OMNIGRAPH: WORKSPACE CONTEXT\n\n";

    // Materialise the temporary into a named local before iterating —
    // graph.value(...) returns by value, and .items() yields a view over
    // that temporary, which would expire at the end of the for-init
    // statement (GCC 14 -Werror=dangling-reference).
    const auto nodes_obj =
        graph.value("nodes", nlohmann::json::object());
    for (auto& [node, data] : nodes_obj.items()) {
        auto node_type = data.value("type", "unknown");
        output += "## " + node + " (" + node_type + ")\n";

        if (data.contains("classes") && data["classes"].is_array()) {
            output += "- Classes: ";
            for (size_t i = 0; i < data["classes"].size(); ++i) {
                if (i > 0) output += ", ";
                output += data["classes"][i].get<std::string>();
            }
            output += "\n";
        }
        if (data.contains("functions") && data["functions"].is_array()) {
            output += "- Functions: ";
            for (size_t i = 0; i < data["functions"].size(); ++i) {
                if (i > 0) output += ", ";
                output += data["functions"][i].get<std::string>();
            }
            output += "\n";
        }
    }

    output += "\n## RELATIONSHIPS\n";
    for (const auto& edge :
         graph.value("edges", nlohmann::json::array())) {
        output += "- " + edge.value("source", "") +
                  " --[" + edge.value("relation", "") + "]--> " +
                  edge.value("target", "") + "\n";
    }
    return output;
}

auto GraphAdapter::format_json(const nlohmann::json& graph) -> std::string {
    return graph.dump(2);
}

auto GraphAdapter::format_minimal(const nlohmann::json& graph) -> std::string {
    std::string output = "# Workspace Map\n\n";
    // Same dangling-reference fix as format_system_prompt above.
    const auto nodes_obj =
        graph.value("nodes", nlohmann::json::object());
    for (const auto& [node, _] : nodes_obj.items()) {
        output += "- " + node + "\n";
    }
    return output;
}

TokenBudgeter::TokenBudgeter(int max_tokens) : max_tokens_(max_tokens) {}

auto TokenBudgeter::estimate_tokens(const std::string& text) const -> int {
    return static_cast<int>(static_cast<double>(text.size()) / kCharsPerToken);
}

auto TokenBudgeter::optimize_graph(nlohmann::json graph,
                                   const std::string& /*target_format*/)
    -> nlohmann::json {
    auto raw = graph.dump();
    auto estimated = estimate_tokens(raw);
    spdlog::info("OmniGraph Estimated Token Cost: ~{} tokens", estimated);

    if (estimated > max_tokens_) {
        spdlog::warn("Graph exceeds budget ({} > {}). Pruning...",
                     estimated, max_tokens_);
        graph["edges"] = nlohmann::json::array();
        auto re_estimated = estimate_tokens(graph.dump());
        spdlog::info("Post-pruning cost: ~{} tokens", re_estimated);
    }
    return graph;
}

} // namespace euxis::cli
