#include <euxis/bridge/policy.hpp>

#include <sodium.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace euxis::bridge {

auto SkillExecutionPolicy::permissive() -> SkillExecutionPolicy {
    SkillExecutionPolicy policy;
    policy.require_signature = false;
    policy.filesystem.read_only = false;
    policy.network.deny_all = false;
    policy.resources.memory_mb = 512;
    policy.resources.timeout_seconds = 60;
    return policy;
}

auto SkillExecutionPolicy::to_json() const -> nlohmann::json {
    nlohmann::json j;
    j["require_signature"] = require_signature;
    if (public_key_path) {
        j["public_key_path"] = public_key_path->string();
    }
    j["filesystem"]["read_only"] = filesystem.read_only;
    nlohmann::json write_paths = nlohmann::json::array();
    for (const auto& p : filesystem.write_paths) {
        write_paths.push_back(p.string());
    }
    j["filesystem"]["write_paths"] = write_paths;
    j["network"]["deny_all"] = network.deny_all;
    j["network"]["allowed_hosts"] = network.allowed_hosts;
    j["resources"]["memory_mb"] = resources.memory_mb;
    j["resources"]["timeout_seconds"] = resources.timeout_seconds;
    if (output_schema) {
        j["output_schema"] = *output_schema;
    }
    if (audit_log_path) {
        j["audit_log_path"] = audit_log_path->string();
    }
    return j;
}

// --- AgentCapabilityToken ---

auto AgentCapabilityToken::canonical_string() const -> std::string {
    auto join = [](const std::vector<std::string>& v, const std::string& delim) -> std::string {
        if (v.empty()) return {};
        return std::accumulate(std::next(v.begin()), v.end(), v.front(),
            [&delim](const std::string& a, const std::string& b) { return a + delim + b; });
    };

    return agent_id + ":" + session_id + ":" +
           join(allowed_providers, ",") + ":" +
           join(allowed_tools, ",") + ":" +
           std::to_string(max_token_budget) + ":" +
           (filesystem_read_only ? "1" : "0") + ":" +
           (network_deny_all ? "1" : "0") + ":" +
           issued_at + ":" + expires_at;
}

auto AgentCapabilityToken::to_json() const -> nlohmann::json {
    nlohmann::json j;
    j["agent_id"] = agent_id;
    j["session_id"] = session_id;
    j["allowed_providers"] = allowed_providers;
    j["allowed_tools"] = allowed_tools;
    j["max_token_budget"] = max_token_budget;
    j["filesystem_read_only"] = filesystem_read_only;
    j["network_deny_all"] = network_deny_all;
    j["issued_at"] = issued_at;
    j["expires_at"] = expires_at;
    j["hmac_signature"] = hmac_signature;
    return j;
}

auto AgentCapabilityToken::from_json(const nlohmann::json& j) -> AgentCapabilityToken {
    AgentCapabilityToken t;
    t.agent_id = j.value("agent_id", "");
    t.session_id = j.value("session_id", "");
    t.allowed_providers = j.value("allowed_providers", std::vector<std::string>{});
    t.allowed_tools = j.value("allowed_tools", std::vector<std::string>{});
    t.max_token_budget = j.value("max_token_budget", 4096);
    t.filesystem_read_only = j.value("filesystem_read_only", true);
    t.network_deny_all = j.value("network_deny_all", true);
    t.issued_at = j.value("issued_at", "");
    t.expires_at = j.value("expires_at", "");
    t.hmac_signature = j.value("hmac_signature", "");
    return t;
}

static auto now_iso8601() -> std::string {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    gmtime_r(&time_t_now, &tm_buf);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

static auto hex_encode(const unsigned char* data, size_t len) -> std::string {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i) {
        oss << std::hex << std::setfill('0') << std::setw(2)
            << static_cast<int>(data[i]);
    }
    return oss.str();
}

static auto hex_decode(const std::string& hex, unsigned char* out, size_t out_len) -> bool {
    if (hex.size() != out_len * 2) return false;
    for (size_t i = 0; i < out_len; ++i) {
        unsigned int byte = 0;
        std::istringstream iss(hex.substr(i * 2, 2));
        iss >> std::hex >> byte;
        if (iss.fail()) return false;
        out[i] = static_cast<unsigned char>(byte);
    }
    return true;
}

auto issue_capability_token(
    const std::string& agent_id,
    const std::string& session_id,
    const std::vector<std::string>& allowed_providers,
    const std::vector<std::string>& allowed_tools,
    int max_token_budget,
    const unsigned char* signing_key) -> AgentCapabilityToken {

    AgentCapabilityToken token;
    token.agent_id = agent_id;
    token.session_id = session_id;
    token.allowed_providers = allowed_providers;
    token.allowed_tools = allowed_tools;
    token.max_token_budget = max_token_budget;

    auto issued = now_iso8601();
    token.issued_at = issued;

    // expires_at = issued_at + 1 hour
    auto now = std::chrono::system_clock::now();
    auto expires = now + std::chrono::hours(1);
    auto time_t_exp = std::chrono::system_clock::to_time_t(expires);
    std::tm tm_buf{};
    gmtime_r(&time_t_exp, &tm_buf);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    token.expires_at = oss.str();

    // Compute HMAC-SHA256
    auto canonical = token.canonical_string();
    unsigned char mac[crypto_auth_hmacsha256_BYTES];
    crypto_auth_hmacsha256(mac,
        reinterpret_cast<const unsigned char*>(canonical.data()),
        canonical.size(), signing_key);

    token.hmac_signature = hex_encode(mac, crypto_auth_hmacsha256_BYTES);
    return token;
}

auto verify_capability_token(
    const AgentCapabilityToken& token,
    const unsigned char* signing_key) -> bool {

    // S7: Check expiration before verifying HMAC — reject expired tokens
    if (!token.expires_at.empty()) {
        auto now = now_iso8601();
        // ISO 8601 strings are lexicographically comparable
        if (now >= token.expires_at) {
            return false;  // token expired
        }
    }

    auto canonical = token.canonical_string();
    unsigned char expected_mac[crypto_auth_hmacsha256_BYTES];

    if (!hex_decode(token.hmac_signature, expected_mac, crypto_auth_hmacsha256_BYTES)) {
        return false;
    }

    return crypto_auth_hmacsha256_verify(expected_mac,
        reinterpret_cast<const unsigned char*>(canonical.data()),
        canonical.size(), signing_key) == 0;
}

}  // namespace euxis::bridge
