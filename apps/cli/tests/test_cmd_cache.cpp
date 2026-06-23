/// @file
/// @brief Integration tests for `cmd_cache` — covers all four
///        subcommands (stats / purge / purge-older / path) plus
///        the dispatcher's help / unknown-subcommand paths.

#include <gtest/gtest.h>

#include "euxis/cli/cmd/cache.hpp"
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

class CacheCmdTest : public ::testing::Test {
protected:
    fs::path tmpdir;
    fs::path db_path;
    Context  ctx;

    void SetUp() override {
        tmpdir = fs::temp_directory_path() /
                 ("euxis_test_cache_" + std::to_string(::getpid()) +
                  "_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        fs::create_directories(tmpdir);
        db_path = tmpdir / "test-cache.sqlite";
        // Sandbox XDG_CACHE_HOME so default-db resolution stays inside
        // tmpdir even on tests that don't pass --db.
        ::setenv("XDG_CACHE_HOME", (tmpdir / "xdg-cache").string().c_str(), 1);
        ctx.euxis_home = tmpdir.string();
        ctx.data_dir   = (tmpdir / "data").string();
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tmpdir, ec);
        ::unsetenv("XDG_CACHE_HOME");
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

    std::string db_arg() const { return "--db=" + db_path.string(); }
};

// ---------------------------------------------------------------------------
// Dispatcher — help + unknown subcommand
// ---------------------------------------------------------------------------

TEST_F(CacheCmdTest, HelpSubcommandPrintsHelp) {
    // The dispatcher only treats the literal "help" string as the
    // help subcommand — `--help` and `-h` start with "-" and fall
    // through into rest[] so they reach the (default) stats path.
    // This matches the current implementation in cache.cpp:171-174.
    auto out = capture_stdout([&]() {
        return cmd_cache(ctx, {"help"});
    });
    EXPECT_NE(out.find("Subcommands"), std::string::npos);
}

TEST_F(CacheCmdTest, UnknownSubcommandIsInfraError) {
    int rc = 0;
    capture_cerr([&]() {
        rc = cmd_cache(ctx, {"bogus-subcommand"});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::InfraError));
}

// ---------------------------------------------------------------------------
// path subcommand
// ---------------------------------------------------------------------------

TEST_F(CacheCmdTest, PathSubcommandPrintsDbPath) {
    auto out = capture_stdout([&]() {
        return cmd_cache(ctx, {"path", db_arg()});
    });
    EXPECT_NE(out.find(db_path.string()), std::string::npos);
}

TEST_F(CacheCmdTest, PathDefaultUsesXdgCacheHome) {
    // No --db: resolve_db should derive from XDG_CACHE_HOME.
    auto out = capture_stdout([&]() {
        return cmd_cache(ctx, {"path"});
    });
    EXPECT_NE(out.find("xdg-cache"), std::string::npos);
}

// ---------------------------------------------------------------------------
// stats subcommand — opens a fresh db, asserts entries=0
// ---------------------------------------------------------------------------

TEST_F(CacheCmdTest, StatsOnFreshDbReportsZeroEntries) {
    int rc = 0;
    auto out = capture_stdout([&]() {
        rc = cmd_cache(ctx, {"stats", db_arg()});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::Success));
    EXPECT_NE(out.find("entries: 0"), std::string::npos);
}

TEST_F(CacheCmdTest, StatsIsTheDefaultSubcommand) {
    int rc = 0;
    auto out = capture_stdout([&]() {
        rc = cmd_cache(ctx, {db_arg()});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::Success));
    EXPECT_NE(out.find("entries"), std::string::npos);
}

// ---------------------------------------------------------------------------
// purge subcommand
// ---------------------------------------------------------------------------

TEST_F(CacheCmdTest, PurgeOnFreshDbSucceeds) {
    int rc = 0;
    auto out = capture_stdout([&]() {
        rc = cmd_cache(ctx, {"purge", db_arg()});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::Success));
    EXPECT_NE(out.find("removed"), std::string::npos);
}

// ---------------------------------------------------------------------------
// purge-older subcommand
// ---------------------------------------------------------------------------

TEST_F(CacheCmdTest, PurgeOlderDefaultTtlSucceeds) {
    int rc = 0;
    auto out = capture_stdout([&]() {
        rc = cmd_cache(ctx, {"purge-older", db_arg()});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::Success));
    EXPECT_NE(out.find("removed"), std::string::npos);
}

TEST_F(CacheCmdTest, PurgeOlderExplicitTtlSucceeds) {
    int rc = 0;
    capture_stdout([&]() {
        rc = cmd_cache(ctx, {"purge-older", "--ttl-days=7", db_arg()});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::Success));
}

TEST_F(CacheCmdTest, PurgeOlderZeroTtlIsInfraError) {
    int rc = 0;
    capture_cerr([&]() {
        rc = cmd_cache(ctx, {"purge-older", "--ttl-days=0", db_arg()});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::InfraError));
}

TEST_F(CacheCmdTest, PurgeOlderNegativeTtlIsInfraError) {
    int rc = 0;
    capture_cerr([&]() {
        rc = cmd_cache(ctx, {"purge-older", "--ttl-days=-5", db_arg()});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::InfraError));
}

} // namespace
} // namespace euxis::cli::cmd

// NOLINTEND(cert-err33-c)
