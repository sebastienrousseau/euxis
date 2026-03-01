#include "euxis/cli/provider_executor.hpp"
#include "euxis/cli/process.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace euxis::cli {

ProviderExecutor::ProviderExecutor(const std::string& data_dir) : data_dir_(data_dir) {}

auto ProviderExecutor::execute(const ModelSelection& selection,
                                const std::string& prompt,
                                int timeout_seconds) -> ProviderResponse {
    auto start = std::chrono::steady_clock::now();

    ProviderResponse resp;
    if (selection.provider == "claude") {
        resp = execute_claude(selection.model, prompt, timeout_seconds);
    } else if (selection.provider == "ollama") {
        resp = execute_ollama(selection.model, prompt, timeout_seconds);
    } else if (selection.provider == "openai" || selection.provider == "gemini") {
        resp = execute_api(selection.provider, selection.model, prompt, timeout_seconds);
    } else {
        // Try the provider as a CLI command (goose, kiro-cli, etc.)
        resp = execute_claude(selection.model, prompt, timeout_seconds);
        if (!resp.success && Process::available(selection.provider)) {
            auto result = Process::run_with_input(selection.provider, {prompt}, "", timeout_seconds);
            resp.output = result.stdout_output;
            resp.error = result.stderr_output;
            resp.exit_code = result.exit_code;
            resp.success = result.exit_code == 0;
        }
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    resp.duration_ms = std::chrono::duration<double, std::milli>(elapsed).count();
    return resp;
}

auto ProviderExecutor::execute_claude(const std::string& model,
                                       const std::string& prompt,
                                       int timeout) -> ProviderResponse {
    if (!Process::available("claude")) {
        return {false, "", "claude CLI not found on PATH", 127, 0.0};
    }

    std::vector<std::string> args = {"-p", prompt, "--output-format", "text"};
    if (!model.empty()) {
        args.push_back("--model");
        args.push_back(model);
    }

    auto result = Process::run("claude", args, timeout);
    return {
        result.exit_code == 0,
        result.stdout_output,
        result.stderr_output,
        result.exit_code,
        0.0
    };
}

auto ProviderExecutor::execute_ollama(const std::string& model,
                                       const std::string& prompt,
                                       int timeout) -> ProviderResponse {
    if (!Process::available("ollama")) {
        return {false, "", "ollama not found on PATH", 127, 0.0};
    }

    std::string m = model.empty() ? "qwen3:32b" : model;
    auto result = Process::run_with_input("ollama", {"run", m}, prompt, timeout);
    return {
        result.exit_code == 0,
        result.stdout_output,
        result.stderr_output,
        result.exit_code,
        0.0
    };
}

auto ProviderExecutor::execute_api(const std::string& provider,
                                    const std::string& model,
                                    const std::string& prompt,
                                    int timeout) -> ProviderResponse {
    if (!Process::available("curl")) {
        return {false, "", "curl not found (required for API calls)", 127, 0.0};
    }

    std::string url;
    std::string auth_header;
    nlohmann::json body;

    if (provider == "openai") {
        const char* key = std::getenv("OPENAI_API_KEY");
        if (!key || !key[0]) {
            return {false, "", "OPENAI_API_KEY not set", 1, 0.0};
        }
        url = "https://api.openai.com/v1/chat/completions";
        auth_header = std::string("Authorization: Bearer ") + key;
        body["model"] = model.empty() ? "gpt-4o" : model;
        body["messages"] = nlohmann::json::array({{{"role", "user"}, {"content", prompt}}});
    } else if (provider == "gemini") {
        const char* key = std::getenv("GEMINI_API_KEY");
        if (!key || !key[0]) {
            key = std::getenv("GOOGLE_API_KEY");
        }
        if (!key || !key[0]) {
            return {false, "", "GEMINI_API_KEY not set", 1, 0.0};
        }
        std::string m = model.empty() ? "gemini-2.0-flash-lite" : model;
        url = "https://generativelanguage.googleapis.com/v1beta/models/" + m +
              ":generateContent?key=" + key;
        body["contents"] = nlohmann::json::array(
            {{{"parts", nlohmann::json::array({{{"text", prompt}}})}}});
    } else {
        return {false, "", "Unsupported API provider: " + provider, 1, 0.0};
    }

    std::string body_str = body.dump();
    std::vector<std::string> curl_args = {"-s", "-S", url, "-H", "Content-Type: application/json"};
    if (!auth_header.empty()) {
        curl_args.push_back("-H");
        curl_args.push_back(auth_header);
    }
    curl_args.push_back("-d");
    curl_args.push_back(body_str);

    auto result = Process::run("curl", curl_args, timeout);
    if (result.exit_code != 0) {
        return {false, "", "curl failed: " + result.stderr_output, result.exit_code, 0.0};
    }

    // Parse JSON response
    try {
        auto resp_json = nlohmann::json::parse(result.stdout_output);
        std::string content;
        if (provider == "openai") {
            content = resp_json["choices"][0]["message"]["content"].get<std::string>();
        } else if (provider == "gemini") {
            content = resp_json["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
        }
        return {true, content, "", 0, 0.0};
    } catch (const std::exception& e) {
        spdlog::warn("Failed to parse {} response: {}", provider, e.what());
        return {false, result.stdout_output, "JSON parse error: " + std::string(e.what()), 1, 0.0};
    }
}

auto ProviderExecutor::load_agent_prompt(const std::string& euxis_home,
                                          const std::string& prompt_path) -> std::string {
    if (prompt_path.empty()) return {};

    auto full_path = std::filesystem::path(euxis_home) / prompt_path;
    if (!std::filesystem::exists(full_path)) return {};

    std::ifstream f(full_path);
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    // Strip YAML frontmatter (--- ... ---)
    if (content.starts_with("---\n")) {
        auto end = content.find("\n---\n", 4);
        if (end != std::string::npos) {
            content = content.substr(end + 5);
        }
    }

    // Trim leading whitespace
    auto pos = content.find_first_not_of(" \t\n\r");
    if (pos != std::string::npos && pos > 0) {
        content = content.substr(pos);
    }

    return content;
}

auto ProviderExecutor::build_prompt(const std::string& system_prompt,
                                     const std::string& task,
                                     const std::string& context) -> std::string {
    std::ostringstream out;
    if (!system_prompt.empty()) {
        out << system_prompt << "\n\n";
    }
    if (!context.empty()) {
        out << "## Context\n" << context << "\n\n";
    }
    out << "## Task\n" << task;
    return out.str();
}

} // namespace euxis::cli
