#include "euxis/cli/provider_executor.hpp"
#include "euxis/cli/process.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace euxis::cli {

ProviderExecutor::ProviderExecutor(const std::string& data_dir)
    : data_dir_(data_dir), auth_store_(data_dir) {
    auth_store_.import_env_vars();
    auth_store_.auto_import_claude_oauth();
    auth_store_.auto_import_gemini_oauth();
}

auto ProviderExecutor::execute(const ModelSelection& selection,
                                const std::string& prompt,
                                int timeout_seconds,
                                std::optional<ResolvedAuth> auth,
                                std::function<void(const std::string&)> on_chunk) -> ProviderResponse {
    if (std::getenv("EUXIS_TEST_MOCK_EXECUTION")) {
        if (on_chunk) on_chunk("mocked response chunk");
        return {true, "mocked response for " + prompt, "", 0, 10.0, {}};
    }
    
    auto start = std::chrono::steady_clock::now();
    std::string effective_provider = selection.provider;
    if (effective_provider == "anthropic") effective_provider = "claude";

    if (!auth.has_value()) {
        auth = auth_store_.resolve_with_fallback(effective_provider);
    }

    ProviderResponse resp;
    if (effective_provider == "claude") {
        resp = execute_claude(selection.model, prompt, timeout_seconds, auth, on_chunk);
    } else if (effective_provider == "ollama") {
        resp = execute_ollama(selection.model, prompt, timeout_seconds, on_chunk);
    } else if (effective_provider == "openai" || effective_provider == "gemini") {
        resp = execute_api(effective_provider, selection.model, prompt, timeout_seconds, auth, on_chunk);
    } else {
        resp = {false, "", "Unknown provider: " + selection.provider, 1, 0.0, {}};
    }

    if (auth.has_value()) {
        if (resp.success) {
            auth_store_.report_success(auth->profile_id);
        } else if (resp.failure_reason.has_value()) {
            auth_store_.report_failure(auth->profile_id, *resp.failure_reason);
        }
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    resp.duration_ms = std::chrono::duration<double, std::milli>(elapsed).count();
    return resp;
}

auto ProviderExecutor::resolve_anthropic_token() -> std::string {
    const char* api_key = std::getenv("ANTHROPIC_API_KEY");
    if (api_key && api_key[0]) return api_key;

    const char* home = std::getenv("HOME");
    if (!home) return {};

    auto creds_path = std::filesystem::path(home) / ".claude" / ".credentials.json";
    if (!std::filesystem::exists(creds_path)) return {};

    try {
        std::ifstream f(creds_path);
        auto creds = nlohmann::json::parse(f);
        if (!creds.contains("claudeAiOauth")) return {};
        auto& oauth = creds["claudeAiOauth"];
        if (oauth.contains("accessToken")) return oauth["accessToken"].get<std::string>();
    } catch (...) {}
    return {};
}

auto ProviderExecutor::classify_error(int http_status, [[maybe_unused]] const std::string& body) -> std::optional<CooldownReason> {
    if (http_status == 429) return CooldownReason::RateLimit;
    if (http_status == 401 || http_status == 403) return CooldownReason::AuthError;
    return std::nullopt;
}

auto ProviderExecutor::execute_claude(const std::string& model,
                                       const std::string& prompt,
                                       int timeout,
                                       const std::optional<ResolvedAuth>& auth,
                                       std::function<void(const std::string&)> on_chunk) -> ProviderResponse {
    std::string token;
    bool is_oauth = false;
    
    if (auth.has_value()) {
        token = auth->token;
        is_oauth = auth->is_oauth;
    } else {
        token = resolve_anthropic_token();
        is_oauth = !token.empty();
    }

    if (is_oauth) {
        std::string claude_path = "/home/seb/.local/share/mise/installs/npm-anthropic-ai-claude-code/2.1.63/bin/claude";
        if (!std::filesystem::exists(claude_path)) claude_path = "claude";

        std::vector<std::string> args = {"-u", "CLAUDE_CODE_ENTRYPOINT", claude_path, "--model", "sonnet", "--permission-mode", "dontAsk", "--no-session-persistence", "--ignore", ".git,build,_deps", "-p", "--", prompt};
        if (on_chunk) {
            auto result = Process::run_streaming("env", args, "", on_chunk, timeout);
            return {result.exit_code == 0, result.stdout_output, result.stderr_output, result.exit_code, 0.0, {}};
        } else {
            auto result = Process::run("env", args, timeout);
            return {result.exit_code == 0, result.stdout_output, result.stderr_output, result.exit_code, 0.0, {}};
        }
    }

    if (token.empty()) {
        return {false, "", "No Anthropic auth found. Set ANTHROPIC_API_KEY.", 1, 0.0, {}};
    }

    std::string m = model.empty() ? "claude-3-5-sonnet-20241022" : model;
    nlohmann::json body;
    body["model"] = m;
    body["max_tokens"] = 4096;
    body["messages"] = nlohmann::json::array({{{"role", "user"}, {"content", prompt}}});

    std::string auth_header = "x-api-key: " + token;

    if (on_chunk) {
        body["stream"] = true;
        auto sse_parser = [&](const std::string& chunk) {
            static std::string full_content;
            static std::string sse_buffer;
            if (full_content.size() > 100 * 1024 * 1024) return;
            sse_buffer += chunk;
            size_t pos = 0;
            while (pos < sse_buffer.size()) {
                auto nl = sse_buffer.find('\n', pos);
                if (nl == std::string::npos) break;
                std::string line = sse_buffer.substr(pos, nl - pos);
                pos = nl + 1;
                if (line.starts_with("data: ")) {
                    std::string data = line.substr(6);
                    if (data == "[DONE]") continue;
                    try {
                        auto j = nlohmann::json::parse(data);
                        if (j.value("type", "") == "content_block_delta") {
                            std::string text = j["delta"].value("text", "");
                            if (!text.empty()) { full_content += text; on_chunk(text); }
                        }
                    } catch (...) {}
                }
            }
            sse_buffer = sse_buffer.substr(pos);
        };

        auto result = Process::run_streaming("curl", {"-s", "-S", "-N", "https://api.anthropic.com/v1/messages", "-H", "Content-Type: application/json", "-H", "anthropic-version: 2023-06-01", "-H", auth_header, "-d", body.dump()}, "", sse_parser, timeout);
        if (result.exit_code != 0) return {false, "", "API failed: " + result.stderr_output, result.exit_code, 0.0, {}};
        return {true, "Stream complete", "", 0, 0.0, {}};
    } else {
        auto result = Process::run("curl", {"-s", "-S", "-w", "\n%{http_code}", "https://api.anthropic.com/v1/messages", "-H", "Content-Type: application/json", "-H", "anthropic-version: 2023-06-01", "-H", auth_header, "-d", body.dump()}, timeout);
        if (result.exit_code != 0) return {false, "", "curl failed", result.exit_code, 0.0, {}};
        std::string output = result.stdout_output;
        int status = 0;
        auto last_nl = output.rfind('\n');
        if (last_nl != std::string::npos) { status = std::stoi(output.substr(last_nl + 1)); output = output.substr(0, last_nl); }
        try {
            auto resp_json = nlohmann::json::parse(output);
            if (resp_json.contains("error")) {
                std::string api_err = resp_json["error"].dump();
                return {false, "", "Anthropic API: " + api_err, 1, 0.0, classify_error(status, output)};
            }
            std::string content;
            for (const auto& b : resp_json["content"]) if (b.contains("text")) content += b["text"].get<std::string>();
            return {true, content, "", 0, 0.0, {}};
        } catch (...) { return {false, output, "JSON parse error", 1, 0.0, {}}; }
    }
}

auto ProviderExecutor::execute_ollama(const std::string& model,
                                       const std::string& prompt,
                                       int timeout,
                                       std::function<void(const std::string&)> on_chunk) -> ProviderResponse {
    // --- STRATEGIC PIVOT: Native API Path for Ollama ---
    // Faster, more robust error reporting, and no model-pull deadlocks.
    if (!Process::available("curl")) return {false, "", "curl not found", 127, 0.0, {}};
    
    std::string m = model.empty() ? "qwen2.5-coder:7b" : model;
    nlohmann::json body;
    body["model"] = m;
    body["prompt"] = prompt;
    body["stream"] = (on_chunk != nullptr);

    std::vector<std::string> curl_args = {
        "-s", "-S", "-w", "\n%{http_code}",
        "http://localhost:11434/api/generate",
        "-H", "Content-Type: application/json",
        "-d", body.dump()
    };

    if (on_chunk) {
        auto sse_parser = [&](const std::string& chunk) {
            std::istringstream stream(chunk);
            std::string line;
            while (std::getline(stream, line)) {
                if (line.empty()) continue;
                try {
                    auto j = nlohmann::json::parse(line);
                    std::string text = j.value("response", "");
                    if (!text.empty()) on_chunk(text);
                } catch (...) {}
            }
        };
        auto result = Process::run_streaming("curl", curl_args, "", sse_parser, timeout);
        return {result.exit_code == 0, "Stream complete", result.stderr_output, result.exit_code, 0.0, {}};
    } else {
        auto result = Process::run("curl", curl_args, timeout);
        std::string output = result.stdout_output;
        auto last_nl = output.rfind('\n');
        if (last_nl != std::string::npos) output = output.substr(0, last_nl);
        
        try {
            auto j = nlohmann::json::parse(output);
            if (j.contains("error")) return {false, "", "Ollama API: " + j["error"].get<std::string>(), 1, 0.0, {}};
            return {true, j.value("response", ""), "", 0, 0.0, {}};
        } catch (...) { return {false, output, "JSON parse error", 1, 0.0, {}}; }
    }
}

auto ProviderExecutor::execute_api(const std::string& provider,
                                   const std::string& model,
                                   const std::string& prompt,
                                   int timeout,
                                   const std::optional<ResolvedAuth>& auth,
                                   std::function<void(const std::string&)> on_chunk) -> ProviderResponse {
    std::string url, auth_header;
    nlohmann::json body;
    
    if (provider == "openai") {
        std::string token = auth.has_value() ? auth->token : (std::getenv("OPENAI_API_KEY") ? std::getenv("OPENAI_API_KEY") : "");
        if (token.empty()) return {false, "", "No OpenAI auth", 1, 0.0, {}};
        url = "https://api.openai.com/v1/chat/completions";
        auth_header = "Authorization: Bearer " + token;
        body["model"] = model.empty() ? "gpt-4o" : model;
        body["messages"] = nlohmann::json::array({{{"role", "user"}, {"content", prompt}}});
    } else if (provider == "gemini") {
        std::string token = auth.has_value() ? auth->token : (std::getenv("GEMINI_API_KEY") ? std::getenv("GEMINI_API_KEY") : "");
        if (token.empty()) return {false, "", "No Gemini auth", 1, 0.0, {}};
        
        bool use_oauth = auth.has_value() && auth->is_oauth;
        std::string m = model.empty() ? "gemini-2.0-flash-lite" : model;
        
        if (use_oauth) {
            url = "https://generativelanguage.googleapis.com/v1beta/models/" + m + ":generateContent";
            auth_header = "Authorization: Bearer " + token;
        } else {
            url = "https://generativelanguage.googleapis.com/v1beta/models/" + m + ":generateContent?key=" + token;
        }
        body["contents"] = nlohmann::json::array({{{"parts", nlohmann::json::array({{{"text", prompt}}})}}});
    }
    
    std::vector<std::string> curl_args = {"-s", "-S", "-w", "\n%{http_code}", url, "-H", "Content-Type: application/json"};
    if (!auth_header.empty()) { curl_args.push_back("-H"); curl_args.push_back(auth_header); }
    curl_args.push_back("-d"); curl_args.push_back(body.dump());

    ProcessResult result;
    if (on_chunk) result = Process::run_streaming("curl", curl_args, "", on_chunk, timeout);
    else result = Process::run("curl", curl_args, timeout);
    
    std::string output = result.stdout_output;
    int http_status = 0;
    auto last_nl = output.rfind('\n');
    if (last_nl != std::string::npos) {
        try { http_status = std::stoi(output.substr(last_nl + 1)); } catch (...) {}
        output = output.substr(0, last_nl);
    }

    try {
        auto resp_json = nlohmann::json::parse(output);
        if (resp_json.contains("error")) return {false, "", provider + " API: " + resp_json["error"].dump(), 1, 0.0, classify_error(http_status, output)};
        std::string content = (provider == "openai") ? resp_json["choices"][0]["message"]["content"].get<std::string>() : resp_json["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
        return {true, content, "", 0, 0.0, {}};
    } catch (...) { return {false, output, "JSON parse error", 1, 0.0, {}}; }
}

auto ProviderExecutor::load_agent_prompt(const std::string& euxis_home, const std::string& prompt_path) -> std::string {
    if (prompt_path.empty()) return {};
    auto full_path = std::filesystem::path(euxis_home) / prompt_path;
    if (!std::filesystem::exists(full_path)) return {};
    std::ifstream f(full_path);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (content.starts_with("---\n")) {
        auto end = content.find("\n---\n", 4);
        if (end != std::string::npos) content = content.substr(end + 5);
    }
    auto pos = content.find_first_not_of(" \t\n\r");
    if (pos != std::string::npos && pos > 0) content = content.substr(pos);
    return content;
}

auto ProviderExecutor::build_prompt(const std::string& system_prompt, const std::string& task, const std::string& context) -> std::string {
    std::ostringstream out;
    if (!system_prompt.empty()) out << system_prompt << "\n\n";
    if (!context.empty()) out << "## Context\n" << context << "\n\n";
    out << "## Task\n" << task;
    return out.str();
}

} // namespace euxis::cli
