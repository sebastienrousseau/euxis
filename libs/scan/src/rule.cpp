#include "euxis/scan/rule.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace euxis::scan {

namespace {

auto ascii_lower(std::string_view s) -> std::string {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

} // namespace

auto parse_severity(std::string_view s) noexcept
    -> std::optional<RuleSeverity> {
    auto lower = ascii_lower(s);
    if (lower == "info" || lower == "informational") return RuleSeverity::Info;
    if (lower == "warning" || lower == "warn")        return RuleSeverity::Warning;
    if (lower == "error"   || lower == "critical")    return RuleSeverity::Error;
    return std::nullopt;
}

auto to_security_severity(RuleSeverity s) noexcept
    -> euxis::security::Severity {
    using Sev = euxis::security::Severity;
    switch (s) {
        case RuleSeverity::Info:    return Sev::Info;
        case RuleSeverity::Warning: return Sev::Medium;
        case RuleSeverity::Error:   return Sev::High;
    }
    return Sev::Medium;
}

auto parse_language_token(std::string_view s) noexcept
    -> std::optional<euxis::parse::Language> {
    using L = euxis::parse::Language;
    auto lower = ascii_lower(s);
    if (lower == "c")                                            return L::C;
    if (lower == "cpp"  || lower == "c++"  || lower == "cxx")    return L::Cpp;
    if (lower == "rust" || lower == "rs")                        return L::Rust;
    if (lower == "go"   || lower == "golang")                    return L::Go;
    if (lower == "python" || lower == "py")                      return L::Python;
    if (lower == "javascript" || lower == "js")                  return L::JavaScript;
    if (lower == "typescript" || lower == "ts")                  return L::TypeScript;
    if (lower == "java")                                         return L::Java;
    return std::nullopt;
}

auto parse_owasp_token(std::string_view s) noexcept
    -> std::optional<euxis::security::OwaspCategory> {
    using Cat = euxis::security::OwaspCategory;

    // Accept "A03", "A03:2025", "A03_2025", "A03-2025", lowercase
    // variants. We only care about the category number; the year
    // suffix is optional and defaults to 2025.
    std::string stripped;
    stripped.reserve(s.size());
    for (char c : s) {
        if (c == ':' || c == '-' || c == '_' || c == ' ') break;
        stripped.push_back(c);
    }
    auto lower = ascii_lower(stripped);

    if (lower == "a01") return Cat::A01_BrokenAccessControl;
    if (lower == "a02") return Cat::A02_CryptographicFailures;
    if (lower == "a03") return Cat::A03_SupplyChainFailures;
    if (lower == "a04") return Cat::A04_InsecureDesign;
    if (lower == "a05") return Cat::A05_SecurityMisconfiguration;
    if (lower == "a06") return Cat::A06_VulnerableComponents;
    if (lower == "a07") return Cat::A07_AuthenticationFailures;
    if (lower == "a08") return Cat::A08_DataIntegrityFailures;
    if (lower == "a09") return Cat::A09_LoggingMonitoringFailures;
    if (lower == "a10") return Cat::A10_MishandlingExceptionalConditions;

    return std::nullopt;
}

} // namespace euxis::scan
