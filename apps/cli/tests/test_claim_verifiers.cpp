/// @file
/// @brief euxis must not run a scanned repository's build or test
///        commands unless the user opts in with --run-verifiers.

#include "euxis/cli/claim_verifiers.hpp"
#include "euxis/cli/cmd/fleet.hpp"
#include "euxis/cli/command.hpp"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;

class ClaimVerifierTest : public ::testing::Test {
   protected:
    void SetUp() override {
        setenv("EUXIS_TEST_MOCK_EXECUTION", "1", 1);
        ctx_.euxis_home = "/tmp/euxis_test_claims_" + std::to_string(getpid());
        ctx_.data_dir = ctx_.euxis_home + "/data";
        fs::create_directories(ctx_.data_dir + "/agents");
        nlohmann::json reg;
        reg["agents"] = nlohmann::json::array({
            {{"agent_id", "alpha"},
             {"role", "leader"},
             {"version", "1.0"},
             {"tier", "code"},
             {"tags", {"core"}},
             {"capabilities", {"routing"}},
             {"prompt_path", ""}},
        });
        std::ofstream(ctx_.data_dir + "/agents/registry.json") << reg.dump();
    }

    void TearDown() override {
        unsetenv("EUXIS_TEST_MOCK_EXECUTION");
        fs::remove_all(ctx_.euxis_home);
    }

    struct CanaryRun {
        bool executed;
        std::string err;
    };

    // Runs a one-step playbook from inside a repo whose `make check`
    // creates `ran.txt`, and reports whether that command was executed.
    CanaryRun run_in_canary_repo(const std::string& name, const std::vector<std::string>& extra) {
        auto repo = fs::path(ctx_.euxis_home) / name;
        fs::create_directories(repo);
        std::ofstream(repo / "Makefile") << "check:\n\t@touch ran.txt\n";

        auto path = (fs::path(ctx_.euxis_home) / (name + ".json")).string();
        nlohmann::json manifest;
        manifest["steps"] = nlohmann::json::array({
            {{"name", "check"}, {"agent", "alpha"}, {"task", "verify code"}},
        });
        std::ofstream(path) << manifest.dump();

        std::vector<std::string> args = {path, "--ci", "--mode", "standard"};
        args.insert(args.end(), extra.begin(), extra.end());

        auto saved_cwd = fs::current_path();
        fs::current_path(repo);
        std::stringstream out;
        std::stringstream err;
        auto* old_cout = std::cout.rdbuf(out.rdbuf());
        auto* old_cerr = std::cerr.rdbuf(err.rdbuf());
        (void)cmd_playbook(ctx_, args);
        std::cerr.rdbuf(old_cerr);
        std::cout.rdbuf(old_cout);
        fs::current_path(saved_cwd);
        return {fs::exists(repo / "ran.txt"), err.str()};
    }

    Context ctx_;
};

TEST_F(ClaimVerifierTest, RepoVerifiersDoNotRunByDefault) {
    // Scanning a repository must not execute its build or test
    // commands: on an untrusted pull request that is arbitrary code
    // execution on the CI runner.
    auto run = run_in_canary_repo("canary-default", {});
    EXPECT_FALSE(run.executed) << "the scanned repo's `make check` ran without opt-in";
    EXPECT_NE(run.err.find("--run-verifiers"), std::string::npos) << run.err;
}

TEST_F(ClaimVerifierTest, RepoVerifiersRunWithOptIn) {
    auto run = run_in_canary_repo("canary-optin", {"--run-verifiers"});
    EXPECT_TRUE(run.executed) << run.err;
}

TEST(ClaimVerifierPillar, MapsVerifierNamesToPillars) {
    EXPECT_EQ(claims::verifier_pillar("make-check"), "build");
    EXPECT_EQ(claims::verifier_pillar("cargo-test"), "testing");
    EXPECT_EQ(claims::verifier_pillar("lint"), "");
}

}  // namespace
}  // namespace euxis::cli::cmd
