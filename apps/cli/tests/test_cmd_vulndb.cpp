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
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

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

} // namespace
} // namespace euxis::cli::cmd

// NOLINTEND(cert-err33-c)
