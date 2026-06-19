/// @file
/// @brief Tests for the OSV-backed SBOM enricher.

#include <gtest/gtest.h>

#include "euxis/vulndb/enricher.hpp"

#include <future>
#include <thread>

#include <httplib.h>

namespace euxis::vulndb {
namespace {

struct MockServer {
    httplib::Server srv;
    std::thread     th;
    int             port{0};

    explicit MockServer(std::string body) {
        srv.Post("/v1/query",
                 [body = std::move(body)](const httplib::Request&,
                                          httplib::Response& res) {
            res.status = 200;
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
    ~MockServer() {
        srv.stop();
        if (th.joinable()) th.join();
    }
    [[nodiscard]] auto base_url() const -> std::string {
        return "http://127.0.0.1:" + std::to_string(port);
    }
};

[[nodiscard]] auto make_doc() -> euxis::sbom::SbomDocument {
    using namespace euxis::sbom;
    SbomDocument doc;
    // Imperative init: GCC's -Werror=missing-field-initializers fires on
    // designated initializers that skip fields, even when the omitted
    // ones have default member initialisers.
    Component a;
    a.purl    = "pkg:pypi/requests@2.20.0";
    a.name    = "requests";
    a.version = "2.20.0";
    a.type    = ComponentType::Library;
    doc.components.push_back(std::move(a));
    Component b;
    b.purl    = "pkg:cargo/serde@1.0.150";
    b.name    = "serde";
    b.version = "1.0.150";
    b.type    = ComponentType::Library;
    doc.components.push_back(std::move(b));
    return doc;
}

constexpr auto kAffectedResponse = R"({
  "vulns": [{
    "id":       "GHSA-aaaa",
    "summary":  "demo vuln",
    "severity": [{"type":"CVSS_V3","score":9.1}]
  }]
})";

TEST(Enricher, AffectedComponentEmitsAffectedVexStatement) {
    MockServer mock{kAffectedResponse};
    OsvClientConfig ccfg;
    ccfg.base_url = mock.base_url();
    OsvClient client{ccfg};

    EnricherConfig ecfg;
    ecfg.emit_not_affected = false;
    Enricher enricher{client, ecfg};

    const auto doc = make_doc();
    auto report = enricher.enrich(doc);
    ASSERT_TRUE(report.has_value());
    ASSERT_EQ(report->matches.size(), 2u);
    // Both components hit the same mock response so both report affected.
    EXPECT_EQ(report->matches[0].vulns.size(), 1u);
    EXPECT_EQ(report->matches[1].vulns.size(), 1u);
    EXPECT_TRUE(report->has_high_or_critical());

    // Each affected match contributes one Affected VEX statement.
    ASSERT_EQ(report->vex.statements.size(), 2u);
    for (const auto& st : report->vex.statements) {
        EXPECT_EQ(st.status, euxis::sbom::VexStatus::Affected);
        EXPECT_EQ(st.vulnerability_name, "GHSA-aaaa");
        ASSERT_EQ(st.product_purls.size(), 1u);
    }
}

TEST(Enricher, NoVulnsLeavesVexDocumentEmptyByDefault) {
    MockServer mock{R"({"vulns":[]})"};
    OsvClientConfig ccfg;
    ccfg.base_url = mock.base_url();
    OsvClient client{ccfg};

    Enricher enricher{client, EnricherConfig{}};
    const auto doc = make_doc();
    auto report = enricher.enrich(doc);
    ASSERT_TRUE(report.has_value());
    ASSERT_EQ(report->matches.size(), 2u);
    EXPECT_TRUE(report->matches[0].vulns.empty());
    EXPECT_TRUE(report->matches[1].vulns.empty());
    EXPECT_FALSE(report->has_high_or_critical());
    EXPECT_TRUE(report->vex.statements.empty());
}

TEST(Enricher, EmitNotAffectedRecordsCleanComponents) {
    MockServer mock{R"({"vulns":[]})"};
    OsvClientConfig ccfg;
    ccfg.base_url = mock.base_url();
    OsvClient client{ccfg};

    EnricherConfig ecfg;
    ecfg.emit_not_affected = true;
    Enricher enricher{client, ecfg};

    const auto doc = make_doc();
    auto report = enricher.enrich(doc);
    ASSERT_TRUE(report.has_value());
    ASSERT_EQ(report->vex.statements.size(), 2u);
    for (const auto& st : report->vex.statements) {
        EXPECT_EQ(st.status, euxis::sbom::VexStatus::NotAffected);
        EXPECT_EQ(st.justification,
                  euxis::sbom::VexJustification::ComponentNotPresent);
    }
}

TEST(Enricher, MaxComponentsBoundsTheRun) {
    MockServer mock{R"({"vulns":[]})"};
    OsvClientConfig ccfg;
    ccfg.base_url = mock.base_url();
    OsvClient client{ccfg};

    EnricherConfig ecfg;
    ecfg.max_components = 1;
    Enricher enricher{client, ecfg};

    const auto doc = make_doc();              // 2 components
    auto report = enricher.enrich(doc);
    ASSERT_TRUE(report.has_value());
    EXPECT_EQ(report->matches.size(), 1u);    // capped
}

TEST(Enricher, ComponentWithoutPurlIsRecordedAsEmptyMatch) {
    MockServer mock{R"({"vulns":[]})"};
    OsvClientConfig ccfg;
    ccfg.base_url = mock.base_url();
    OsvClient client{ccfg};

    Enricher enricher{client, EnricherConfig{}};

    euxis::sbom::SbomDocument doc;
    doc.components.push_back(euxis::sbom::Component{
        .purl    = "",                        // no PURL — skip query
        .name    = "no-purl-component",
        .version = "0.0.0",
        .type    = euxis::sbom::ComponentType::Library,
    });

    auto report = enricher.enrich(doc);
    ASSERT_TRUE(report.has_value());
    ASSERT_EQ(report->matches.size(), 1u);
    EXPECT_TRUE(report->matches[0].vulns.empty());
    EXPECT_TRUE(report->vex.statements.empty());
}

} // namespace
} // namespace euxis::vulndb
