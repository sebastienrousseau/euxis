/// @file
/// @brief Errors emitted by libs/vulndb — OSV client + enricher.

#pragma once

#include <string>

namespace euxis::vulndb {

/// @brief Error kinds returned by `OsvClient::query` and `Enricher::enrich`.
enum class ErrorKind {
    /// Network / HTTP transport failure before the response was parsed.
    NetworkFailure,
    /// Non-2xx HTTP response from OSV.dev.
    UpstreamStatus,
    /// Response body was not valid JSON or did not match the OSV schema.
    InvalidResponse,
    /// PURL was malformed or the package shape was insufficient to query.
    InvalidPackage,
    /// Local OSV-dump file could not be read or parsed.
    OfflineDumpUnreadable,
    /// Timeout exceeded before a response arrived.
    Timeout,
};

/// @brief Diagnostic surface paired with `ErrorKind` in `std::expected<T, Error>`.
struct Error {
    ErrorKind   kind;
    std::string message;
};

/// @brief Human-readable identifier for telemetry / SARIF emission.
[[nodiscard]] inline auto error_name(ErrorKind k) noexcept -> std::string_view {
    switch (k) {
        case ErrorKind::NetworkFailure:        return "network_failure";
        case ErrorKind::UpstreamStatus:        return "upstream_status";
        case ErrorKind::InvalidResponse:       return "invalid_response";
        case ErrorKind::InvalidPackage:        return "invalid_package";
        case ErrorKind::OfflineDumpUnreadable: return "offline_dump_unreadable";
        case ErrorKind::Timeout:               return "timeout";
    }
    return "unknown";
}

} // namespace euxis::vulndb
