#include "euxis/cli/pii_filter.hpp"

#include <cstdlib>
#include <regex>

namespace euxis::cli {

bool PiiFilter::enabled() {
    const char* v = std::getenv("EUXIS_LOG_SANITIZE");
    return !v || std::string_view(v) != "0";
}

auto PiiFilter::redact(const std::string& input) -> std::string {
    if (!enabled()) return input;

    std::string result = input;

    // Email addresses
    static const std::regex email_re(R"([a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,})");
    result = std::regex_replace(result, email_re, "[EMAIL]");

    // API keys (sk-..., sk_live_..., key-..., etc. — 20+ chars total after prefix)
    static const std::regex api_key_re(R"((sk|key|api[_-]?key)[_\-][a-zA-Z0-9_\-]{16,})");
    result = std::regex_replace(result, api_key_re, "[API_KEY]");

    // Bearer tokens
    static const std::regex bearer_re(R"(Bearer\s+[a-zA-Z0-9._\-]+)", std::regex::icase);
    result = std::regex_replace(result, bearer_re, "Bearer [TOKEN]");

    // IPv4 addresses (except 127.0.0.1 and 0.0.0.0)
    static const std::regex ipv4_re(
        R"((?!127\.0\.0\.1)(?!0\.0\.0\.0)\b\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\b)");
    result = std::regex_replace(result, ipv4_re, "[IP]");

    return result;
}

} // namespace euxis::cli
