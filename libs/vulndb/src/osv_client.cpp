/// @file
/// @brief Implementation of the OSV.dev REST client.

#include "euxis/vulndb/osv_client.hpp"

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace euxis::vulndb {

namespace {

/// @brief Parse an `osv.dev` JSON response body into a list of `OsvVuln`s.
[[nodiscard]] auto parse_response(const std::string& body)
    -> std::expected<std::vector<OsvVuln>, Error> {
    auto j = nlohmann::json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) {
        return std::unexpected(Error{ErrorKind::InvalidResponse,
                                     "OSV response was not valid JSON"});
    }
    if (!j.is_object()) {
        return std::unexpected(Error{ErrorKind::InvalidResponse,
                                     "OSV response was not a JSON object"});
    }

    std::vector<OsvVuln> out;
    if (!j.contains("vulns") || !j["vulns"].is_array()) {
        // Empty `{}` or an object without `vulns` is OSV's canonical
        // "no vulnerabilities for this package" — return an empty vec.
        return out;
    }

    for (const auto& v : j["vulns"]) {
        OsvVuln rec;
        rec.id      = v.value("id",      std::string{});
        rec.summary = v.value("summary", std::string{});
        rec.details = v.value("details", std::string{});

        if (v.contains("aliases") && v["aliases"].is_array()) {
            for (const auto& a : v["aliases"]) {
                if (a.is_string()) rec.aliases.push_back(a.get<std::string>());
            }
        }

        // OSV's `affected[]` carries the per-package range info we need.
        if (v.contains("affected") && v["affected"].is_array()) {
            for (const auto& aff : v["affected"]) {
                if (aff.contains("ranges") && aff["ranges"].is_array()) {
                    for (const auto& r : aff["ranges"]) {
                        if (!r.contains("events") || !r["events"].is_array()) continue;
                        std::string lower, upper;
                        for (const auto& ev : r["events"]) {
                            if (ev.contains("introduced") && ev["introduced"].is_string()) {
                                lower = ev["introduced"].get<std::string>();
                            }
                            if (ev.contains("fixed") && ev["fixed"].is_string()) {
                                upper = ev["fixed"].get<std::string>();
                                rec.fixed_in = upper;  // first fix wins
                            }
                        }
                        if (!lower.empty() || !upper.empty()) {
                            std::string range;
                            if (!lower.empty()) range += ">=" + lower;
                            if (!upper.empty()) {
                                if (!range.empty()) range += ", ";
                                range += "<" + upper;
                            }
                            rec.vulnerable_range = range;
                        }
                    }
                }
            }
        }

        // CVSS — OSV's `severity[]` array carries one entry per scoring
        // system. We extract the first `CVSS_V3` score we see.
        if (v.contains("severity") && v["severity"].is_array()) {
            for (const auto& sev : v["severity"]) {
                const auto sys = sev.value("type", std::string{});
                if (sys == "CVSS_V3") {
                    // OSV stores either a vector string or a score; try
                    // a numeric `score` first, fall back to the vector.
                    if (sev.contains("score")) {
                        const auto& s = sev["score"];
                        if (s.is_number()) {
                            rec.cvss_score = s.get<double>();
                        } else if (s.is_string()) {
                            // Vector string. Skip — we don't decode.
                        }
                    }
                    break;
                }
            }
        }
        rec.severity = severity_from_cvss(rec.cvss_score);

        if (v.contains("references") && v["references"].is_array()) {
            for (const auto& r : v["references"]) {
                if (r.contains("url") && r["url"].is_string()) {
                    rec.references.push_back(r["url"].get<std::string>());
                }
            }
        }

        rec.raw = v;
        out.push_back(std::move(rec));
    }

    return out;
}

[[nodiscard]] auto post_with_retry(const OsvClientConfig& cfg,
                                   const std::string& body)
    -> std::expected<std::string, Error> {
    httplib::Client client{cfg.base_url};
    client.set_read_timeout(
        std::chrono::duration_cast<std::chrono::seconds>(cfg.timeout));
    client.set_connection_timeout(
        std::chrono::duration_cast<std::chrono::seconds>(cfg.timeout));

    int backoff_ms = 250;
    for (int attempt = 0; attempt <= cfg.max_retries; ++attempt) {
        auto result = client.Post("/v1/query",
                                  {{"content-type", "application/json"}},
                                  body, "application/json");
        if (!result) {
            const auto err = httplib::to_string(result.error());
            if (attempt == cfg.max_retries) {
                return std::unexpected(Error{ErrorKind::NetworkFailure, err});
            }
        } else if (result->status >= 200 && result->status < 300) {
            return result->body;
        } else if (result->status >= 500 || result->status == 429) {
            if (attempt == cfg.max_retries) {
                return std::unexpected(Error{
                    ErrorKind::UpstreamStatus,
                    "OSV.dev returned HTTP " + std::to_string(result->status)});
            }
        } else {
            return std::unexpected(Error{
                ErrorKind::UpstreamStatus,
                "OSV.dev returned HTTP " + std::to_string(result->status)});
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{backoff_ms});
        backoff_ms = std::min(backoff_ms * 2, 4000);
    }
    return std::unexpected(Error{ErrorKind::NetworkFailure,
                                 "exhausted retries"});
}

} // namespace

OsvClient::OsvClient(OsvClientConfig cfg) : cfg_{std::move(cfg)} {}

auto OsvClient::query_by_purl(const std::string& purl) const
    -> std::expected<std::vector<OsvVuln>, Error> {
    if (purl.empty()) {
        return std::unexpected(Error{ErrorKind::InvalidPackage, "empty purl"});
    }
    nlohmann::json body{{"package", {{"purl", purl}}}};
    auto resp = post_with_retry(cfg_, body.dump());
    if (!resp) return std::unexpected(resp.error());
    return parse_response(*resp);
}

auto OsvClient::query_by_package(const std::string& ecosystem,
                                  const std::string& name,
                                  const std::string& version) const
    -> std::expected<std::vector<OsvVuln>, Error> {
    if (name.empty() || ecosystem.empty()) {
        return std::unexpected(Error{
            ErrorKind::InvalidPackage,
            "ecosystem and name are required"});
    }
    nlohmann::json body{
        {"package", {{"name", name}, {"ecosystem", ecosystem}}},
    };
    if (!version.empty()) body["version"] = version;
    auto resp = post_with_retry(cfg_, body.dump());
    if (!resp) return std::unexpected(resp.error());
    return parse_response(*resp);
}

} // namespace euxis::vulndb
