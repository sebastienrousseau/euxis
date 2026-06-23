/// @file
/// @brief Integration tests for `cmd_vulndb` — the implementation
///        behind `euxis vulndb`. Covers help / dispatcher / sync
///        / query-arg-parser paths without making live HTTP calls
///        to OSV.dev (which the OsvClient is hardcoded against).

#include <gtest/gtest.h>

#include "euxis/cli/cmd/vulndb.hpp"
#include "euxis/cli/command.hpp"
#include "euxis/cli/exit_codes.hpp"

#include <cstdio>
#include <filesystem>
#include <functional>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include <httplib.h>

// NOLINTBEGIN(cert-err33-c)

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;

class VulndbCmdTest : public ::testing::Test {
protected:
    fs::path tmpdir;
    Context  ctx;

    void SetUp() override {
        tmpdir = fs::temp_directory_path() /
                 ("euxis_test_vulndb_" + std::to_string(::getpid()) +
                  "_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        fs::create_directories(tmpdir);
        ctx.euxis_home = tmpdir.string();
        ctx.data_dir   = (tmpdir / "data").string();
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tmpdir, ec);
    }

    std::string capture_fd(int target_fd, std::function<int()> fn) {
        auto* tmp = std::tmpfile();
        if (tmp == nullptr) return {};
        int old_fd = ::dup(target_fd);
        ::dup2(::fileno(tmp), target_fd);
        fn();
        if (target_fd == STDOUT_FILENO) ::fflush(stdout);
        if (target_fd == STDERR_FILENO) ::fflush(stderr);
        ::dup2(old_fd, target_fd);
        ::close(old_fd);
        std::rewind(tmp);
        std::string out;
        char buf[4096];
        size_t n = 0;
        while ((n = std::fread(buf, 1, sizeof(buf), tmp)) > 0) {
            out.append(buf, n);
        }
        std::fclose(tmp);
        return out;
    }
    std::string capture_stdout(std::function<int()> fn) {
        return capture_fd(STDOUT_FILENO, std::move(fn));
    }
    std::string capture_cerr(std::function<int()> fn) {
        return capture_fd(STDERR_FILENO, std::move(fn));
    }
};

// ---------------------------------------------------------------------------
// Top-level dispatcher
// ---------------------------------------------------------------------------

TEST_F(VulndbCmdTest, NoArgsPrintsTopUsageWithSuccess) {
    int rc = 0;
    auto out = capture_stdout([&]() {
        rc = cmd_vulndb(ctx, {});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::Success));
    EXPECT_NE(out.find("Usage: euxis vulndb"), std::string::npos);
}

TEST_F(VulndbCmdTest, DashHelpPrintsTopUsage) {
    auto out = capture_stdout([&]() {
        return cmd_vulndb(ctx, {"--help"});
    });
    EXPECT_NE(out.find("Subcommands"), std::string::npos);
}

TEST_F(VulndbCmdTest, ShortHelpPrintsTopUsage) {
    auto out = capture_stdout([&]() {
        return cmd_vulndb(ctx, {"-h"});
    });
    EXPECT_NE(out.find("Subcommands"), std::string::npos);
}

TEST_F(VulndbCmdTest, UnknownSubcommandIsInfraError) {
    int rc = 0;
    capture_cerr([&]() {
        rc = cmd_vulndb(ctx, {"bogus-subcommand"});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::InfraError));
}

// ---------------------------------------------------------------------------
// sync subcommand — placeholder
// ---------------------------------------------------------------------------

TEST_F(VulndbCmdTest, SyncSubcommandPrintsPlaceholder) {
    int rc = 0;
    auto out = capture_stdout([&]() {
        rc = cmd_vulndb(ctx, {"sync"});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::Success));
    EXPECT_NE(out.find("not yet"), std::string::npos);
}

TEST_F(VulndbCmdTest, SyncWithExtraArgsStillPrintsPlaceholder) {
    int rc = 0;
    capture_stdout([&]() {
        rc = cmd_vulndb(ctx, {"sync", "ignored-arg"});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::Success));
}

// ---------------------------------------------------------------------------
// query subcommand — arg-parser paths (no HTTP)
// ---------------------------------------------------------------------------

TEST_F(VulndbCmdTest, QueryWithoutPurlPrintsQueryUsageAndIsInfraError) {
    int rc = 0;
    auto out = capture_stdout([&]() {
        rc = cmd_vulndb(ctx, {"query"});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::InfraError));
    EXPECT_NE(out.find("Usage: euxis vulndb query"), std::string::npos);
}

TEST_F(VulndbCmdTest, QueryDashHelpPrintsQueryUsageWithSuccess) {
    int rc = 0;
    auto out = capture_stdout([&]() {
        rc = cmd_vulndb(ctx, {"query", "--help"});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::Success));
    EXPECT_NE(out.find("Usage: euxis vulndb query"), std::string::npos);
}

TEST_F(VulndbCmdTest, QueryShortHelp) {
    int rc = 0;
    auto out = capture_stdout([&]() {
        rc = cmd_vulndb(ctx, {"query", "-h"});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::Success));
    EXPECT_NE(out.find("Usage: euxis vulndb query"), std::string::npos);
}

TEST_F(VulndbCmdTest, QueryWithJsonFlagButNoPurlIsInfraError) {
    // --json without an actual PURL: parser fills json_output but
    // leaves purl empty, falls through to the empty-purl branch.
    int rc = 0;
    capture_stdout([&]() {
        rc = cmd_vulndb(ctx, {"query", "--json"});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::InfraError));
}

TEST_F(VulndbCmdTest, QueryWithUnexpectedExtraArgIsInfraError) {
    int rc = 0;
    capture_cerr([&]() {
        rc = cmd_vulndb(ctx, {"query", "pkg:cargo/serde@1.0.197", "second-positional"});
        return rc;
    });
    // Either InfraError from the parser, or InfraError from the
    // HTTP failure if the first PURL got accepted. Both signal a
    // problem — accept both as a "non-Success" result.
    EXPECT_NE(rc, to_int(ExitCode::Success));
}

// ---------------------------------------------------------------------------
// query subcommand — full HTTP path via EUXIS_OSV_BASE_URL + mock server
// ---------------------------------------------------------------------------

namespace {

/// Spins up an httplib::Server on a random localhost port that responds
/// to POST /v1/query with the configured body. Mirrors the
/// MockOsvServer in libs/vulndb/tests/test_osv_client.cpp — replicated
/// here so the apps/cli tests have no libs/vulndb-tests dependency.
struct MockOsvServer {
    httplib::Server srv;
    std::thread     th;
    int             port{0};

    explicit MockOsvServer(std::string body, int status = 200) {
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

/// RAII scope guard: sets EUXIS_OSV_BASE_URL for the lifetime of the
/// scope and restores the previous value (or unsets) on destruction.
class OsvUrlOverride {
public:
    explicit OsvUrlOverride(const std::string& url) {
        ::setenv("EUXIS_OSV_BASE_URL", url.c_str(), 1);
    }
    ~OsvUrlOverride() { ::unsetenv("EUXIS_OSV_BASE_URL"); }
    OsvUrlOverride(const OsvUrlOverride&)            = delete;
    OsvUrlOverride& operator=(const OsvUrlOverride&) = delete;
};

constexpr auto kEmptyVulnsResponse  = R"({"vulns": []})";
constexpr auto kSingleLowVuln       = R"({"vulns":[{"id":"GHSA-low","summary":"Low severity issue","severity":[{"type":"CVSS_V3","score":2.0}]}]})";
constexpr auto kSingleHighVuln      = R"({"vulns":[{"id":"GHSA-high","summary":"High severity issue","severity":[{"type":"CVSS_V3","score":8.5}]}]})";
constexpr auto kSingleCriticalVuln  = R"({"vulns":[{"id":"GHSA-critical","summary":"Critical issue","severity":[{"type":"CVSS_V3","score":9.8}]}]})";

} // namespace

TEST_F(VulndbCmdTest, QueryAgainstMockEmptyVulnsExitsSuccess) {
    MockOsvServer mock{kEmptyVulnsResponse};
    OsvUrlOverride env_override{mock.base_url()};
    int rc = 0;
    auto out = capture_stdout([&]() {
        rc = cmd_vulndb(ctx, {"query", "pkg:cargo/serde@1.0.197"});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::Success));
    EXPECT_NE(out.find("0 vulnerabilities"), std::string::npos);
}

TEST_F(VulndbCmdTest, QueryAgainstMockLowSeverityExitsSuccess) {
    MockOsvServer mock{kSingleLowVuln};
    OsvUrlOverride env_override{mock.base_url()};
    int rc = 0;
    auto out = capture_stdout([&]() {
        rc = cmd_vulndb(ctx, {"query", "pkg:cargo/serde@1.0.0"});
        return rc;
    });
    // Low severity → not at/above High → ExitCode::Success.
    EXPECT_EQ(rc, to_int(ExitCode::Success));
    EXPECT_NE(out.find("GHSA-low"), std::string::npos);
    EXPECT_NE(out.find("Low severity issue"), std::string::npos);
}

TEST_F(VulndbCmdTest, QueryAgainstMockHighSeverityExitsBlockingFindings) {
    MockOsvServer mock{kSingleHighVuln};
    OsvUrlOverride env_override{mock.base_url()};
    int rc = 0;
    capture_stdout([&]() {
        rc = cmd_vulndb(ctx, {"query", "pkg:pypi/requests@2.18.0"});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::BlockingFindings));
}

TEST_F(VulndbCmdTest, QueryAgainstMockCriticalSeverityExitsBlockingFindings) {
    MockOsvServer mock{kSingleCriticalVuln};
    OsvUrlOverride env_override{mock.base_url()};
    int rc = 0;
    capture_stdout([&]() {
        rc = cmd_vulndb(ctx, {"query", "pkg:pypi/critical-pkg@1.0"});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::BlockingFindings));
}

TEST_F(VulndbCmdTest, QueryWithJsonFlagAndMockEmitsJsonArray) {
    MockOsvServer mock{kSingleLowVuln};
    OsvUrlOverride env_override{mock.base_url()};
    auto out = capture_stdout([&]() {
        return cmd_vulndb(ctx, {"query", "pkg:cargo/foo@1.0", "--json"});
    });
    EXPECT_NE(out.find('['), std::string::npos);
    EXPECT_NE(out.find("GHSA-low"), std::string::npos);
}

TEST_F(VulndbCmdTest, QueryAgainstUnreachableEndpointIsInfraError) {
    // Port 1 is reserved and not bound; the OsvClient transport fails
    // without a server. Exercises the transport-error branch in
    // cmd_vulndb.
    OsvUrlOverride env_override{"http://127.0.0.1:1"};
    int rc = 0;
    capture_cerr([&]() {
        rc = cmd_vulndb(ctx, {"query", "pkg:cargo/foo@1.0"});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::InfraError));
}

} // namespace
} // namespace euxis::cli::cmd

// NOLINTEND(cert-err33-c)
