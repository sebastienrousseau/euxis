// Copyright (c) 2026 The Euxis Authors
// SPDX-License-Identifier: AGPL-3.0-only

// The claude provider runs through the user's Claude Code CLI session.
// On macOS that session lives in the Keychain, not ~/.claude/.credentials.json,
// so euxis found no OAuth profile, fell back to an API key, and every claude
// agent failed with "No Anthropic auth found." These tests put a stub `claude`
// first on PATH that records its environment and arguments, so no real CLI,
// login or network is touched.

#include "euxis/cli/provider_executor.hpp"

#include <gtest/gtest.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unistd.h>

namespace euxis::cli {
namespace {

namespace fs = std::filesystem;

// Saves an environment variable and restores it on destruction.
class EnvGuard {
public:
    explicit EnvGuard(std::string name) : name_(std::move(name)) {
        if (const char* v = std::getenv(name_.c_str())) saved_ = v;
    }
    EnvGuard(const EnvGuard&) = delete;
    EnvGuard& operator=(const EnvGuard&) = delete;
    EnvGuard(EnvGuard&&) = delete;
    EnvGuard& operator=(EnvGuard&&) = delete;
    ~EnvGuard() {
        if (saved_) ::setenv(name_.c_str(), saved_->c_str(), 1);
        else ::unsetenv(name_.c_str());
    }

private:
    std::string name_;
    std::optional<std::string> saved_;
};

class ClaudeCliSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        root_ = fs::temp_directory_path() / ("euxis_claude_cli_" + std::to_string(::getpid()));
        fs::remove_all(root_);
        fs::create_directories(root_ / "bin");
        fs::create_directories(root_ / "home");
        fs::create_directories(root_ / "data");
        record_ = root_ / "record.txt";

        // The stub reports whether a key reached it, then its argv.
        const auto stub = root_ / "bin" / "claude";
        std::ofstream(stub) << "#!/bin/sh\n"
                            << "{ echo \"key=${ANTHROPIC_API_KEY:-unset}\";"
                            << " echo \"token=${ANTHROPIC_AUTH_TOKEN:-unset}\";"
                            << " for a in \"$@\"; do echo \"arg=$a\"; done; } > '" << record_.string() << "'\n"
                            << "echo 'review from the CLI session'\n";
        fs::permissions(stub, fs::perms::owner_all);

        ::setenv("PATH", ((root_ / "bin").string() + ":/usr/bin:/bin").c_str(), 1);
        ::setenv("HOME", (root_ / "home").c_str(), 1);  // no ~/.claude/.credentials.json
        ::unsetenv("ANTHROPIC_API_KEY");
        ::unsetenv("ANTHROPIC_AUTH_TOKEN");
        ::unsetenv("EUXIS_TEST_MOCK_EXECUTION");
    }

    void TearDown() override { fs::remove_all(root_); }

    [[nodiscard]] std::string recorded() const {
        std::ifstream f(record_);
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    [[nodiscard]] static ModelSelection claude() {
        ModelSelection sel;
        sel.provider = "claude";
        sel.model = "claude-sonnet-4-6";
        sel.tier = Tier::Code;
        return sel;
    }

    EnvGuard path_{"PATH"};
    EnvGuard home_{"HOME"};
    EnvGuard key_{"ANTHROPIC_API_KEY"};
    EnvGuard token_{"ANTHROPIC_AUTH_TOKEN"};
    EnvGuard mock_{"EUXIS_TEST_MOCK_EXECUTION"};
    fs::path root_;
    fs::path record_;
};

TEST_F(ClaudeCliSessionTest, UsesTheCliSessionWhenNoCredentialsFileExists) {
    ProviderExecutor executor((root_ / "data").string());

    auto resp = executor.execute(claude(), "review this", 10);

    EXPECT_TRUE(resp.success) << resp.error;
    EXPECT_NE(resp.output.find("review from the CLI session"), std::string::npos);
    EXPECT_NE(recorded().find("arg=-p"), std::string::npos) << recorded();
}

TEST_F(ClaudeCliSessionTest, NeverHandsAnApiKeyToTheCli) {
    ::setenv("ANTHROPIC_API_KEY", "sk-ant-must-not-be-used", 1);
    ::setenv("ANTHROPIC_AUTH_TOKEN", "must-not-be-used-either", 1);
    ProviderExecutor executor((root_ / "data").string());

    auto resp = executor.execute(claude(), "review this", 10);

    EXPECT_TRUE(resp.success) << resp.error;
    const auto rec = recorded();
    EXPECT_NE(rec.find("key=unset"), std::string::npos) << rec;
    EXPECT_NE(rec.find("token=unset"), std::string::npos) << rec;
    EXPECT_EQ(rec.find("sk-ant-must-not-be-used"), std::string::npos) << rec;
}

TEST_F(ClaudeCliSessionTest, PassesThePromptAfterTheOptionTerminator) {
    ProviderExecutor executor((root_ / "data").string());

    auto resp = executor.execute(claude(), "--dangerous-looking prompt", 10);

    EXPECT_TRUE(resp.success) << resp.error;
    const auto rec = recorded();
    const auto sep = rec.find("arg=--\n");
    const auto prompt = rec.find("arg=--dangerous-looking prompt");
    ASSERT_NE(sep, std::string::npos) << rec;
    ASSERT_NE(prompt, std::string::npos) << rec;
    EXPECT_LT(sep, prompt);
}

TEST_F(ClaudeCliSessionTest, TheAnthropicAliasUsesTheSameCliSession) {
    ProviderExecutor executor((root_ / "data").string());
    auto sel = claude();
    sel.provider = "anthropic";

    auto resp = executor.execute(sel, "review this", 10);

    EXPECT_TRUE(resp.success) << resp.error;
    EXPECT_NE(recorded().find("arg=-p"), std::string::npos) << recorded();
}

TEST_F(ClaudeCliSessionTest, AnAuthHandedInByTheCallerIsNotUsed) {
    ProviderExecutor executor((root_ / "data").string());
    ResolvedAuth auth;
    auth.token = "sk-ant-caller-supplied";
    auth.provider = "anthropic";
    auth.is_oauth = false;

    auto resp = executor.execute(claude(), "review this", 10, auth);

    EXPECT_TRUE(resp.success) << resp.error;
    const auto rec = recorded();
    EXPECT_NE(rec.find("key=unset"), std::string::npos) << rec;
    EXPECT_EQ(rec.find("sk-ant-caller-supplied"), std::string::npos) << rec;
}

TEST_F(ClaudeCliSessionTest, WithoutTheCliItSaysHowToLogIn) {
    ::setenv("PATH", "/usr/bin:/bin", 1);
    ProviderExecutor executor((root_ / "data").string());

    auto resp = executor.execute(claude(), "review this", 10);

    EXPECT_FALSE(resp.success);
    EXPECT_NE(resp.error.find("claude"), std::string::npos) << resp.error;
    EXPECT_NE(resp.error.find("login"), std::string::npos) << resp.error;
}

} // namespace
} // namespace euxis::cli
