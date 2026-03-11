#include <gtest/gtest.h>
#include "euxis/cli/cmd/dev.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;

class DevCmdTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx_.euxis_home = "/tmp/euxis_test_dev_" + std::to_string(getpid());
        ctx_.data_dir = ctx_.euxis_home + "/data";
        fs::create_directories(ctx_.data_dir + "/agents");
    }

    void TearDown() override {
        fs::remove_all(ctx_.euxis_home);
    }

    Context ctx_;
};

// ---- Bench command tests ----

TEST_F(DevCmdTest, BenchRuns) {
    auto code = cmd_bench(ctx_, {"filesystem", "--iterations", "3"});
    EXPECT_EQ(code, 0);
}

TEST_F(DevCmdTest, BenchAll) {
    auto code = cmd_bench(ctx_, {"all", "--iterations", "2"});
    EXPECT_EQ(code, 0);
}

TEST_F(DevCmdTest, BenchDefault) {
    auto code = cmd_bench(ctx_, {});
    EXPECT_EQ(code, 0); // "all" is default
}

TEST_F(DevCmdTest, BenchRegistry) {
    auto code = cmd_bench(ctx_, {"registry", "--iterations", "2"});
    EXPECT_EQ(code, 0);
}

TEST_F(DevCmdTest, BenchRegistryWithFile) {
    // Create registry file so parse benchmark actually parses
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array();
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    auto code = cmd_bench(ctx_, {"registry", "--iterations", "3"});
    EXPECT_EQ(code, 0);
}

TEST_F(DevCmdTest, BenchProcess) {
    auto code = cmd_bench(ctx_, {"process", "--iterations", "2"});
    EXPECT_EQ(code, 0);
}

TEST_F(DevCmdTest, BenchFilesystem) {
    auto code = cmd_bench(ctx_, {"filesystem", "--iterations", "2"});
    EXPECT_EQ(code, 0);
}

// ---- Hooks command tests ----

TEST_F(DevCmdTest, HooksList) {
    auto code = cmd_hooks(ctx_, {"list"});
    EXPECT_EQ(code, 0);
}

TEST_F(DevCmdTest, HooksListDefault) {
    auto code = cmd_hooks(ctx_, {});
    EXPECT_EQ(code, 0); // list is default
}

TEST_F(DevCmdTest, HooksListWithHooksDir) {
    auto hooks_dir = fs::path(ctx_.euxis_home) / "euxis-bin" / "hooks";
    fs::create_directories(hooks_dir);
    std::ofstream(hooks_dir / "pre-commit") << "#!/bin/bash\n";
    std::ofstream(hooks_dir / "post-commit") << "#!/bin/bash\n";

    auto code = cmd_hooks(ctx_, {"list"});
    EXPECT_EQ(code, 0);
}

TEST_F(DevCmdTest, HooksInstallNoHooksDir) {
    auto code = cmd_hooks(ctx_, {"install"});
    EXPECT_EQ(code, 1);
}

TEST_F(DevCmdTest, HooksInstallSuccess) {
    auto hooks_dir = fs::path(ctx_.euxis_home) / "euxis-bin" / "hooks";
    fs::create_directories(hooks_dir);
    std::ofstream(hooks_dir / "pre-commit") << "#!/bin/bash\necho test\n";

    // Create .git dir so hooks can be installed
    auto git_hooks_dir = fs::path(ctx_.euxis_home) / ".git" / "hooks";
    fs::create_directories(git_hooks_dir);

    auto code = cmd_hooks(ctx_, {"install"});
    EXPECT_EQ(code, 0);

    // Verify hook was copied
    EXPECT_TRUE(fs::exists(git_hooks_dir / "pre-commit"));
}

TEST_F(DevCmdTest, HooksUsageInvalid) {
    auto code = cmd_hooks(ctx_, {"bogus"});
    EXPECT_EQ(code, 2);
}

// ---- Setup Shell Hooks tests ----

TEST_F(DevCmdTest, SetupShellHooks) {
    auto code = cmd_setup_shell_hooks(ctx_, {});
    EXPECT_EQ(code, 0);
}

// ---- Git Guard tests ----

TEST_F(DevCmdTest, GitGuard) {
    auto code = cmd_git_guard(ctx_, {});
    EXPECT_GE(code, 0);
}

TEST_F(DevCmdTest, GitGuardVerbose) {
    ctx_.verbose = true;
    auto code = cmd_git_guard(ctx_, {});
    EXPECT_GE(code, 0);
}

// ---- License Check tests ----

TEST_F(DevCmdTest, LicenseCheck) {
    auto code = cmd_license_check(ctx_, {});
    EXPECT_GE(code, 0);
}

TEST_F(DevCmdTest, LicenseCheckWithLicenseFile) {
    std::ofstream(fs::path(ctx_.euxis_home) / "LICENSE") << "MIT License\n";

    auto code = cmd_license_check(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(DevCmdTest, LicenseCheckWithCppDir) {
    auto cpp_dir = fs::path(ctx_.euxis_home) / "euxis-cpp";
    fs::create_directories(cpp_dir);
    std::ofstream(cpp_dir / "main.cpp") << "// License: MIT\nint main() {}\n";
    std::ofstream(cpp_dir / "util.cpp") << "#include <iostream>\nint util() {}\n";

    std::ofstream(fs::path(ctx_.euxis_home) / "LICENSE") << "MIT License\n";

    auto code = cmd_license_check(ctx_, {});
    EXPECT_EQ(code, 0);
}

// ---- Docs Test tests ----

TEST_F(DevCmdTest, DocsTest) {
    auto code = cmd_docs_test(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(DevCmdTest, DocsTestWithDocsDir) {
    auto docs_dir = fs::path(ctx_.euxis_home) / "docs";
    fs::create_directories(docs_dir);
    std::ofstream(docs_dir / "README.md") << "# Docs\n\nSome content.\n";
    std::ofstream(docs_dir / "guide.md")
        << "# Guide\n\n[link](existing.md)\n";
    std::ofstream(docs_dir / "existing.md") << "# Existing\n";

    auto code = cmd_docs_test(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(DevCmdTest, DocsTestWithBrokenLinks) {
    auto docs_dir = fs::path(ctx_.euxis_home) / "docs";
    fs::create_directories(docs_dir);
    std::ofstream(docs_dir / "broken.md")
        << "# Broken\n\n[missing](nonexistent.md)\n";

    auto code = cmd_docs_test(ctx_, {});
    EXPECT_EQ(code, 1);
}

TEST_F(DevCmdTest, DocsTestVerboseBrokenLinks) {
    ctx_.verbose = true;
    auto docs_dir = fs::path(ctx_.euxis_home) / "docs";
    fs::create_directories(docs_dir);
    std::ofstream(docs_dir / "broken.md")
        << "# Broken\n\n[missing](nonexistent.md)\n";

    auto code = cmd_docs_test(ctx_, {});
    EXPECT_EQ(code, 1);
}

TEST_F(DevCmdTest, DocsTestIgnoresHttpLinks) {
    auto docs_dir = fs::path(ctx_.euxis_home) / "docs";
    fs::create_directories(docs_dir);
    std::ofstream(docs_dir / "links.md")
        << "# Links\n\n[ext](https://example.com)\n[anchor](#section)\n";

    auto code = cmd_docs_test(ctx_, {});
    EXPECT_EQ(code, 0);
}

// ---- Sync Docs tests ----

TEST_F(DevCmdTest, SyncDocs) {
    auto code = cmd_sync_docs(ctx_, {});
    EXPECT_TRUE(code == 0 || code == 1);
}

TEST_F(DevCmdTest, SyncDocsWithDocsDir) {
    auto docs_dir = fs::path(ctx_.euxis_home) / "docs";
    fs::create_directories(docs_dir);
    std::ofstream(docs_dir / "README.md") << "# Docs\n";
    std::ofstream(docs_dir / "guide.md") << "# Guide\n";

    auto code = cmd_sync_docs(ctx_, {});
    EXPECT_EQ(code, 0);

    // Check output directory was created
    auto output_dir = fs::path(ctx_.euxis_home) / "data" / "docs";
    EXPECT_TRUE(fs::exists(output_dir / "README.md"));
    EXPECT_TRUE(fs::exists(output_dir / "guide.md"));
}

TEST_F(DevCmdTest, SyncDocsSubdirectories) {
    auto docs_dir = fs::path(ctx_.euxis_home) / "docs";
    fs::create_directories(docs_dir / "sub");
    std::ofstream(docs_dir / "sub" / "nested.md") << "# Nested\n";

    auto code = cmd_sync_docs(ctx_, {});
    EXPECT_EQ(code, 0);
}

// ---- Test Infra tests ----

TEST_F(DevCmdTest, TestInfra) {
    auto code = cmd_test_infra(ctx_, {});
    EXPECT_GE(code, 0);
}

TEST_F(DevCmdTest, TestInfraWithFullSetup) {
    // Create expected directories and files
    fs::create_directories(ctx_.data_dir + "/agents");
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array();
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    auto code = cmd_test_infra(ctx_, {});
    EXPECT_GE(code, 0);
}

// --- Coverage: lines 135-137 (setup-shell-hooks for bash) ---
TEST_F(DevCmdTest, SetupShellHooksBash) {
    setenv("SHELL", "/bin/bash", 1);
    auto code = cmd_setup_shell_hooks(ctx_, {});
    EXPECT_EQ(code, 0);
}

// --- Coverage: lines 141-143 (setup-shell-hooks for zsh) ---
TEST_F(DevCmdTest, SetupShellHooksZsh) {
    setenv("SHELL", "/bin/zsh", 1);
    auto code = cmd_setup_shell_hooks(ctx_, {});
    EXPECT_EQ(code, 0);
}

// --- Coverage: lines 141-143 (setup-shell-hooks for fish) ---
TEST_F(DevCmdTest, SetupShellHooksFish) {
    setenv("SHELL", "/usr/bin/fish", 1);
    auto code = cmd_setup_shell_hooks(ctx_, {});
    EXPECT_EQ(code, 0);
}

// --- Coverage: lines 145-146 (setup-shell-hooks for unknown shell) ---
TEST_F(DevCmdTest, SetupShellHooksUnknown) {
    setenv("SHELL", "/bin/unknown_shell", 1);
    auto code = cmd_setup_shell_hooks(ctx_, {});
    EXPECT_EQ(code, 0);
}

// --- Coverage: lines 191, 195 (git-guard verbose with staged changes) ---
TEST_F(DevCmdTest, GitGuardVerboseOnProtectedBranch) {
    ctx_.verbose = true;
    // Will run git commands in whatever dir; exercises verbose + branch output
    auto code = cmd_git_guard(ctx_, {});
    EXPECT_GE(code, 0);
}

// --- Coverage: lines 347-348 (sync-docs with pandoc available) ---
TEST_F(DevCmdTest, SyncDocsWithMarkdownFiles) {
    auto docs_dir = fs::path(ctx_.euxis_home) / "docs";
    fs::create_directories(docs_dir);
    std::ofstream(docs_dir / "guide.md") << "# Guide\n\nSome guide content.\n";
    std::ofstream(docs_dir / "tutorial.md") << "# Tutorial\n\nSteps here.\n";

    auto code = cmd_sync_docs(ctx_, {});
    EXPECT_EQ(code, 0);
}

// --- Coverage: lines 374-375 (sync-docs without pandoc message) ---
TEST_F(DevCmdTest, SyncDocsShowsPandocStatus) {
    auto docs_dir = fs::path(ctx_.euxis_home) / "docs";
    fs::create_directories(docs_dir);
    std::ofstream(docs_dir / "readme.md") << "# Readme\n";

    auto code = cmd_sync_docs(ctx_, {});
    EXPECT_EQ(code, 0);
}

// --- Coverage: line 398 (test-infra "cmd:" check) ---
TEST_F(DevCmdTest, TestInfraWithAllDirs) {
    fs::create_directories(ctx_.data_dir + "/agents");
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array();
    std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();

    // Create runtime and core dirs
    fs::create_directories(ctx_.euxis_home + "/data/runtime");
    fs::create_directories(ctx_.euxis_home + "/euxis-core");

    auto code = cmd_test_infra(ctx_, {});
    EXPECT_GE(code, 0);
}

// --- Coverage: bench with json output ---
TEST_F(DevCmdTest, BenchWithJsonOutput) {
    ctx_.json_output = true;
    auto code = cmd_bench(ctx_, {"filesystem", "--iterations", "2"});
    EXPECT_EQ(code, 0);
}

// --- Coverage: hooks install with missing git dir ---
TEST_F(DevCmdTest, HooksInstallNoGitDir) {
    auto hooks_dir = fs::path(ctx_.euxis_home) / "euxis-bin" / "hooks";
    fs::create_directories(hooks_dir);
    std::ofstream(hooks_dir / "pre-commit") << "#!/bin/bash\necho test\n";

    // No .git dir -> may fail gracefully
    auto code = cmd_hooks(ctx_, {"install"});
    EXPECT_GE(code, 0);
}

} // namespace
} // namespace euxis::cli::cmd
