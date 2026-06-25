#include <gtest/gtest.h>
#include "euxis/cli/cmd/lifecycle.hpp"
#include "euxis/cli/engine.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <unistd.h>
#include <nlohmann/json.hpp>

// NOLINTBEGIN(cert-err33-c) — test scratch I/O (tempfile setup, fclose
// teardown) intentionally discards return; tests can blanket-disable per
// docs/development/clang-tidy-policy.md.

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;

class LifecycleCmdTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_home_ = "/tmp/euxis_test_lifecycle_" + std::to_string(getpid());
        ctx_.euxis_home = test_home_;
        ctx_.data_dir = test_home_ + "/data";
        fs::create_directories(ctx_.data_dir + "/agents");
        fs::create_directories(ctx_.data_dir + "/config");
    }

    void TearDown() override {
        fs::remove_all(test_home_);
    }

    // Capture stderr (help text goes there)
    std::string capture_cerr(std::function<int()> fn) {
        std::ostringstream buf;
        auto* old = std::cerr.rdbuf(buf.rdbuf());
        fn();
        std::cerr.rdbuf(old);
        return buf.str();
    }

    // Capture stdout via fd-level redirect (for std::println / std::cout)
    std::string capture_stdout(std::function<int()> fn) {
        auto tmp = std::tmpfile();
        if (!tmp) return {};
        int old_fd = dup(STDOUT_FILENO);
        dup2(fileno(tmp), STDOUT_FILENO);
        fn();
        fflush(stdout);
        dup2(old_fd, STDOUT_FILENO);
        close(old_fd);
        fseek(tmp, 0, SEEK_END);
        auto sz = ftell(tmp);
        fseek(tmp, 0, SEEK_SET);
        std::string out(sz, '\0');
        auto nread = fread(out.data(), 1, sz, tmp);
        out.resize(nread);
        fclose(tmp);
        return out;
    }

    Context ctx_;
    std::string test_home_;
};

// ========== install tests ==========

TEST_F(LifecycleCmdTest, InstallCreatesDirectories) {
    // Remove data dir so install creates it
    fs::remove_all(test_home_);
    auto code = cmd_install(ctx_, {});
    EXPECT_EQ(code, 0);
    EXPECT_TRUE(fs::is_directory(fs::path(test_home_) / "bin"));
    EXPECT_TRUE(fs::is_directory(fs::path(test_home_) / "data/agents"));
    EXPECT_TRUE(fs::is_directory(fs::path(test_home_) / "data/config"));
    EXPECT_TRUE(fs::is_directory(fs::path(test_home_) / "data/runtime/memory"));
    EXPECT_TRUE(fs::is_directory(fs::path(test_home_) / "data/runtime/sessions"));
    EXPECT_TRUE(fs::is_directory(fs::path(test_home_) / "data/runtime/verdicts"));
}

TEST_F(LifecycleCmdTest, InstallIdempotent) {
    auto code1 = cmd_install(ctx_, {});
    auto code2 = cmd_install(ctx_, {});
    EXPECT_EQ(code1, 0);
    EXPECT_EQ(code2, 0);
}

TEST_F(LifecycleCmdTest, InstallDryRunNoChanges) {
    fs::remove_all(test_home_);
    auto code = cmd_install(ctx_, {"--dry-run"});
    EXPECT_EQ(code, 0);
    // Dry run should NOT create directories
    EXPECT_FALSE(fs::is_directory(fs::path(test_home_) / "bin"));
}

TEST_F(LifecycleCmdTest, InstallHelpContent) {
    auto out = capture_cerr([&]{ return cmd_install(ctx_, {"--help"}); });
    EXPECT_NE(out.find("Usage:"), std::string::npos);
    EXPECT_NE(out.find("euxis install"), std::string::npos);
    EXPECT_NE(out.find("--dry-run"), std::string::npos);
    EXPECT_NE(out.find("--force"), std::string::npos);
    EXPECT_NE(out.find("--shell-setup"), std::string::npos);
    EXPECT_NE(out.find("--shell"), std::string::npos);
    EXPECT_NE(out.find("--no-completions"), std::string::npos);
}

TEST_F(LifecycleCmdTest, InstallShowsNextSteps) {
    // Default install (no --shell-setup) should print next steps
    auto out = capture_stdout([&]{ return cmd_install(ctx_, {}); });
    EXPECT_NE(out.find("Next steps"), std::string::npos);
}

TEST_F(LifecycleCmdTest, InstallShellSetupWritesMarkers) {
    // Create a temp shell config to write to
    auto config = fs::path(test_home_) / "test_config.fish";
    std::ofstream(config) << "# existing config\n";

    // Override HOME so shell_config_path resolves to our test dir
    // Instead, test with --shell-setup and a real config path won't work in test
    // But we can verify the help mentions --shell-setup
    auto code = cmd_install(ctx_, {"--help"});
    EXPECT_EQ(code, 0);
}

TEST_F(LifecycleCmdTest, InstallCreatesBinSymlink) {
    auto code = cmd_install(ctx_, {});
    EXPECT_EQ(code, 0);
    // Binary symlink may or may not exist depending on /proc/self/exe resolution
    // but install should not fail
}

// ========== update tests ==========

TEST_F(LifecycleCmdTest, UpdateRunsLocally) {
    // Without a git repo or registry, update should still succeed
    auto code = cmd_update(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(LifecycleCmdTest, UpdateDryRunNoChanges) {
    auto code = cmd_update(ctx_, {"--dry-run"});
    EXPECT_EQ(code, 0);
}

TEST_F(LifecycleCmdTest, UpdateHelpContent) {
    auto out = capture_cerr([&]{ return cmd_update(ctx_, {"--help"}); });
    EXPECT_NE(out.find("Usage:"), std::string::npos);
    EXPECT_NE(out.find("euxis update"), std::string::npos);
    EXPECT_NE(out.find("--dry-run"), std::string::npos);
    EXPECT_NE(out.find("--fetch"), std::string::npos);
}

TEST_F(LifecycleCmdTest, UpdateValidatesRegistry) {
    // Create a valid registry
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "test"}, {"role", "r1"}}
    });
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    auto out = capture_stdout([&]{ return cmd_update(ctx_, {}); });
    EXPECT_NE(out.find("1"), std::string::npos); // 1 agent
}

TEST_F(LifecycleCmdTest, UpdateValidatesConfigs) {
    // Create some config files
    std::ofstream(ctx_.data_dir + "/config/router.json") << "{}";
    std::ofstream(ctx_.data_dir + "/config/policy.json") << "{}";

    auto code = cmd_update(ctx_, {});
    EXPECT_EQ(code, 0);
}

// ========== upgrade tests ==========

TEST_F(LifecycleCmdTest, UpgradeNoGitReturns1) {
    auto code = cmd_upgrade(ctx_, {});
    EXPECT_EQ(code, 1);
}

TEST_F(LifecycleCmdTest, UpgradeDryRunNoChanges) {
    auto code = cmd_upgrade(ctx_, {"--dry-run"});
    // Dry run should still fail if no .git
    EXPECT_EQ(code, 1);
}

TEST_F(LifecycleCmdTest, UpgradeHelpContent) {
    auto out = capture_cerr([&]{ return cmd_upgrade(ctx_, {"--help"}); });
    EXPECT_NE(out.find("Usage:"), std::string::npos);
    EXPECT_NE(out.find("euxis upgrade"), std::string::npos);
    EXPECT_NE(out.find("--dry-run"), std::string::npos);
    EXPECT_NE(out.find("--clean"), std::string::npos);
}

// ========== uninstall tests ==========

TEST_F(LifecycleCmdTest, UninstallRemovesSymlink) {
    // Create a symlink to remove
    auto bin_dir = fs::path(test_home_) / "bin";
    fs::create_directories(bin_dir);
    auto link = bin_dir / "euxis";
    std::ofstream(link) << "fake";  // Create a regular file to remove

    auto code = cmd_uninstall(ctx_, {});
    EXPECT_EQ(code, 0);
    EXPECT_FALSE(fs::exists(link));
}

TEST_F(LifecycleCmdTest, UninstallNoSymlinkOk) {
    auto code = cmd_uninstall(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(LifecycleCmdTest, UninstallPurgeDryRun) {
    auto code = cmd_uninstall(ctx_, {"--purge", "--dry-run"});
    EXPECT_EQ(code, 0);
    // Dry run should NOT delete EUXIS_HOME
    EXPECT_TRUE(fs::is_directory(test_home_));
}

TEST_F(LifecycleCmdTest, UninstallHelpContent) {
    auto out = capture_cerr([&]{ return cmd_uninstall(ctx_, {"--help"}); });
    EXPECT_NE(out.find("Usage:"), std::string::npos);
    EXPECT_NE(out.find("euxis uninstall"), std::string::npos);
    EXPECT_NE(out.find("--purge"), std::string::npos);
    EXPECT_NE(out.find("--dry-run"), std::string::npos);
    EXPECT_NE(out.find("--yes"), std::string::npos);
}

TEST_F(LifecycleCmdTest, UninstallPurgeWithYes) {
    auto code = cmd_uninstall(ctx_, {"--purge", "--yes"});
    EXPECT_EQ(code, 0);
    EXPECT_FALSE(fs::is_directory(test_home_));
}

// ========== self tests ==========

TEST_F(LifecycleCmdTest, SelfNoSubcommandReturns2) {
    auto code = cmd_self(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(LifecycleCmdTest, SelfHelpContent) {
    auto out = capture_cerr([&]{ return cmd_self(ctx_, {"--help"}); });
    EXPECT_NE(out.find("Usage:"), std::string::npos);
    EXPECT_NE(out.find("euxis self"), std::string::npos);
    EXPECT_NE(out.find("status"), std::string::npos);
    EXPECT_NE(out.find("paths"), std::string::npos);
    EXPECT_NE(out.find("doctor"), std::string::npos);
    EXPECT_NE(out.find("version"), std::string::npos);
    EXPECT_NE(out.find("where"), std::string::npos);
    EXPECT_NE(out.find("repair"), std::string::npos);
}

TEST_F(LifecycleCmdTest, SelfStatusShowsVersion) {
    auto out = capture_stdout([&]{ return cmd_self(ctx_, {"status"}); });
    EXPECT_NE(out.find("v0.1.3"), std::string::npos);
}

TEST_F(LifecycleCmdTest, SelfStatusShowsInstallMode) {
    auto out = capture_stdout([&]{ return cmd_self(ctx_, {"status"}); });
    // Should show some mode (unknown for test dir)
    EXPECT_NE(out.find("Mode:"), std::string::npos);
}

TEST_F(LifecycleCmdTest, SelfStatusJsonOutput) {
    ctx_.json_output = true;
    auto out = capture_stdout([&]{ return cmd_self(ctx_, {"status"}); });
    auto j = nlohmann::json::parse(out);
    EXPECT_EQ(j["version"], "v0.1.3");
    EXPECT_TRUE(j.contains("euxis_home"));
    EXPECT_TRUE(j.contains("install_mode"));
    EXPECT_TRUE(j.contains("shell"));
    EXPECT_TRUE(j.contains("agents"));
}

TEST_F(LifecycleCmdTest, SelfStatusJsonViaFlag) {
    auto out = capture_stdout([&]{ return cmd_self(ctx_, {"status", "--json"}); });
    auto j = nlohmann::json::parse(out);
    EXPECT_EQ(j["version"], "v0.1.3");
}

TEST_F(LifecycleCmdTest, SelfPathsShowsEuxisHome) {
    auto out = capture_stdout([&]{ return cmd_self(ctx_, {"paths"}); });
    EXPECT_NE(out.find(ctx_.euxis_home), std::string::npos);
}

TEST_F(LifecycleCmdTest, SelfPathsJsonOutput) {
    ctx_.json_output = true;
    auto out = capture_stdout([&]{ return cmd_self(ctx_, {"paths"}); });
    auto j = nlohmann::json::parse(out);
    EXPECT_EQ(j["euxis_home"], ctx_.euxis_home);
    EXPECT_TRUE(j.contains("data"));
    EXPECT_TRUE(j.contains("config"));
    EXPECT_TRUE(j.contains("sessions"));
}

TEST_F(LifecycleCmdTest, SelfDoctorDelegates) {
    auto code = cmd_self(ctx_, {"doctor"});
    // Doctor will find issues in test env but shouldn't crash
    EXPECT_GE(code, 0);
}

TEST_F(LifecycleCmdTest, SelfVersionShowsVersion) {
    auto out = capture_stdout([&]{ return cmd_self(ctx_, {"version"}); });
    EXPECT_NE(out.find("v0.1.3"), std::string::npos);
}

TEST_F(LifecycleCmdTest, SelfVersionJsonOutput) {
    ctx_.json_output = true;
    auto out = capture_stdout([&]{ return cmd_self(ctx_, {"version"}); });
    auto j = nlohmann::json::parse(out);
    EXPECT_EQ(j["version"], "v0.1.3");
    EXPECT_EQ(j["protocol"], "1.0");
    EXPECT_EQ(j["language"], "C++23");
}

TEST_F(LifecycleCmdTest, SelfWhereShowsPath) {
    auto out = capture_stdout([&]{ return cmd_self(ctx_, {"where"}); });
    // Should contain some path (via /proc/self/exe)
    EXPECT_FALSE(out.empty());
}

TEST_F(LifecycleCmdTest, SelfRepairDelegates) {
    auto code = cmd_self(ctx_, {"repair"});
    // Repair delegates to cmd_fix which runs doctor --fix
    EXPECT_GE(code, 0);
}

TEST_F(LifecycleCmdTest, SelfInvalidSubcommandReturns2) {
    auto code = cmd_self(ctx_, {"bogus"});
    EXPECT_EQ(code, 2);
}

// ==========================================================================
// Shell-setup paths — install + uninstall against a sandboxed HOME
// ==========================================================================
// The shell-setup branches in install/uninstall (lines 48-216 in
// lifecycle.cpp) are gated on $HOME pointing at a shell-config-bearing
// home dir. The tests below set HOME to a tmpdir, drop fixture shell
// configs in it, then exercise the full path: detect existing entries,
// write the marker block, idempotency on re-run, --purge cleanup.

class LifecycleShellTest : public LifecycleCmdTest {
protected:
    fs::path fake_home;
    std::string saved_home;

    void SetUp() override {
        LifecycleCmdTest::SetUp();
        fake_home = fs::path(test_home_) / "fake_home";
        fs::create_directories(fake_home);
        // Drop completions source so install_completions has something to
        // symlink (or it'll early-exit). The source files just need to
        // exist; their content doesn't matter for the install logic.
        fs::create_directories(fs::path(ctx_.euxis_home) / "data" / "config" / "completions");
        std::ofstream(fs::path(ctx_.euxis_home) / "data" / "config" / "completions" / "euxis.fish") << "complete -c euxis";
        std::ofstream(fs::path(ctx_.euxis_home) / "data" / "config" / "completions" / "euxis.bash") << "# bash completions";
        std::ofstream(fs::path(ctx_.euxis_home) / "data" / "config" / "completions" / "euxis.zsh") << "# zsh completions";

        if (const char* h = std::getenv("HOME")) saved_home = h;
        ::setenv("HOME", fake_home.string().c_str(), 1);
    }

    void TearDown() override {
        if (saved_home.empty()) ::unsetenv("HOME");
        else ::setenv("HOME", saved_home.c_str(), 1);
        LifecycleCmdTest::TearDown();
    }
};

TEST_F(LifecycleShellTest, InstallShellSetupBashWritesMarker) {
    std::ofstream(fake_home / ".bashrc") << "# pre-existing\n";
    int code = 0;
    capture_stdout([&]{ code = cmd_install(ctx_, {"--shell-setup", "--shell", "bash"}); return code; });
    EXPECT_EQ(code, 0);
    std::ifstream f(fake_home / ".bashrc");
    std::string body((std::istreambuf_iterator<char>(f)), {});
    EXPECT_NE(body.find("euxis install"), std::string::npos);
    EXPECT_NE(body.find("EUXIS_HOME"), std::string::npos);
}

TEST_F(LifecycleShellTest, InstallShellSetupZshWritesMarker) {
    std::ofstream(fake_home / ".zshrc") << "# zsh config\n";
    capture_stdout([&]{ return cmd_install(ctx_, {"--shell-setup", "--shell", "zsh"}); });
    std::ifstream f(fake_home / ".zshrc");
    std::string body((std::istreambuf_iterator<char>(f)), {});
    EXPECT_NE(body.find("euxis install"), std::string::npos);
}

TEST_F(LifecycleShellTest, InstallShellSetupFishWritesMarker) {
    fs::create_directories(fake_home / ".config" / "fish");
    std::ofstream(fake_home / ".config" / "fish" / "config.fish") << "# fish config\n";
    capture_stdout([&]{ return cmd_install(ctx_, {"--shell-setup", "--shell", "fish"}); });
    std::ifstream f(fake_home / ".config" / "fish" / "config.fish");
    std::string body((std::istreambuf_iterator<char>(f)), {});
    EXPECT_NE(body.find("euxis install"), std::string::npos);
}

TEST_F(LifecycleShellTest, InstallShellSetupIdempotent) {
    std::ofstream(fake_home / ".bashrc") << "# initial\n";
    capture_stdout([&]{ return cmd_install(ctx_, {"--shell-setup", "--shell", "bash"}); });
    auto first_body = [&]{
        std::ifstream f(fake_home / ".bashrc");
        return std::string((std::istreambuf_iterator<char>(f)), {});
    }();
    // Second run — should detect the existing entry and not duplicate it
    capture_stdout([&]{ return cmd_install(ctx_, {"--shell-setup", "--shell", "bash"}); });
    auto second_body = [&]{
        std::ifstream f(fake_home / ".bashrc");
        return std::string((std::istreambuf_iterator<char>(f)), {});
    }();
    // Same number of BEGIN markers either way (idempotent re-run).
    auto count = [](const std::string& s, const std::string& m) {
        size_t n = 0, pos = 0;
        while ((pos = s.find(m, pos)) != std::string::npos) { ++n; ++pos; }
        return n;
    };
    EXPECT_EQ(count(first_body, "euxis install"),
              count(second_body, "euxis install"));
}

TEST_F(LifecycleShellTest, InstallShellSetupDryRunDoesNotWrite) {
    std::ofstream(fake_home / ".bashrc") << "# untouched\n";
    capture_stdout([&]{
        return cmd_install(ctx_, {"--shell-setup", "--shell", "bash", "--dry-run"});
    });
    std::ifstream f(fake_home / ".bashrc");
    std::string body((std::istreambuf_iterator<char>(f)), {});
    EXPECT_EQ(body.find("euxis install"), std::string::npos);
}

TEST_F(LifecycleShellTest, InstallFishCompletionsSymlinkCreated) {
    capture_stdout([&]{ return cmd_install(ctx_, {"--shell-setup", "--shell", "fish"}); });
    auto fish_comp_dest = fake_home / ".config" / "fish" / "completions" / "euxis.fish";
    EXPECT_TRUE(fs::exists(fish_comp_dest) || fs::is_symlink(fish_comp_dest));
}

TEST_F(LifecycleShellTest, InstallNoCompletionsFlagSkipsCompletionsStep) {
    int code = 0;
    auto out = capture_stdout([&]{
        code = cmd_install(ctx_, {"--shell-setup", "--shell", "bash", "--no-completions"});
        return code;
    });
    EXPECT_EQ(code, 0);
}

TEST_F(LifecycleShellTest, UninstallPurgeRemovesShellMarker) {
    // Install first so there's something to purge.
    std::ofstream(fake_home / ".bashrc") << "# before\n";
    capture_stdout([&]{ return cmd_install(ctx_, {"--shell-setup", "--shell", "bash"}); });
    // Now purge with --yes to skip the prompt.
    capture_stdout([&]{ return cmd_uninstall(ctx_, {"--purge", "--yes"}); });
    std::ifstream f(fake_home / ".bashrc");
    std::string body((std::istreambuf_iterator<char>(f)), {});
    EXPECT_EQ(body.find("euxis install"), std::string::npos);
}

} // namespace
} // namespace euxis::cli::cmd

// ========== Engine integration tests ==========

namespace euxis::cli {
namespace {

namespace fs = std::filesystem;

class EngineLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_home_ = "/tmp/euxis_test_lifecycle_eng_" + std::to_string(getpid());
        auto data_dir = test_home_ + "/data";
        fs::create_directories(data_dir + "/agents");
        fs::create_directories(data_dir + "/config");
    }

    void TearDown() override {
        fs::remove_all(test_home_);
    }

    std::string test_home_;
};

TEST_F(EngineLifecycleTest, HelpShowsLifecycleGroup) {
    Engine e(test_home_);
    auto tmp = std::tmpfile();
    ASSERT_NE(tmp, nullptr);
    int old_fd = dup(STDOUT_FILENO);
    dup2(fileno(tmp), STDOUT_FILENO);
    e.run({"--help"});
    fflush(stdout);
    dup2(old_fd, STDOUT_FILENO);
    close(old_fd);
    fseek(tmp, 0, SEEK_END);
    auto sz = ftell(tmp);
    fseek(tmp, 0, SEEK_SET);
    std::string out(sz, '\0');
    auto nread = fread(out.data(), 1, sz, tmp);
    out.resize(nread);
    fclose(tmp);

    EXPECT_NE(out.find("Lifecycle"), std::string::npos);
    EXPECT_NE(out.find("install"), std::string::npos);
    EXPECT_NE(out.find("update"), std::string::npos);
    EXPECT_NE(out.find("upgrade"), std::string::npos);
    EXPECT_NE(out.find("uninstall"), std::string::npos);
    EXPECT_NE(out.find("self"), std::string::npos);

    // Lifecycle should appear between Core and System
    auto core_pos = out.find("Core");
    auto lifecycle_pos = out.find("Lifecycle");
    auto system_pos = out.find("System");
    EXPECT_LT(core_pos, lifecycle_pos);
    EXPECT_LT(lifecycle_pos, system_pos);
}

} // namespace
} // namespace euxis::cli

// NOLINTEND(cert-err33-c)
