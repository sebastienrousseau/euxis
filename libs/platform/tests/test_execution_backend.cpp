/// @file
/// @brief Tests for the local + Docker execution backends.

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "euxis/platform/execution_backend.hpp"

namespace euxis::platform {
namespace {

// ---------------------------------------------------------------------------
// LocalBackend
// ---------------------------------------------------------------------------

TEST(ExecLocalBackendTest, NameIsLocal) {
    LocalBackend b;
    EXPECT_EQ(b.name(), "local");
}

TEST(ExecLocalBackendTest, EchoReturnsStdoutAndExitZero) {
    LocalBackend b;
    ExecutionRequest req;
    req.argv = {"echo", "hello"};

    auto res = b.execute(req);
    EXPECT_FALSE(res.error.has_value()) << *res.error;
    // If exec failed the child printed "execvp failed: <strerror>" to
    // stderr before _exit(127). Surface that in the assertion message so
    // CI logs tell us *why* on Linux (issue #96).
    EXPECT_EQ(res.exit_code, 0)
        << "stderr=\"" << res.stderr_text << "\""
        << " stdout=\"" << res.stdout_text << "\""
        << " PATH=\"" << (std::getenv("PATH") ? std::getenv("PATH") : "<unset>") << "\"";
    EXPECT_EQ(res.stdout_text, "hello\n");
    EXPECT_TRUE(res.stderr_text.empty()) << res.stderr_text;
    EXPECT_EQ(res.backend_name, "local");
}

TEST(ExecLocalBackendTest, FalseReturnsNonZeroExitCode) {
    LocalBackend b;
    ExecutionRequest req;
    req.argv = {"false"};

    auto res = b.execute(req);
    EXPECT_FALSE(res.error.has_value()) << *res.error;
    EXPECT_NE(res.exit_code, 0);
}

TEST(ExecLocalBackendTest, StdinIsPipedThrough) {
    LocalBackend b;
    ExecutionRequest req;
    req.argv = {"cat"};
    req.stdin_text = "feed me\nbye\n";

    auto res = b.execute(req);
    ASSERT_FALSE(res.error.has_value()) << *res.error;
    EXPECT_EQ(res.exit_code, 0);
    EXPECT_EQ(res.stdout_text, "feed me\nbye\n");
}

TEST(ExecLocalBackendTest, EnvironmentIsPropagated) {
    LocalBackend b;
    ExecutionRequest req;
    // Use sh -c to print the env var so we don't depend on `env` being
    // installed at a specific path.
    req.argv = {"sh", "-c", "echo $EUXIS_TEST_VAR"};
    req.env  = {{"EUXIS_TEST_VAR", "value-42"}};

    auto res = b.execute(req);
    ASSERT_FALSE(res.error.has_value()) << *res.error;
    EXPECT_EQ(res.exit_code, 0);
    EXPECT_EQ(res.stdout_text, "value-42\n");
}

TEST(ExecLocalBackendTest, WorkingDirectoryIsHonoured) {
    LocalBackend b;
    ExecutionRequest req;
    req.argv = {"pwd"};
    req.working_dir = std::filesystem::path{"/tmp"};

    auto res = b.execute(req);
    ASSERT_FALSE(res.error.has_value()) << *res.error;
    EXPECT_EQ(res.exit_code, 0);
    // /tmp on macOS is a symlink to /private/tmp; on Linux it's just /tmp.
    EXPECT_TRUE(res.stdout_text == "/tmp\n" || res.stdout_text == "/private/tmp\n")
        << "actual: " << res.stdout_text;
}

TEST(ExecLocalBackendTest, TimeoutKillsLongProcess) {
    LocalBackend b;
    ExecutionRequest req;
    req.argv    = {"sleep", "5"};
    req.timeout = std::chrono::milliseconds{200};

    const auto t0 = std::chrono::steady_clock::now();
    auto res = b.execute(req);
    const auto t1 = std::chrono::steady_clock::now();

    // The child was SIGKILL'd before completion.
    EXPECT_TRUE(res.error.has_value());
    EXPECT_EQ(*res.error, "timeout");
    // Wall time should be roughly the timeout, not 5s. Allow generous
    // headroom for slow CI.
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    EXPECT_LT(elapsed_ms.count(), 2000) << "timeout did not kill the child";
}

TEST(ExecLocalBackendTest, EmptyArgvReportsBackendError) {
    LocalBackend b;
    ExecutionRequest req;  // empty argv
    auto res = b.execute(req);
    EXPECT_TRUE(res.error.has_value());
    EXPECT_EQ(res.exit_code, -1);
}

TEST(ExecLocalBackendTest, NonexistentBinaryProducesNonZeroExit) {
    LocalBackend b;
    ExecutionRequest req;
    req.argv = {"/this/path/does/not/exist/at/all"};

    auto res = b.execute(req);
    // Child exec failed; canonical exit-code 127 from our exec handler.
    EXPECT_FALSE(res.error.has_value()) << *res.error;
    EXPECT_NE(res.exit_code, 0);
    EXPECT_NE(res.stderr_text.find("execvp failed"), std::string::npos);
}

TEST(ExecLocalBackendTest, ExecutionResultDefaultsAreSafe) {
    ExecutionResult r{};
    EXPECT_EQ(r.exit_code, -1);
    EXPECT_TRUE(r.stdout_text.empty());
    EXPECT_TRUE(r.stderr_text.empty());
    EXPECT_FALSE(r.error.has_value());
}

TEST(ExecutionRequestTest, DefaultsMatchSpec) {
    ExecutionRequest req;
    EXPECT_EQ(req.timeout.count(), 60'000);
    EXPECT_EQ(req.docker_image, "alpine:latest");
    EXPECT_FALSE(req.stdin_text.has_value());
}

// ---------------------------------------------------------------------------
// DockerBackend — skipped when docker isn't available
// ---------------------------------------------------------------------------

TEST(DockerBackendTest, NameIsDocker) {
    DockerBackend b;
    EXPECT_EQ(b.name(), "docker");
}

TEST(DockerBackendTest, IsAvailableProbesQuickly) {
    // Either docker is on PATH or not — both outcomes are valid.
    const auto t0 = std::chrono::steady_clock::now();
    const bool avail = DockerBackend::is_available();
    const auto t1 = std::chrono::steady_clock::now();
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);

    // The probe has a 2-second internal timeout; give the platform some
    // headroom but reject anything obviously stuck.
    EXPECT_LT(ms.count(), 5'000);
    // Use the value so the compiler doesn't elide the call.
    (void)avail;
}

TEST(DockerBackendTest, ExecuteSkipsCleanlyWhenDockerAbsent) {
    if (DockerBackend::is_available()) {
        GTEST_SKIP() << "docker is available — covered by other tests";
    }
    DockerBackend b;
    ExecutionRequest req;
    req.argv = {"echo", "hi"};
    auto res = b.execute(req);
    EXPECT_TRUE(res.error.has_value());
    EXPECT_EQ(res.backend_name, "docker");
}

TEST(DockerBackendTest, EchoInsideAlpineWhenAvailable) {
    if (!DockerBackend::is_available()) {
        GTEST_SKIP() << "docker not available on this host";
    }
    DockerBackend b;
    ExecutionRequest req;
    req.argv = {"echo", "from-container"};
    req.docker_image = "alpine:latest";
    req.timeout = std::chrono::milliseconds{60'000};

    auto res = b.execute(req);
    EXPECT_FALSE(res.error.has_value()) << *res.error;
    EXPECT_EQ(res.exit_code, 0);
    EXPECT_NE(res.stdout_text.find("from-container"), std::string::npos);
    EXPECT_EQ(res.backend_name, "docker");
}

TEST(DockerBackendTest, EmptyArgvReportsBackendError) {
    if (!DockerBackend::is_available()) {
        GTEST_SKIP() << "docker not available on this host";
    }
    DockerBackend b;
    ExecutionRequest req;  // empty argv
    auto res = b.execute(req);
    EXPECT_TRUE(res.error.has_value());
}

} // namespace
} // namespace euxis::platform
