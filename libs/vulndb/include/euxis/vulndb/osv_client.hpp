/// @file
/// @brief HTTP client for the OSV.dev REST API.

#pragma once

#include <chrono>
#include <expected>
#include <string>
#include <vector>

#include "errors.hpp"
#include "types.hpp"

namespace euxis::vulndb {

/// @brief Configuration for an `OsvClient`.
struct OsvClientConfig {
    /// Override base URL. Empty falls back to the public OSV.dev endpoint.
    /// Mock-server tests set this to `http://127.0.0.1:<port>`.
    std::string base_url{"https://api.osv.dev"};
    /// Per-query wall-clock budget (connect + read combined).
    std::chrono::milliseconds timeout{std::chrono::seconds{30}};
    /// Maximum number of retries on 5xx / 429 responses. Each retry
    /// waits an exponentially-growing backoff capped at 4s.
    int max_retries{3};
};

/// @brief Client for `POST /v1/query` against api.osv.dev.
///
/// One client is safe to share across threads — each `query` call
/// constructs its own `httplib::Client` internally, so calls do not
/// share connection state. The transport is bounded by
/// `OsvClientConfig::timeout` + retry policy.
///
/// All public methods return `std::expected<T, Error>` — never throws.
class OsvClient {
public:
    explicit OsvClient(OsvClientConfig cfg = {});

    /// @brief Query OSV by PURL.
    ///
    /// The PURL must include the package coordinates AND a version (the
    /// `@version` segment). Bare-package queries are out of scope for
    /// this client — bulk queries should use the offline dump path.
    ///
    /// @return The (possibly empty) list of vulnerabilities OSV reports
    ///         for this PURL, or an `Error` describing the transport /
    ///         parse failure.
    [[nodiscard]] auto query_by_purl(const std::string& purl) const
        -> std::expected<std::vector<OsvVuln>, Error>;

    /// @brief Query OSV by `(ecosystem, name, version)` triple.
    ///
    /// Useful when the consumer already has the parsed ecosystem string
    /// (`PyPI`, `npm`, `Go`, ...) and would otherwise have to round-trip
    /// through a PURL formatter. The semantics match `query_by_purl`.
    [[nodiscard]] auto query_by_package(const std::string& ecosystem,
                                        const std::string& name,
                                        const std::string& version) const
        -> std::expected<std::vector<OsvVuln>, Error>;

    [[nodiscard]] auto config() const noexcept -> const OsvClientConfig& {
        return cfg_;
    }

private:
    OsvClientConfig cfg_;
};

} // namespace euxis::vulndb
