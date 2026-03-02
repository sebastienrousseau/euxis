#include "euxis/cli/process.hpp"

#include <gtest/gtest.h>
#include <filesystem>

namespace euxis::cli {
namespace {

TEST(ProcessTest, RunEchoReturnsOutput) {
    auto result = Process::run("echo", {"hello world"}, 5);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_FALSE(result.stdout_output.empty());
    EXPECT_NE(result.stdout_output.find("hello world"), std::string::npos);
}

TEST(ProcessTest, RunNonexistentProgram) {
    auto result = Process::run("/nonexistent/program/xyz", {}, 5);
    EXPECT_NE(result.exit_code, 0);
}

TEST(ProcessTest, RunCapturesStderr) {
    auto result = Process::shell("echo error >&2", 5);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.stderr_output.find("error"), std::string::npos);
}

TEST(ProcessTest, RunExitCode) {
    auto result = Process::shell("exit 42", 5);
    EXPECT_EQ(result.exit_code, 42);
}

TEST(ProcessTest, ShellRunsCommand) {
    auto result = Process::shell("echo shell_test", 5);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.stdout_output.find("shell_test"), std::string::npos);
}

TEST(ProcessTest, RunWithInputPipesStdin) {
    auto result = Process::run_with_input("cat", {}, "piped_data", 5);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.stdout_output.find("piped_data"), std::string::npos);
}

TEST(ProcessTest, WhichFindsLs) {
    auto path = Process::which("ls");
    ASSERT_TRUE(path.has_value());
    EXPECT_TRUE(std::filesystem::exists(*path));
}

TEST(ProcessTest, WhichReturnsNulloptForNonexistent) {
    auto path = Process::which("totally_nonexistent_binary_xyz_123");
    EXPECT_FALSE(path.has_value());
}

TEST(ProcessTest, AvailableForLs) {
    EXPECT_TRUE(Process::available("ls"));
}

TEST(ProcessTest, AvailableReturnsFalseForNonexistent) {
    EXPECT_FALSE(Process::available("totally_nonexistent_binary_xyz_123"));
}

TEST(ProcessTest, RunWithArgs) {
    auto result = Process::run("printf", {"%s-%s", "foo", "bar"}, 5);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.stdout_output.find("foo-bar"), std::string::npos);
}

TEST(ProcessTest, RunWithInputEmpty) {
    auto result = Process::run_with_input("cat", {}, "", 5);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(result.stdout_output.empty());
}

TEST(ProcessTest, RunWithInputLargeData) {
    std::string large_data(8192, 'A');
    auto result = Process::run_with_input("wc", {"-c"}, large_data, 5);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.stdout_output.find("8192"), std::string::npos);
}

TEST(ProcessTest, RunSignalledProcess) {
    // Kill self with signal 9 — exit code should be 128+9=137
    auto result = Process::shell("kill -9 $$", 5);
    EXPECT_EQ(result.exit_code, 137);
}

TEST(ProcessTest, ShellWithPipeline) {
    auto result = Process::shell("echo 'abc def' | wc -w", 5);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.stdout_output.find("2"), std::string::npos);
}

TEST(ProcessTest, WhichFindsBash) {
    auto path = Process::which("bash");
    // Bash may or may not exist, but should not crash
    if (path.has_value()) {
        EXPECT_TRUE(std::filesystem::exists(*path));
    }
}

TEST(ProcessTest, RunWithMultilineOutput) {
    auto result = Process::shell("echo line1; echo line2; echo line3", 5);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.stdout_output.find("line1"), std::string::npos);
    EXPECT_NE(result.stdout_output.find("line2"), std::string::npos);
    EXPECT_NE(result.stdout_output.find("line3"), std::string::npos);
}

TEST(ProcessTest, RunWithInputNonexistentProgram) {
    auto result = Process::run_with_input("/nonexistent/bin/xyz", {}, "data", 5);
    EXPECT_NE(result.exit_code, 0);
}

TEST(ProcessTest, RunEmptyArgs) {
    auto result = Process::run("true", {}, 5);
    EXPECT_EQ(result.exit_code, 0);
}

TEST(ProcessTest, RunFalseExitCode) {
    auto result = Process::run("false", {}, 5);
    EXPECT_EQ(result.exit_code, 1);
}

TEST(ProcessTest, AvailableForEcho) {
    EXPECT_TRUE(Process::available("echo"));
}

// --- Coverage: timeout handling (deadline exceeded, kills child) ---
TEST(ProcessTest, RunWithTimeout) {
    // Sleep for 10 seconds but timeout after 1 second
    auto result = Process::run("sleep", {"10"}, 1);
    // Should be killed by timeout
    EXPECT_NE(result.exit_code, 0);
}

// --- Coverage: run_with_input signal handling (WIFSIGNALED) ---
TEST(ProcessTest, RunWithInputSignalledProcess) {
    auto result = Process::run_with_input("sh", {"-c", "kill -9 $$"}, "", 5);
    EXPECT_EQ(result.exit_code, 137); // 128 + 9
}

// --- Coverage: run_with_input timeout ---
TEST(ProcessTest, RunWithInputTimeout) {
    auto result = Process::run_with_input("sleep", {"10"}, "", 1);
    EXPECT_NE(result.exit_code, 0);
}

// --- Coverage: run_with_input with data to stdin and multiline output ---
TEST(ProcessTest, RunWithInputMultilineOutput) {
    auto result = Process::run_with_input("sh", {"-c", "cat; echo done"}, "input data\n", 5);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.stdout_output.find("input data"), std::string::npos);
    EXPECT_NE(result.stdout_output.find("done"), std::string::npos);
}

// --- Coverage: which with PATH env ---
TEST(ProcessTest, WhichReturnsAbsPath) {
    auto path = Process::which("echo");
    if (path.has_value()) {
        EXPECT_TRUE(path->starts_with("/"));
    }
}

// --- Coverage: run with stderr and stdout simultaneously ---
TEST(ProcessTest, RunCapturesBothStreams) {
    auto result = Process::shell("echo out; echo err >&2", 5);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.stdout_output.find("out"), std::string::npos);
    EXPECT_NE(result.stderr_output.find("err"), std::string::npos);
}

} // namespace
} // namespace euxis::cli
