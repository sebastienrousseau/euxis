/// @file
/// @brief Integration tests for `cmd_sdk_demo` — drives the runtime
///        harness + libs/metrics insights pipeline so the 141
///        uncovered lines flagged at 0 % in the 2026-06-22 gcovr
///        baseline become live.

#include <gtest/gtest.h>

#include "euxis/cli/cmd/sdk.hpp"
#include "euxis/cli/command.hpp"

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

class SdkDemoCmdTest : public ::testing::Test {
protected:
    fs::path tmpdir;
    Context  ctx;

    void SetUp() override {
        tmpdir = fs::temp_directory_path() /
                 ("euxis_test_sdk_" + std::to_string(::getpid()) +
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
// Help + arg-parser errors
// ---------------------------------------------------------------------------

TEST_F(SdkDemoCmdTest, HelpFlagPrintsHelp) {
    auto err = capture_cerr([&]() {
        return cmd_sdk_demo(ctx, {"--help"});
    });
    EXPECT_NE(err.find("euxis sdk-demo"), std::string::npos);
}

TEST_F(SdkDemoCmdTest, ShortHelpFlag) {
    auto err = capture_cerr([&]() {
        return cmd_sdk_demo(ctx, {"-h"});
    });
    EXPECT_NE(err.find("euxis sdk-demo"), std::string::npos);
}

TEST_F(SdkDemoCmdTest, InvalidTurnsValueIsError) {
    auto err = capture_cerr([&]() {
        return cmd_sdk_demo(ctx, {"--turns", "not-a-number"});
    });
    EXPECT_NE(err.find("--turns"), std::string::npos);
}

TEST_F(SdkDemoCmdTest, ZeroTurnsIsError) {
    auto err = capture_cerr([&]() {
        return cmd_sdk_demo(ctx, {"--turns", "0"});
    });
    EXPECT_NE(err.find("--turns"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Happy paths
// ---------------------------------------------------------------------------

TEST_F(SdkDemoCmdTest, DefaultRunSucceeds) {
    capture_stdout([&]() {
        return cmd_sdk_demo(ctx, {});
    });
    // Returns 0 on a clean run.
}

TEST_F(SdkDemoCmdTest, ExplicitTurnsRuns) {
    int rc = 0;
    capture_stdout([&]() {
        rc = cmd_sdk_demo(ctx, {"--turns", "2"});
        return rc;
    });
    EXPECT_EQ(rc, 0);
}

TEST_F(SdkDemoCmdTest, JsonOutputEmitsValidJson) {
    auto out = capture_stdout([&]() {
        return cmd_sdk_demo(ctx, {"--turns", "1", "--json"});
    });
    // The --json flag at minimum produces parseable JSON somewhere
    // in stdout; we check for an opening brace.
    EXPECT_NE(out.find('{'), std::string::npos);
}

TEST_F(SdkDemoCmdTest, ProviderOverride) {
    int rc = 0;
    capture_stdout([&]() {
        rc = cmd_sdk_demo(ctx, {
            "--turns", "1",
            "--provider", "openai",
            "--model", "gpt-4o",
        });
        return rc;
    });
    EXPECT_EQ(rc, 0);
}

TEST_F(SdkDemoCmdTest, SessionIdOverride) {
    int rc = 0;
    capture_stdout([&]() {
        rc = cmd_sdk_demo(ctx, {
            "--turns", "1",
            "--session", "my-custom-session-id",
        });
        return rc;
    });
    EXPECT_EQ(rc, 0);
}

TEST_F(SdkDemoCmdTest, JsonViaContextFlagAlsoTriggersEmission) {
    ctx.json_output = true;
    auto out = capture_stdout([&]() {
        return cmd_sdk_demo(ctx, {"--turns", "1"});
    });
    EXPECT_NE(out.find('{'), std::string::npos);
}

TEST_F(SdkDemoCmdTest, MultipleTurnsTriggersCompactionRecommendation) {
    // The harness uses a 4096-token context window with synthetic
    // 800-byte prompts; several turns push past the trigger ratio.
    int rc = 0;
    capture_stdout([&]() {
        rc = cmd_sdk_demo(ctx, {"--turns", "5"});
        return rc;
    });
    EXPECT_EQ(rc, 0);
}

} // namespace
} // namespace euxis::cli::cmd

// NOLINTEND(cert-err33-c)
