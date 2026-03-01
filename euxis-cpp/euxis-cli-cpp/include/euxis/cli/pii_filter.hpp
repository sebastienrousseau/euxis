#pragma once

#include <string>

namespace euxis::cli {

/// Redact PII from text (emails, API keys, bearer tokens, IPv4 addresses).
class PiiFilter {
public:
    /// Redact all recognized PII patterns in the input string.
    static auto redact(const std::string& input) -> std::string;

    /// Returns true if redaction is enabled (EUXIS_LOG_SANITIZE != "0").
    static bool enabled();
};

} // namespace euxis::cli
