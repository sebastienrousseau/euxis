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

    // Bearer tokens (including JWTs: eyJ...)
    static const std::regex bearer_re(R"(Bearer\s+[a-zA-Z0-9._\-]+)", std::regex::icase);
    result = std::regex_replace(result, bearer_re, "Bearer [TOKEN]");

    // Q1: Standalone JWTs (eyJhbG... three base64 segments)
    static const std::regex jwt_re(R"(eyJ[a-zA-Z0-9_\-]{10,}\.[a-zA-Z0-9_\-]{10,}\.[a-zA-Z0-9_\-]{10,})");
    result = std::regex_replace(result, jwt_re, "[JWT]");

    // Q1: SSN patterns (###-##-####)
    static const std::regex ssn_re(R"(\b\d{3}-\d{2}-\d{4}\b)");
    result = std::regex_replace(result, ssn_re, "[SSN]");

    // Q1: Credit card numbers (13-19 digit sequences with optional separators)
    static const std::regex cc_re(R"(\b\d{4}[\s\-]?\d{4}[\s\-]?\d{4}[\s\-]?\d{1,7}\b)");
    result = std::regex_replace(result, cc_re, "[CREDIT_CARD]");

    // IPv4 addresses (except 127.0.0.1 and 0.0.0.0)
    static const std::regex ipv4_re(
        R"((?!127\.0\.0\.1)(?!0\.0\.0\.0)\b\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\b)");
    result = std::regex_replace(result, ipv4_re, "[IP]");

    return result;
}

} // namespace euxis::cli
