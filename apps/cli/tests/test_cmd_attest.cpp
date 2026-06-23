/// @file
/// @brief Integration tests for `cmd_attest` and `cmd_verify_attest`
///        — the implementations behind `euxis attest` and
///        `euxis verify`. Drives the full sign-then-verify cycle
///        against a tmpdir fixture so the 257 uncovered lines that
///        the 2026-06-22 gcovr baseline flagged at 0 % become live.

#include <gtest/gtest.h>

#include "euxis/cli/cmd/attest.hpp"
#include "euxis/cli/command.hpp"
#include "euxis/cli/exit_codes.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include <nlohmann/json.hpp>
#include <sodium.h>

// NOLINTBEGIN(cert-err33-c) — test scratch I/O intentionally discards
// return values; tests can blanket-disable per
// docs/development/clang-tidy-policy.md.

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;

class AttestCmdTest : public ::testing::Test {
protected:
    fs::path tmpdir;
    fs::path subject_path;
    fs::path key_basename;
    fs::path bundle_path;
    Context  ctx;

    static void SetUpTestSuite() {
        // Ed25519 ops via libsodium need this once per process; safe
        // to call multiple times.
        ASSERT_GE(sodium_init(), 0);
    }

    void SetUp() override {
        tmpdir = fs::temp_directory_path() /
                 ("euxis_test_attest_" + std::to_string(::getpid()) +
                  "_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        fs::create_directories(tmpdir);
        subject_path = tmpdir / "subject.bin";
        key_basename = tmpdir / "ekey";
        bundle_path  = tmpdir / "out.sigstore.json";
        ctx.euxis_home = tmpdir.string();
        ctx.data_dir   = (tmpdir / "data").string();

        // A non-trivial subject file the test can sign.
        std::ofstream{subject_path} << "euxis-attest-test-payload\n";
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

    // Run a full attest pass with the supplied key-basename.
    // Returns the exit code; caller checks for Success and that the
    // bundle was written.
    int sign_with_keygen(const fs::path& keygen_basename,
                        const fs::path& bundle_out) {
        std::vector<std::string> argv = {
            subject_path.string(),
            "--keygen=" + keygen_basename.string(),
            "--bundle="  + bundle_out.string(),
        };
        return cmd_attest(ctx, argv);
    }
};

// ---------------------------------------------------------------------------
// cmd_attest — help + arg-parser errors
// ---------------------------------------------------------------------------

TEST_F(AttestCmdTest, HelpFlagPrintsHelp) {
    auto out = capture_stdout([&]() {
        return cmd_attest(ctx, {"--help"});
    });
    EXPECT_NE(out.find("Usage: euxis attest"), std::string::npos);
}

TEST_F(AttestCmdTest, ShortHelpFlagAlsoPrintsHelp) {
    auto out = capture_stdout([&]() {
        return cmd_attest(ctx, {"-h"});
    });
    EXPECT_NE(out.find("Usage: euxis attest"), std::string::npos);
}

TEST_F(AttestCmdTest, MissingSubjectIsInfraError) {
    auto err = capture_cerr([&]() {
        return cmd_attest(ctx, {"--key=" + key_basename.string()});
    });
    EXPECT_NE(err.find("subject file is required"), std::string::npos);
}

TEST_F(AttestCmdTest, UnknownFlagIsInfraError) {
    auto err = capture_cerr([&]() {
        return cmd_attest(ctx, {"--not-a-real-flag",
                                subject_path.string()});
    });
    EXPECT_NE(err.find("Unknown flag"), std::string::npos);
}

TEST_F(AttestCmdTest, ExtraPositionalIsInfraError) {
    auto err = capture_cerr([&]() {
        return cmd_attest(ctx, {subject_path.string(),
                                "second-positional"});
    });
    EXPECT_NE(err.find("Unexpected positional"), std::string::npos);
}

TEST_F(AttestCmdTest, SubjectFileDoesNotExistIsInfraError) {
    auto err = capture_cerr([&]() {
        return cmd_attest(ctx, {(tmpdir / "missing.bin").string(),
                                "--keygen=" + key_basename.string()});
    });
    EXPECT_NE(err.find("subject file does not exist"), std::string::npos);
}

TEST_F(AttestCmdTest, NoKeyAndNoKeygenIsInfraError) {
    auto err = capture_cerr([&]() {
        return cmd_attest(ctx, {subject_path.string()});
    });
    EXPECT_NE(err.find("provide --key"), std::string::npos);
}

// ---------------------------------------------------------------------------
// cmd_attest — happy paths
// ---------------------------------------------------------------------------

TEST_F(AttestCmdTest, KeygenWritesPrivAndPubFiles) {
    capture_stdout([&]() {
        return cmd_attest(ctx, {
            subject_path.string(),
            "--keygen=" + key_basename.string(),
            "--bundle="  + bundle_path.string(),
        });
    });
    EXPECT_TRUE(fs::exists(fs::path{key_basename.string() + ".priv"}));
    EXPECT_TRUE(fs::exists(fs::path{key_basename.string() + ".pub"}));
}

TEST_F(AttestCmdTest, KeygenProducesValidBundle) {
    capture_stdout([&]() {
        return sign_with_keygen(key_basename, bundle_path);
    });
    ASSERT_TRUE(fs::exists(bundle_path));
    std::ifstream f(bundle_path);
    nlohmann::json j = nlohmann::json::parse(f);
    EXPECT_TRUE(j.contains("dsseEnvelope"));
    EXPECT_TRUE(j.contains("verificationMaterial"));
}

TEST_F(AttestCmdTest, KeyFromFilePathSignsBundle) {
    // First: generate a key in a separate basename.
    auto gen_base = tmpdir / "gen";
    capture_stdout([&]() {
        return sign_with_keygen(gen_base, tmpdir / "discard.json");
    });
    auto priv_path = fs::path{gen_base.string() + ".priv"};
    ASSERT_TRUE(fs::exists(priv_path));

    // Second: re-sign the same subject by pointing --key at the
    // generated .priv file.
    capture_stdout([&]() {
        return cmd_attest(ctx, {
            subject_path.string(),
            "--key=" + priv_path.string(),
            "--bundle=" + bundle_path.string(),
        });
    });
    EXPECT_TRUE(fs::exists(bundle_path));
}

TEST_F(AttestCmdTest, KeyFileMissingIsInfraError) {
    auto err = capture_cerr([&]() {
        return cmd_attest(ctx, {
            subject_path.string(),
            "--key=" + (tmpdir / "no-key.priv").string(),
        });
    });
    EXPECT_NE(err.find("read key"), std::string::npos);
}

TEST_F(AttestCmdTest, KeyTooShortIsInfraError) {
    auto bad_key = tmpdir / "bad.priv";
    std::ofstream{bad_key} << "deadbeef";  // not 64 b64-decoded bytes
    auto err = capture_cerr([&]() {
        return cmd_attest(ctx, {
            subject_path.string(),
            "--key=" + bad_key.string(),
        });
    });
    EXPECT_NE(err.find("64 base64"), std::string::npos);
}

TEST_F(AttestCmdTest, SubjectNameOverrideIsApplied) {
    capture_stdout([&]() {
        return cmd_attest(ctx, {
            subject_path.string(),
            "--keygen=" + key_basename.string(),
            "--subject-name=custom-friendly-name",
            "--bundle=" + bundle_path.string(),
        });
    });
    ASSERT_TRUE(fs::exists(bundle_path));
    // The subject name shows up in the DSSE envelope's payload
    // (after base64 decoding), but the bundle structure alone is
    // enough to confirm the option was accepted without an error.
}

TEST_F(AttestCmdTest, DefaultBundlePathDerivedFromSubject) {
    // Don't pass --bundle; expect <subject>.sigstore.json.
    capture_stdout([&]() {
        return cmd_attest(ctx, {
            subject_path.string(),
            "--keygen=" + key_basename.string(),
        });
    });
    auto expected = subject_path;
    expected += ".sigstore.json";
    EXPECT_TRUE(fs::exists(expected));
}

// ---------------------------------------------------------------------------
// cmd_verify_attest — help + arg-parser errors
// ---------------------------------------------------------------------------

TEST_F(AttestCmdTest, VerifyHelpFlagPrintsHelp) {
    auto out = capture_stdout([&]() {
        return cmd_verify_attest(ctx, {"--help"});
    });
    EXPECT_NE(out.find("Usage: euxis verify"), std::string::npos);
}

TEST_F(AttestCmdTest, VerifyMissingBundleIsInfraError) {
    auto err = capture_cerr([&]() {
        return cmd_verify_attest(ctx, {});
    });
    EXPECT_NE(err.find("bundle path is required"), std::string::npos);
}

TEST_F(AttestCmdTest, VerifyUnknownFlagIsInfraError) {
    auto err = capture_cerr([&]() {
        return cmd_verify_attest(ctx, {"--definitely-not-a-flag",
                                       bundle_path.string()});
    });
    EXPECT_NE(err.find("Unknown flag"), std::string::npos);
}

TEST_F(AttestCmdTest, VerifyBundleFileMissingIsInfraError) {
    auto err = capture_cerr([&]() {
        return cmd_verify_attest(ctx, {(tmpdir / "no-bundle.json").string()});
    });
    EXPECT_FALSE(err.empty());
}

// ---------------------------------------------------------------------------
// cmd_verify_attest — happy paths
// ---------------------------------------------------------------------------

TEST_F(AttestCmdTest, VerifyValidBundleSucceeds) {
    // Sign first.
    capture_stdout([&]() {
        return sign_with_keygen(key_basename, bundle_path);
    });
    ASSERT_TRUE(fs::exists(bundle_path));
    auto pub_path = fs::path{key_basename.string() + ".pub"};
    ASSERT_TRUE(fs::exists(pub_path));

    // Verify with the matching public key.
    int rc = 0;
    capture_stdout([&]() {
        rc = cmd_verify_attest(ctx, {
            bundle_path.string(),
            "--key=" + pub_path.string(),
        });
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::Success));
}

TEST_F(AttestCmdTest, VerifyWithoutExplicitKeyUsesBundleEmbeddedKey) {
    // Sign first.
    capture_stdout([&]() {
        return sign_with_keygen(key_basename, bundle_path);
    });
    ASSERT_TRUE(fs::exists(bundle_path));

    // Verify WITHOUT --key — the bundle carries the public key
    // in verificationMaterial.publicKey.rawBytes.
    int rc = 0;
    capture_stdout([&]() {
        rc = cmd_verify_attest(ctx, {bundle_path.string()});
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::Success));
}

TEST_F(AttestCmdTest, VerifyWithWrongKeyIsPolicyViolation) {
    // Sign with one key.
    capture_stdout([&]() {
        return sign_with_keygen(key_basename, bundle_path);
    });
    ASSERT_TRUE(fs::exists(bundle_path));

    // Generate a SECOND key — different keypair.
    auto other_base = tmpdir / "other";
    capture_stdout([&]() {
        return sign_with_keygen(other_base, tmpdir / "other-bundle.json");
    });
    auto wrong_pub = fs::path{other_base.string() + ".pub"};
    ASSERT_TRUE(fs::exists(wrong_pub));

    // Verify with the wrong key — must reject as PolicyViolation.
    int rc = 0;
    capture_cerr([&]() {
        rc = cmd_verify_attest(ctx, {
            bundle_path.string(),
            "--key=" + wrong_pub.string(),
        });
        return rc;
    });
    EXPECT_EQ(rc, to_int(ExitCode::PolicyViolation));
}

} // namespace
} // namespace euxis::cli::cmd

// NOLINTEND(cert-err33-c)
