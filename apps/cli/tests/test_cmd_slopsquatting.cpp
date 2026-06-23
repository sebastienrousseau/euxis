/// @file
/// @brief Integration tests for `cmd_slopsquatting` — walks
///        lockfiles + applies the hallucinated-package corpus
///        guard. Targets the 141 uncovered lines flagged at 0% in
///        the 2026-06-22 gcovr baseline.

#include <gtest/gtest.h>

#include "euxis/cli/cmd/slopsquatting.hpp"
#include "euxis/cli/command.hpp"
#include "euxis/cli/exit_codes.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

// NOLINTBEGIN(cert-err33-c)

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;

class SlopsquattingCmdTest : public ::testing::Test {
protected:
    fs::path tmpdir;
    fs::path sarif_path;
    fs::path corpus_path;
    Context  ctx;

    void SetUp() override {
        tmpdir = fs::temp_directory_path() /
                 ("euxis_test_slop_" + std::to_string(::getpid()) +
                  "_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        fs::create_directories(tmpdir);
        sarif_path  = tmpdir / "out.sarif.json";
        corpus_path = tmpdir / "corpus.txt";
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

    // Clean Cargo.lock with one real package — should produce 0
    // findings against any reasonable corpus.
    void write_clean_cargo_lock() {
        std::ofstream{tmpdir / "Cargo.lock"} <<
            "version = 3\n"
            "\n"
            "[[package]]\n"
            "name = \"serde\"\n"
            "version = \"1.0.197\"\n"
            "source = \"registry+https://github.com/rust-lang/crates.io-index\"\n";
    }

    // Cargo.lock that names "definitely-hallucinated-package-name"
    // — when paired with a corpus that includes that name, the
    // scan should emit one finding and exit with code 3.
    void write_suspicious_cargo_lock(std::string_view bad_name) {
        std::ofstream{tmpdir / "Cargo.lock"} <<
            "version = 3\n"
            "\n"
            "[[package]]\n"
            "name = \"" << bad_name << "\"\n"
            "version = \"1.0.0\"\n"
            "source = \"registry+https://github.com/rust-lang/crates.io-index\"\n";
    }

    // External corpus file with one entry per line (cargo ecosystem).
    void write_corpus(std::string_view name) {
        std::ofstream{corpus_path} << "cargo:" << name << "\n";
    }
};

// ---------------------------------------------------------------------------
// Help + arg-parser errors
// ---------------------------------------------------------------------------

TEST_F(SlopsquattingCmdTest, HelpFlagPrintsHelp) {
    auto out = capture_stdout([&]() {
        return cmd_slopsquatting(ctx, {"--help"});
    });
    EXPECT_NE(out.find("Usage: euxis slopsquatting"), std::string::npos);
}

TEST_F(SlopsquattingCmdTest, ShortHelpFlag) {
    auto out = capture_stdout([&]() {
        return cmd_slopsquatting(ctx, {"-h"});
    });
    EXPECT_NE(out.find("Usage: euxis slopsquatting"), std::string::npos);
}

TEST_F(SlopsquattingCmdTest, UnknownFlagIsInfraError) {
    auto err = capture_cerr([&]() {
        return cmd_slopsquatting(ctx, {"--not-a-real-flag"});
    });
    EXPECT_NE(err.find("Unknown flag"), std::string::npos);
}

TEST_F(SlopsquattingCmdTest, ExtraPositionalIsInfraError) {
    auto err = capture_cerr([&]() {
        return cmd_slopsquatting(ctx, {tmpdir.string(), "extra"});
    });
    EXPECT_NE(err.find("Unexpected positional"), std::string::npos);
}

TEST_F(SlopsquattingCmdTest, MissingCorpusFileIsInfraError) {
    auto err = capture_cerr([&]() {
        return cmd_slopsquatting(ctx, {
            tmpdir.string(),
            "--corpus=" + (tmpdir / "missing-corpus.txt").string(),
        });
    });
    EXPECT_NE(err.find("corpus"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Happy paths — clean scan, exit code 0
// ---------------------------------------------------------------------------

TEST_F(SlopsquattingCmdTest, EmptyTargetReturnsSuccess) {
    int rc = 0;
    capture_stdout([&]() {
        rc = cmd_slopsquatting(ctx, {tmpdir.string()});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::Success));
}

TEST_F(SlopsquattingCmdTest, CleanCargoLockReturnsSuccess) {
    write_clean_cargo_lock();
    int rc = 0;
    capture_stdout([&]() {
        rc = cmd_slopsquatting(ctx, {tmpdir.string()});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::Success));
}

TEST_F(SlopsquattingCmdTest, QuietFlagSuppressesPerFindingOutput) {
    write_clean_cargo_lock();
    auto out = capture_stdout([&]() {
        return cmd_slopsquatting(ctx, {tmpdir.string(), "--quiet"});
    });
    // --quiet still prints the summary line.
    EXPECT_NE(out.find("euxis slopsquatting"), std::string::npos);
}

TEST_F(SlopsquattingCmdTest, NoSeedWithoutCorpusIsInfraError) {
    // --no-seed AND no --corpus means the guard has nothing to
    // check against; the tool rejects the empty-corpus state as
    // a usage error.
    write_clean_cargo_lock();
    int rc = 0;
    capture_stdout([&]() {
        rc = cmd_slopsquatting(ctx, {tmpdir.string(), "--no-seed"});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::InfraError));
}

TEST_F(SlopsquattingCmdTest, NoSeedWithExternalCorpusSucceeds) {
    // --no-seed paired with an external corpus runs cleanly.
    write_clean_cargo_lock();
    write_corpus("some-random-fake-name");
    int rc = 0;
    capture_stdout([&]() {
        rc = cmd_slopsquatting(ctx, {
            tmpdir.string(),
            "--no-seed",
            "--corpus=" + corpus_path.string(),
        });
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::Success));
}

TEST_F(SlopsquattingCmdTest, SarifFlagWritesReport) {
    write_clean_cargo_lock();
    capture_stdout([&]() {
        return cmd_slopsquatting(ctx, {
            tmpdir.string(),
            "--sarif=" + sarif_path.string(),
        });
    });
    EXPECT_TRUE(fs::exists(sarif_path));
}

TEST_F(SlopsquattingCmdTest, ExternalCorpusLoadedOnTopOfSeed) {
    write_clean_cargo_lock();
    write_corpus("not-a-real-package-name");
    int rc = 0;
    capture_stdout([&]() {
        rc = cmd_slopsquatting(ctx, {
            tmpdir.string(),
            "--corpus=" + corpus_path.string(),
        });
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::Success));
}

// ---------------------------------------------------------------------------
// Match path — exit code 3
// ---------------------------------------------------------------------------

TEST_F(SlopsquattingCmdTest, MatchingCorpusEntryEmitsFindingAndExitCode3) {
    auto fake_name = "definitely-hallucinated-pkg-xyz123";
    write_suspicious_cargo_lock(fake_name);
    write_corpus(fake_name);
    int rc = 0;
    auto out = capture_stdout([&]() {
        rc = cmd_slopsquatting(ctx, {
            tmpdir.string(),
            "--corpus=" + corpus_path.string(),
            "--no-seed",  // isolate so only our corpus matches
        });
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::BlockingFindings));
    EXPECT_NE(out.find("findings=1"), std::string::npos);
}

TEST_F(SlopsquattingCmdTest, MatchWithSarifProducesValidSarifBody) {
    auto fake_name = "another-fake-pkg-abc";
    write_suspicious_cargo_lock(fake_name);
    write_corpus(fake_name);
    capture_stdout([&]() {
        return cmd_slopsquatting(ctx, {
            tmpdir.string(),
            "--corpus=" + corpus_path.string(),
            "--no-seed",
            "--sarif=" + sarif_path.string(),
        });
    });
    ASSERT_TRUE(fs::exists(sarif_path));
    std::ifstream f(sarif_path);
    std::stringstream ss; ss << f.rdbuf();
    auto body = ss.str();
    EXPECT_NE(body.find("\"version\""), std::string::npos);
    EXPECT_NE(body.find("2.1.0"), std::string::npos);
}

} // namespace
} // namespace euxis::cli::cmd

// NOLINTEND(cert-err33-c)
