// Copyright (c) 2026 The Euxis Authors
// SPDX-License-Identifier: AGPL-3.0-only

// The pre-flight probe must describe how euxis will really authenticate.
// It reported "Native API Key Active" for claude whenever ANTHROPIC_API_KEY
// was set, but the claude provider only ever uses the Claude Code CLI login.

#include "euxis/cli/provider_status.hpp"

#include <gtest/gtest.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

namespace euxis::cli {
namespace {

namespace fs = std::filesystem;

class ProviderStatusTest : public ::testing::Test {
protected:
    void SetUp() override {
        for (const char* name : {"PATH", "HOME", "ANTHROPIC_API_KEY", "OPENAI_API_KEY", "GEMINI_API_KEY"}) {
            const char* v = std::getenv(name);
            saved_.emplace_back(name, v ? std::optional<std::string>(v) : std::nullopt);
        }
        root_ = fs::temp_directory_path() / ("euxis_provider_status_" + std::to_string(::getpid()));
        fs::remove_all(root_);
        fs::create_directories(root_ / "bin");
        fs::create_directories(root_ / "home");
        ::setenv("HOME", (root_ / "home").c_str(), 1);
        ::setenv("PATH", "/usr/bin:/bin", 1);
        for (const char* k : {"ANTHROPIC_API_KEY", "OPENAI_API_KEY", "GEMINI_API_KEY"}) ::unsetenv(k);
    }

    void TearDown() override {
        for (const auto& [name, value] : saved_) {
            if (value) ::setenv(name.c_str(), value->c_str(), 1);
            else ::unsetenv(name.c_str());
        }
        fs::remove_all(root_);
    }

    void put_on_path(const std::string& binary) {
        const auto stub = root_ / "bin" / binary;
        std::ofstream(stub) << "#!/bin/sh\nexit 0\n";
        fs::permissions(stub, fs::perms::owner_all);
        ::setenv("PATH", ((root_ / "bin").string() + ":/usr/bin:/bin").c_str(), 1);
    }

    std::vector<std::pair<std::string, std::optional<std::string>>> saved_;
    fs::path root_;
};

TEST_F(ProviderStatusTest, ClaudeIsTheCliSessionWhenTheCliIsInstalled) {
    put_on_path("claude");
    AuthProfileStore store((root_ / "data").string());
    EXPECT_EQ(provider_auth_kind(store, "claude"), ProviderAuthKind::CliSession);
}

TEST_F(ProviderStatusTest, AnAnthropicKeyNeverCountsForClaude) {
    ::setenv("ANTHROPIC_API_KEY", "sk-ant-not-used", 1);
    AuthProfileStore store((root_ / "data").string());
    store.import_env_vars();
    EXPECT_EQ(provider_auth_kind(store, "claude"), ProviderAuthKind::NotConfigured);
}

TEST_F(ProviderStatusTest, OtherProvidersStillReportTheirKeys) {
    ::setenv("OPENAI_API_KEY", "sk-test", 1);
    AuthProfileStore store((root_ / "data").string());
    EXPECT_EQ(provider_auth_kind(store, "openai"), ProviderAuthKind::ApiKey);
    EXPECT_EQ(provider_auth_kind(store, "gemini"), ProviderAuthKind::NotConfigured);
}

TEST_F(ProviderStatusTest, NotConfiguredLineNamesTheLoginCommand) {
    std::ostringstream out;
    print_provider_auth(out, "claude", ProviderAuthKind::NotConfigured, "claude login");
    EXPECT_NE(out.str().find("claude login"), std::string::npos) << out.str();
}

} // namespace
} // namespace euxis::cli
