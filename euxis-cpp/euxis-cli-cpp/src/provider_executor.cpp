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
    // Import credentials from environment and local AI CLI tools
    auth_store_.import_env_vars();
    auth_store_.auto_import_claude_oauth();
    auth_store_.auto_import_gemini_oauth();
}

auto ProviderExecutor::execute(const ModelSelection& selection,
                                const std::string& prompt,
                                int timeout_seconds,
                                std::optional<ResolvedAuth> auth) -> ProviderResponse {
    auto start = std::chrono::steady_clock::now();

    // Resolve auth if not provided
    if (!auth.has_value()) {
        auth = auth_store_.resolve_with_fallback(selection.provider);
    }

    ProviderResponse resp;
    if (selection.provider == "claude" || selection.provider == "anthropic") {
        resp = execute_claude(selection.model, prompt, timeout_seconds, auth);
    } else if (selection.provider == "ollama") {
        resp = execute_ollama(selection.model, prompt, timeout_seconds);
    } else if (selection.provider == "openai" || selection.provider == "gemini") {
        resp = execute_api(selection.provider, selection.model, prompt, timeout_seconds, auth);
    } else {
        // Try as a generic CLI command
        if (Process::available(selection.provider)) {
            if (std::getenv("EUXIS_TEST_SKIP_BROWSER")) {
                resp = {false, "", "CLI execution skipped due to EUXIS_TEST_SKIP_BROWSER", 1, 0.0, {}};
            } else {
                auto result = Process::run_with_input(selection.provider, {prompt}, "", timeout_seconds);
                resp.output = result.stdout_output;
                resp.error = result.stderr_output;
                resp.exit_code = result.exit_code;
                resp.success = result.exit_code == 0;
            }
        } else {
            resp = {false, "", "Unknown provider: " + selection.provider, 1, 0.0, {}};
        }
    }

    // Report success/failure to the auth store for round-robin/cooldowns
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
    // 1. Check ANTHROPIC_API_KEY env var
    const char* api_key = std::getenv("ANTHROPIC_API_KEY");
    if (api_key && api_key[0]) return api_key;

    // 2. Read OAuth token from Claude Code credentials
    const char* home = std::getenv("HOME");
    if (!home) return {};

    auto creds_path = std::filesystem::path(home) / ".claude" / ".credentials.json";
    if (!std::filesystem::exists(creds_path)) return {};

    try {
        std::ifstream f(creds_path);
        auto creds = nlohmann::json::parse(f);

        if (!creds.contains("claudeAiOauth")) return {};
        auto& oauth = creds["claudeAiOauth"];

        // Check expiry
        if (oauth.contains("expiresAt")) {
            auto expires_ms = oauth["expiresAt"].get<int64_t>();
            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            if (now_ms >= expires_ms) {
                spdlog::warn("Claude OAuth token expired");
                return {};
            }
        }

        if (oauth.contains("accessToken")) {
            return oauth["accessToken"].get<std::string>();
        }
    } catch (const std::exception& e) {
        spdlog::warn("Failed to read Claude credentials: {}", e.what());
    }
    return {};
}

auto ProviderExecutor::classify_error(int http_status,
                                        const std::string& body) -> std::optional<CooldownReason> {
    if (http_status == 429) return CooldownReason::RateLimit;
    if (http_status == 401 || http_status == 403) return CooldownReason::AuthError;

    std::string lower = body;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.find("rate_limit") != std::string::npos ||
        lower.find("rate limit") != std::string::npos) {
        return CooldownReason::RateLimit;
    }
    if (lower.find("billing") != std::string::npos ||
        lower.find("credit") != std::string::npos ||
        lower.find("insufficient_quota") != std::string::npos) {
        return CooldownReason::BillingError;
    }
    return std::nullopt;
}

auto ProviderExecutor::execute_claude(const std::string& model,
                                       const std::string& prompt,
                                       int timeout,
                                       const std::optional<ResolvedAuth>& auth) -> ProviderResponse {
    // Use provided auth token or fall back to legacy resolution
    std::string token;
    bool is_oauth = false;
    if (auth.has_value()) {
        token = auth->token;
        is_oauth = auth->is_oauth;
    } else {
        token = resolve_anthropic_token();
        if (!token.empty()) {
            is_oauth = token.starts_with("sk-ant-oat");
        }
    }

    if (!token.empty()) {
        std::string m = model.empty() ? "claude-sonnet-4-6" : model;

        nlohmann::json body;
        body["model"] = m;
        body["max_tokens"] = 4096;
        body["messages"] = nlohmann::json::array({
            {{"role", "user"}, {"content", prompt}}
        });

        std::string body_str = body.dump();

        // Determine auth header based on token type
        std::string auth_header;
        if (is_oauth) {
            auth_header = "Authorization: Bearer " + token;
        } else {
            auth_header = "x-api-key: " + token;
        }

        std::vector<std::string> curl_args = {
            "-s", "-S", "-w", "\n%{http_code}",
            "https://api.anthropic.com/v1/messages",
            "-H", "Content-Type: application/json",
            "-H", "anthropic-version: 2023-06-01",
            "-H", auth_header,
            "-d", body_str
        };

        auto result = Process::run("curl", curl_args, timeout);
        if (result.exit_code != 0) {
            return {false, "", "API call failed: " + result.stderr_output, result.exit_code, 0.0, {}};
        }

        // Extract HTTP status from last line
        std::string output = result.stdout_output;
        int http_status = 0;
        auto last_nl = output.rfind('\n');
        if (last_nl != std::string::npos) {
            try { http_status = std::stoi(output.substr(last_nl + 1)); }
            catch (...) {}
            output = output.substr(0, last_nl);
        }

        try {
            auto resp_json = nlohmann::json::parse(output);

            // Check for API errors
            if (resp_json.contains("error")) {
                std::string err_msg = resp_json["error"].contains("message")
                    ? resp_json["error"]["message"].get<std::string>()
                    : resp_json["error"].dump();

                // If OAuth is rejected by the public API, we must fall back to the CLI
                if (is_oauth && err_msg.find("OAuth authentication is currently not supported") != std::string::npos) {
                    spdlog::info("Anthropic API rejected OAuth token, falling back to 'claude' CLI");
                    goto fallback_to_cli;
                }

                ProviderResponse resp;
                resp.success = false;
                resp.error = "Anthropic API: " + err_msg;
                resp.exit_code = 1;
                resp.failure_reason = classify_error(http_status, output);
                return resp;
            }

            // Extract content from Messages API response
            if (resp_json.contains("content") && !resp_json["content"].empty()) {
                std::string content;
                for (const auto& block : resp_json["content"]) {
                    if (block.contains("text")) {
                        if (!content.empty()) content += "\n";
                        content += block["text"].get<std::string>();
                    }
                }
                return {true, content, "", 0, 0.0, {}};
            }

            return {false, output, "No content in API response", 1, 0.0, {}};
        } catch (const std::exception& e) {
            spdlog::warn("Failed to parse Anthropic response: {}", e.what());
            return {false, output, "JSON parse error: " + std::string(e.what()), 1, 0.0, {}};
        }
    }

fallback_to_cli:
    // In tests, skip the CLI fallback if we want to avoid browser popups
    if (std::getenv("EUXIS_TEST_SKIP_BROWSER")) {
        return {false, "", "Anthropic API failed and CLI fallback skipped due to EUXIS_TEST_SKIP_BROWSER", 1, 0.0, {}};
    }

    // Fallback: try claude CLI with env vars stripped
    if (!Process::available("claude")) {
        return {false, "", "No Anthropic auth found. Set ANTHROPIC_API_KEY or sign in via Claude Code.", 1, 0.0, {}};
    }

    std::vector<std::string> args = {
        "-u", "CLAUDECODE", 
        "-u", "CLAUDE_CODE_ENTRYPOINT",
        "-u", "CLAUDE_MODEL",
        "-u", "ANTHROPIC_MODEL",
        "-u", "MODEL",
        "claude", "-p", prompt, "--output-format", "text"
    };
    
    // Map Euxis aliases to Claude Code internal aliases if necessary
    // or just omit the model if we're falling back, as the CLI handles defaults well.
    // For now, if the fallback triggers, we just use the CLI's default model 
    // to avoid compatibility issues across different versions of Claude Code.
    // If the user explicitly asks for a non-sonnet model, we might want to pass it, 
    // but the CLI is quite strict about model names.

    auto result = Process::run("env", args, timeout);
    
    // Some versions of Claude CLI might return non-zero even on success if they can't 
    // access some terminal features, but still print the response to stdout.
    bool success = (result.exit_code == 0) || (!result.stdout_output.empty() && result.exit_code != 127);
    
    std::string err = result.stderr_output;
    if (err.empty() && !success) {
        err = "Claude CLI failed with exit code " + std::to_string(result.exit_code);
    }
    
    return {
        success,
        result.stdout_output,
        err,
        result.exit_code,
        0.0,
        {}
    };
}

auto ProviderExecutor::execute_ollama(const std::string& model,
                                       const std::string& prompt,
                                       int timeout) -> ProviderResponse {
    if (!Process::available("ollama")) {
        return {false, "", "ollama not found on PATH", 127, 0.0, {}};
    }

    std::string m = model.empty() ? "qwen3:32b" : model;
    auto result = Process::run_with_input("ollama", {"run", m}, prompt, timeout);
    return {
        result.exit_code == 0,
        result.stdout_output,
        result.stderr_output,
        result.exit_code,
        0.0,
        {}
    };
}

auto ProviderExecutor::execute_api(const std::string& provider,
                                    const std::string& model,
                                    const std::string& prompt,
                                    int timeout,
                                    const std::optional<ResolvedAuth>& auth) -> ProviderResponse {
    if (!Process::available("curl")) {
        return {false, "", "curl not found (required for API calls)", 127, 0.0, {}};
    }

    std::string url;
    std::string auth_header;
    nlohmann::json body;

    if (provider == "openai") {
        std::string token;
        if (auth.has_value()) {
            token = auth->token;
        } else {
            const char* key = std::getenv("OPENAI_API_KEY");
            if (key && key[0]) token = key;
        }
        if (token.empty()) {
            return {false, "", "No OpenAI auth available", 1, 0.0, {}};
        }
        url = "https://api.openai.com/v1/chat/completions";
        auth_header = "Authorization: Bearer " + token;
        body["model"] = model.empty() ? "gpt-4o" : model;
        body["messages"] = nlohmann::json::array({{{"role", "user"}, {"content", prompt}}});
    } else if (provider == "gemini") {
        std::string token;
        bool is_oauth = false;
        if (auth.has_value()) {
            token = auth->token;
            is_oauth = auth->is_oauth;
        } else {
            const char* key = std::getenv("GEMINI_API_KEY");
            if (!key || !key[0]) key = std::getenv("GOOGLE_API_KEY");
            if (key && key[0]) token = key;
        }
        if (token.empty()) {
            return {false, "", "No Gemini auth available", 1, 0.0, {}};
        }
        std::string m = model.empty() ? "gemini-2.0-flash-lite" : model;
        if (is_oauth) {
            url = "https://generativelanguage.googleapis.com/v1beta/models/" + m +
                  ":generateContent";
            auth_header = "Authorization: Bearer " + token;
        } else {
            url = "https://generativelanguage.googleapis.com/v1beta/models/" + m +
                  ":generateContent?key=" + token;
        }
        body["contents"] = nlohmann::json::array(
            {{{"parts", nlohmann::json::array({{{"text", prompt}}})}}});
    } else {
        return {false, "", "Unsupported API provider: " + provider, 1, 0.0, {}};
    }

    std::string body_str = body.dump();
    std::vector<std::string> curl_args = {"-s", "-S", "-w", "\n%{http_code}",
                                           url, "-H", "Content-Type: application/json"};
    if (!auth_header.empty()) {
        curl_args.push_back("-H");
        curl_args.push_back(auth_header);
    }
    curl_args.push_back("-d");
    curl_args.push_back(body_str);

    auto result = Process::run("curl", curl_args, timeout);
    if (result.exit_code != 0) {
        return {false, "", "curl failed: " + result.stderr_output, result.exit_code, 0.0, {}};
    }

    // Extract HTTP status from last line
    std::string output = result.stdout_output;
    int http_status = 0;
    auto last_nl = output.rfind('\n');
    if (last_nl != std::string::npos) {
        try { http_status = std::stoi(output.substr(last_nl + 1)); }
        catch (...) {}
        output = output.substr(0, last_nl);
    }

    // Parse JSON response
    try {
        auto resp_json = nlohmann::json::parse(output);

        // Check for error responses
        if (resp_json.contains("error")) {
            std::string err_msg;
            if (resp_json["error"].is_object() && resp_json["error"].contains("message")) {
                err_msg = resp_json["error"]["message"].get<std::string>();
            } else {
                err_msg = resp_json["error"].dump();
            }

            ProviderResponse resp;
            resp.success = false;
            resp.error = provider + " API: " + err_msg;
            resp.exit_code = 1;
            resp.failure_reason = classify_error(http_status, output);
            return resp;
        }

        std::string content;
        if (provider == "openai") {
            content = resp_json["choices"][0]["message"]["content"].get<std::string>();
        } else if (provider == "gemini") {
            content = resp_json["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
        }
        return {true, content, "", 0, 0.0, {}};
    } catch (const std::exception& e) {
        spdlog::warn("Failed to parse {} response: {}", provider, e.what());
        ProviderResponse resp;
        resp.success = false;
        resp.output = output;
        resp.error = "JSON parse error: " + std::string(e.what());
        resp.exit_code = 1;
        resp.failure_reason = classify_error(http_status, output);
        return resp;
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
