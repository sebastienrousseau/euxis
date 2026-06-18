/// @file
/// @brief Tests for the OSV.dev HTTP client against a local mock server.

#include <gtest/gtest.h>

#include "euxis/vulndb/osv_client.hpp"

#include <future>
#include <string>
#include <thread>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace euxis::vulndb {
namespace {

/// @brief Spins up an httplib::Server on a random localhost port and
///        responds to `POST /v1/query` with the configured body.
struct MockOsvServer {
    httplib::Server srv;
    std::thread     th;
    int             port{0};

    MockOsvServer(std::string body, int status = 200) {
        srv.Post("/v1/query",
                 [body = std::move(body), status](const httplib::Request&,
                                                  httplib::Response& res) {
            res.status = status;
            res.set_content(body, "application/json");
        });
        std::promise<int> bound;
        auto fut = bound.get_future();
        th = std::thread([this, &bound]() {
            int p = srv.bind_to_any_port("127.0.0.1");
            bound.set_value(p);
            srv.listen_after_bind();
        });
        port = fut.get();
    }
    ~MockOsvServer() {
        srv.stop();
        if (th.joinable()) th.join();
    }
    [[nodiscard]] auto base_url() const -> std::string {
        return "http://127.0.0.1:" + std::to_string(port);
    }
};

constexpr auto kSingleVulnResponse = R"({
  "vulns": [
    {
      "id":      "GHSA-xxxx-yyyy-zzzz",
      "aliases": ["CVE-2026-12345"],
      "summary": "Example vulnerability summary",
      "details": "long description here",
      "affected": [{
        "ranges": [{
          "type":   "ECOSYSTEM",
          "events": [{"introduced": "1.0.0"}, {"fixed": "1.5.10"}]
        }]
      }],
      "severity": [{"type": "CVSS_V3", "score": 7.5}],
      "references": [
        {"url": "https://example.com/advisory"},
        {"url": "https://github.com/example/patch/commit/abc"}
      ]
    }
  ]
})";

TEST(OsvClient, ParsesSingleVulnResponse) {
    MockOsvServer mock{kSingleVulnResponse};
    OsvClientConfig cfg;
    cfg.base_url = mock.base_url();
    OsvClient client{cfg};

    auto vulns = client.query_by_purl("pkg:pypi/requests@2.20.0");
    ASSERT_TRUE(vulns.has_value());
    ASSERT_EQ(vulns->size(), 1u);

    const auto& v = vulns->front();
    EXPECT_EQ(v.id, "GHSA-xxxx-yyyy-zzzz");
    ASSERT_EQ(v.aliases.size(), 1u);
    EXPECT_EQ(v.aliases.front(), "CVE-2026-12345");
    EXPECT_EQ(v.summary, "Example vulnerability summary");
    EXPECT_EQ(v.fixed_in, "1.5.10");
    EXPECT_EQ(v.vulnerable_range, ">=1.0.0, <1.5.10");
    EXPECT_DOUBLE_EQ(v.cvss_score, 7.5);
    EXPECT_EQ(v.severity, Severity::High);
    ASSERT_EQ(v.references.size(), 2u);
}

TEST(OsvClient, EmptyVulnsReturnsEmptyVector) {
    MockOsvServer mock{R"({"vulns": []})"};
    OsvClientConfig cfg;
    cfg.base_url = mock.base_url();
    OsvClient client{cfg};

    auto vulns = client.query_by_purl("pkg:cargo/serde@1.0.150");
    ASSERT_TRUE(vulns.has_value());
    EXPECT_TRUE(vulns->empty());
}

TEST(OsvClient, MissingVulnsKeyReturnsEmptyVector) {
    MockOsvServer mock{R"({})"};
    OsvClientConfig cfg;
    cfg.base_url = mock.base_url();
    OsvClient client{cfg};

    auto vulns = client.query_by_purl("pkg:cargo/serde@1.0.150");
    ASSERT_TRUE(vulns.has_value());
    EXPECT_TRUE(vulns->empty());
}

TEST(OsvClient, EmptyPurlIsRejectedWithoutTransport) {
    OsvClient client{};
    auto vulns = client.query_by_purl("");
    ASSERT_FALSE(vulns.has_value());
    EXPECT_EQ(vulns.error().kind, ErrorKind::InvalidPackage);
}

TEST(OsvClient, Non2xxIsSurfacedAsUpstreamStatus) {
    MockOsvServer mock{R"({"error":"bad request"})", /*status=*/400};
    OsvClientConfig cfg;
    cfg.base_url = mock.base_url();
    cfg.max_retries = 0;  // don't retry 4xx
    OsvClient client{cfg};

    auto vulns = client.query_by_purl("pkg:pypi/foo@1.0");
    ASSERT_FALSE(vulns.has_value());
    EXPECT_EQ(vulns.error().kind, ErrorKind::UpstreamStatus);
}

TEST(OsvClient, MalformedJsonIsSurfacedAsInvalidResponse) {
    MockOsvServer mock{"not a json {{"};
    OsvClientConfig cfg;
    cfg.base_url = mock.base_url();
    OsvClient client{cfg};

    auto vulns = client.query_by_purl("pkg:pypi/foo@1.0");
    ASSERT_FALSE(vulns.has_value());
    EXPECT_EQ(vulns.error().kind, ErrorKind::InvalidResponse);
}

TEST(OsvClient, QueryByPackageRoundtrip) {
    MockOsvServer mock{R"({"vulns":[{"id":"X","severity":[{"type":"CVSS_V3","score":2.0}]}]})"};
    OsvClientConfig cfg;
    cfg.base_url = mock.base_url();
    OsvClient client{cfg};

    auto vulns = client.query_by_package("PyPI", "requests", "2.20.0");
    ASSERT_TRUE(vulns.has_value());
    ASSERT_EQ(vulns->size(), 1u);
    EXPECT_EQ(vulns->front().severity, Severity::Low);
}

TEST(OsvClient, QueryByPackageMissingNameIsRejected) {
    OsvClient client{};
    auto vulns = client.query_by_package("PyPI", "", "1.0");
    ASSERT_FALSE(vulns.has_value());
    EXPECT_EQ(vulns.error().kind, ErrorKind::InvalidPackage);
}

// Severity bucketing — pure logic test, no transport.
TEST(SeverityBucket, MatchesNvdConvention) {
    EXPECT_EQ(severity_from_cvss(0.0), Severity::Unknown);
    EXPECT_EQ(severity_from_cvss(2.0), Severity::Low);
    EXPECT_EQ(severity_from_cvss(5.5), Severity::Medium);
    EXPECT_EQ(severity_from_cvss(7.5), Severity::High);
    EXPECT_EQ(severity_from_cvss(9.5), Severity::Critical);
    EXPECT_EQ(severity_from_cvss(10.0), Severity::Critical);
}

} // namespace
} // namespace euxis::vulndb
