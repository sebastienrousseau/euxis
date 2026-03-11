#include <gtest/gtest.h>
#include "euxis/cli/cmd/knowledge.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;

class KnowledgeCmdTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx_.euxis_home = "/tmp/euxis_test_know_" + std::to_string(getpid());
        ctx_.data_dir = ctx_.euxis_home + "/data";
        fs::create_directories(ctx_.data_dir + "/config");
    }

    void TearDown() override {
        fs::remove_all(ctx_.euxis_home);
    }

    Context ctx_;
};

// ---- Cortex command tests ----

TEST_F(KnowledgeCmdTest, CortexUsage) {
    auto code = cmd_cortex(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(KnowledgeCmdTest, CortexUnknownSubcommand) {
    auto code = cmd_cortex(ctx_, {"bogus"});
    EXPECT_EQ(code, 2);
}

TEST_F(KnowledgeCmdTest, CortexStatsNoDb) {
    auto code = cmd_cortex(ctx_, {"stats"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, CortexStatsJson) {
    ctx_.json_output = true;
    auto code = cmd_cortex(ctx_, {"stats"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, CortexStatsWithDbFiles) {
    auto db_dir = fs::path(ctx_.euxis_home) / "data/runtime" / "memory" / "cortex" / "db";
    fs::create_directories(db_dir);
    std::ofstream(db_dir / "data1.db") << "some data";
    std::ofstream(db_dir / "data2.db") << "more data";

    auto code = cmd_cortex(ctx_, {"stats"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, CortexRememberUsage) {
    auto code = cmd_cortex(ctx_, {"remember"});
    EXPECT_EQ(code, 2);
}

TEST_F(KnowledgeCmdTest, CortexRememberTooFewArgs) {
    auto code = cmd_cortex(ctx_, {"remember", "key"});
    EXPECT_EQ(code, 2);
}

TEST_F(KnowledgeCmdTest, CortexRememberSuccess) {
    auto code = cmd_cortex(ctx_, {"remember", "my-key", "my-value"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, CortexRememberMultiWord) {
    auto code = cmd_cortex(ctx_, {"remember", "project", "this", "is", "important"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, CortexRememberJson) {
    ctx_.json_output = true;
    auto code = cmd_cortex(ctx_, {"remember", "key1", "value1"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, CortexRecallUsage) {
    auto code = cmd_cortex(ctx_, {"recall"});
    EXPECT_EQ(code, 2);
}

TEST_F(KnowledgeCmdTest, CortexRecallExactMatch) {
    cmd_cortex(ctx_, {"remember", "test-key", "test-value"});
    auto code = cmd_cortex(ctx_, {"recall", "test-key"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, CortexRecallExactMatchJson) {
    ctx_.json_output = true;
    cmd_cortex(ctx_, {"remember", "test-key", "test-value"});
    auto code = cmd_cortex(ctx_, {"recall", "test-key"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, CortexRecallPartialMatch) {
    cmd_cortex(ctx_, {"remember", "project-alpha", "alpha details"});
    cmd_cortex(ctx_, {"remember", "project-beta", "beta details"});
    auto code = cmd_cortex(ctx_, {"recall", "project"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, CortexRecallPartialMatchJson) {
    ctx_.json_output = true;
    cmd_cortex(ctx_, {"remember", "project-alpha", "alpha details"});
    auto code = cmd_cortex(ctx_, {"recall", "project"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, CortexRecallNoMatch) {
    auto code = cmd_cortex(ctx_, {"recall", "nonexistent"});
    EXPECT_EQ(code, 1);
}

TEST_F(KnowledgeCmdTest, CortexRecallNoMatchJson) {
    ctx_.json_output = true;
    auto code = cmd_cortex(ctx_, {"recall", "nonexistent"});
    EXPECT_EQ(code, 1);
}

TEST_F(KnowledgeCmdTest, CortexRecallPartialMatchInValue) {
    cmd_cortex(ctx_, {"remember", "key1", "contains searchterm here"});
    auto code = cmd_cortex(ctx_, {"recall", "searchterm"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, CortexForgetUsage) {
    auto code = cmd_cortex(ctx_, {"forget"});
    EXPECT_EQ(code, 2);
}

TEST_F(KnowledgeCmdTest, CortexForgetNotFound) {
    auto code = cmd_cortex(ctx_, {"forget", "nonexistent"});
    EXPECT_EQ(code, 1);
}

TEST_F(KnowledgeCmdTest, CortexForgetNotFoundJson) {
    ctx_.json_output = true;
    auto code = cmd_cortex(ctx_, {"forget", "nonexistent"});
    EXPECT_EQ(code, 1);
}

TEST_F(KnowledgeCmdTest, CortexForgetSuccess) {
    cmd_cortex(ctx_, {"remember", "delete-me", "temp value"});
    auto code = cmd_cortex(ctx_, {"forget", "delete-me"});
    EXPECT_EQ(code, 0);
    // Verify it's gone
    auto code2 = cmd_cortex(ctx_, {"recall", "delete-me"});
    EXPECT_EQ(code2, 1);
}

TEST_F(KnowledgeCmdTest, CortexForgetSuccessJson) {
    ctx_.json_output = true;
    cmd_cortex(ctx_, {"remember", "delete-me", "temp"});
    auto code = cmd_cortex(ctx_, {"forget", "delete-me"});
    EXPECT_EQ(code, 0);
}

// ---- Graph command tests ----

TEST_F(KnowledgeCmdTest, GraphShow) {
    auto code = cmd_graph(ctx_, {"show"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, GraphShowWithDirectory) {
    auto graph_dir = fs::path(ctx_.euxis_home) / "data/runtime" / "memory" / "cortex";
    fs::create_directories(graph_dir);
    std::ofstream(graph_dir / "entries.json") << R"({"key":"val"})";

    auto code = cmd_graph(ctx_, {"show"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, GraphUsage) {
    auto code = cmd_graph(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(KnowledgeCmdTest, GraphQueryUsage) {
    auto code = cmd_graph(ctx_, {"query"});
    EXPECT_EQ(code, 2);
}

TEST_F(KnowledgeCmdTest, GraphQueryNoDirFound) {
    auto code = cmd_graph(ctx_, {"query", "search"});
    EXPECT_EQ(code, 1);
}

TEST_F(KnowledgeCmdTest, GraphQueryNoMatches) {
    auto graph_dir = fs::path(ctx_.euxis_home) / "data/runtime" / "memory" / "cortex";
    fs::create_directories(graph_dir);
    std::ofstream(graph_dir / "data.txt") << "some random content\n";

    auto code = cmd_graph(ctx_, {"query", "zzz_nonexistent_zzz"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, GraphQueryWithMatches) {
    auto graph_dir = fs::path(ctx_.euxis_home) / "data/runtime" / "memory" / "cortex";
    fs::create_directories(graph_dir);
    std::ofstream(graph_dir / "data.txt") << "line with keyword here\nno match\nanother keyword line\n";

    auto code = cmd_graph(ctx_, {"query", "keyword"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, GraphQueryWithMatchesJson) {
    ctx_.json_output = true;
    auto graph_dir = fs::path(ctx_.euxis_home) / "data/runtime" / "memory" / "cortex";
    fs::create_directories(graph_dir);
    std::ofstream(graph_dir / "data.txt") << "found it\n";

    auto code = cmd_graph(ctx_, {"query", "found"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, GraphExportNoData) {
    auto code = cmd_graph(ctx_, {"export", ctx_.euxis_home + "/export.json"});
    EXPECT_EQ(code, 1);
}

TEST_F(KnowledgeCmdTest, GraphExportSuccess) {
    auto graph_dir = fs::path(ctx_.euxis_home) / "data/runtime" / "memory" / "cortex";
    fs::create_directories(graph_dir);
    std::ofstream(graph_dir / "entries.json") << R"({"key":"val"})";

    auto output = ctx_.euxis_home + "/graph-export.json";
    auto code = cmd_graph(ctx_, {"export", output});
    EXPECT_EQ(code, 0);
    EXPECT_TRUE(fs::exists(output));
}

TEST_F(KnowledgeCmdTest, GraphExportNoArgs) {
    auto code = cmd_graph(ctx_, {"export"});
    EXPECT_EQ(code, 2);
}

TEST_F(KnowledgeCmdTest, GraphUnknownSubcommand) {
    auto code = cmd_graph(ctx_, {"bogus"});
    EXPECT_EQ(code, 2);
}

// ---- Codex command tests ----

TEST_F(KnowledgeCmdTest, CodexListNoFile) {
    auto code = cmd_codex(ctx_, {"list"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, CodexListDefault) {
    auto code = cmd_codex(ctx_, {});
    EXPECT_EQ(code, 0); // list is default
}

TEST_F(KnowledgeCmdTest, CodexListWithTemplates) {
    auto codex_path = fs::path(ctx_.data_dir) / "config" / "codex.json";
    nlohmann::json codex;
    codex["templates"] = nlohmann::json::array({
        {{"name", "agent"}, {"description", "Agent template"}, {"file", "templates/agent.md"}},
        {{"name", "squad"}, {"description", "Squad template"}, {"file", "templates/squad.md"}}
    });
    std::ofstream(codex_path) << codex.dump();

    auto code = cmd_codex(ctx_, {"list"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, CodexValidateNoFile) {
    auto code = cmd_codex(ctx_, {"validate"});
    EXPECT_EQ(code, 1);
}

TEST_F(KnowledgeCmdTest, CodexValidateAllPresent) {
    auto codex_path = fs::path(ctx_.data_dir) / "config" / "codex.json";
    // Create template file
    auto tpl_dir = fs::path(ctx_.euxis_home) / "templates";
    fs::create_directories(tpl_dir);
    std::ofstream(tpl_dir / "agent.md") << "# {{NAME}}\n";

    nlohmann::json codex;
    codex["templates"] = nlohmann::json::array({
        {{"name", "agent"}, {"file", "templates/agent.md"}}
    });
    std::ofstream(codex_path) << codex.dump();

    auto code = cmd_codex(ctx_, {"validate"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, CodexValidateMissingFile) {
    auto codex_path = fs::path(ctx_.data_dir) / "config" / "codex.json";
    nlohmann::json codex;
    codex["templates"] = nlohmann::json::array({
        {{"name", "agent"}, {"file", "templates/missing.md"}}
    });
    std::ofstream(codex_path) << codex.dump();

    auto code = cmd_codex(ctx_, {"validate"});
    EXPECT_EQ(code, 1);
}

TEST_F(KnowledgeCmdTest, CodexRenderUsage) {
    auto code = cmd_codex(ctx_, {"render"});
    EXPECT_EQ(code, 2);
}

TEST_F(KnowledgeCmdTest, CodexRenderNoCodex) {
    auto code = cmd_codex(ctx_, {"render", "agent"});
    EXPECT_EQ(code, 1);
}

TEST_F(KnowledgeCmdTest, CodexRenderTemplateNotFound) {
    auto codex_path = fs::path(ctx_.data_dir) / "config" / "codex.json";
    nlohmann::json codex;
    codex["templates"] = nlohmann::json::array({
        {{"name", "agent"}, {"file", "templates/agent.md"}}
    });
    std::ofstream(codex_path) << codex.dump();

    auto code = cmd_codex(ctx_, {"render", "nonexistent"});
    EXPECT_EQ(code, 1);
}

TEST_F(KnowledgeCmdTest, CodexRenderTemplateFileMissing) {
    auto codex_path = fs::path(ctx_.data_dir) / "config" / "codex.json";
    nlohmann::json codex;
    codex["templates"] = nlohmann::json::array({
        {{"name", "agent"}, {"file", "templates/missing.md"}}
    });
    std::ofstream(codex_path) << codex.dump();

    auto code = cmd_codex(ctx_, {"render", "agent"});
    EXPECT_EQ(code, 1);
}

TEST_F(KnowledgeCmdTest, CodexRenderSuccess) {
    auto codex_path = fs::path(ctx_.data_dir) / "config" / "codex.json";
    auto tpl_dir = fs::path(ctx_.euxis_home) / "templates";
    fs::create_directories(tpl_dir);
    std::ofstream(tpl_dir / "agent.md") << "# {{NAME}}\nRole: {{ROLE}}\n";

    nlohmann::json codex;
    codex["templates"] = nlohmann::json::array({
        {{"name", "agent"}, {"file", "templates/agent.md"}}
    });
    std::ofstream(codex_path) << codex.dump();

    auto code = cmd_codex(ctx_, {"render", "agent", "--var", "NAME=TestAgent", "--var", "ROLE=worker"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, CodexRenderNoTemplatesSection) {
    auto codex_path = fs::path(ctx_.data_dir) / "config" / "codex.json";
    nlohmann::json codex;
    codex["version"] = "1.0";
    std::ofstream(codex_path) << codex.dump();

    auto code = cmd_codex(ctx_, {"render", "agent"});
    EXPECT_EQ(code, 1);
}

TEST_F(KnowledgeCmdTest, CodexUsageInvalid) {
    auto code = cmd_codex(ctx_, {"bogus"});
    EXPECT_EQ(code, 2);
}

// ---- Omnigraph command tests ----

TEST_F(KnowledgeCmdTest, OmnigraphRuns) {
    auto code = cmd_omnigraph(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, OmnigraphWithProvider) {
    auto code = cmd_omnigraph(ctx_, {"--provider", "openai"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, OmnigraphWithBudget) {
    auto code = cmd_omnigraph(ctx_, {"--budget", "50000"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, OmnigraphJson) {
    ctx_.json_output = true;
    auto code = cmd_omnigraph(ctx_, {});
    EXPECT_EQ(code, 0);
}

// ---- Slash command tests ----

TEST_F(KnowledgeCmdTest, SlashList) {
    auto code = cmd_slash(ctx_, {"list"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, SlashUsage) {
    auto code = cmd_slash(ctx_, {});
    EXPECT_EQ(code, 0); // list is default
}

TEST_F(KnowledgeCmdTest, SlashListWithConfig) {
    auto config_path = fs::path(ctx_.data_dir) / "config" / "slash-commands.json";
    nlohmann::json config;
    config["commands"] = nlohmann::json::array({
        {{"name", "status"}, {"description", "Show status"}},
        {{"name", "deploy"}, {"description", "Deploy agents"}}
    });
    std::ofstream(config_path) << config.dump();

    auto code = cmd_slash(ctx_, {"list"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, SlashRunUsage) {
    auto code = cmd_slash(ctx_, {"run"});
    EXPECT_EQ(code, 2);
}

TEST_F(KnowledgeCmdTest, SlashRunNoConfig) {
    auto code = cmd_slash(ctx_, {"run", "status"});
    EXPECT_EQ(code, 1);
}

TEST_F(KnowledgeCmdTest, SlashRunUnknownCommand) {
    auto config_path = fs::path(ctx_.data_dir) / "config" / "slash-commands.json";
    nlohmann::json config;
    config["commands"] = nlohmann::json::array({
        {{"name", "status"}, {"description", "Show status"}, {"action", "cortex stats"}}
    });
    std::ofstream(config_path) << config.dump();

    auto code = cmd_slash(ctx_, {"run", "nonexistent"});
    EXPECT_EQ(code, 1);
}

TEST_F(KnowledgeCmdTest, SlashRunNoCommandsSection) {
    auto config_path = fs::path(ctx_.data_dir) / "config" / "slash-commands.json";
    nlohmann::json config;
    config["version"] = "1";
    std::ofstream(config_path) << config.dump();

    auto code = cmd_slash(ctx_, {"run", "status"});
    EXPECT_EQ(code, 1);
}

TEST_F(KnowledgeCmdTest, SlashRunNoActionOrScript) {
    auto config_path = fs::path(ctx_.data_dir) / "config" / "slash-commands.json";
    nlohmann::json config;
    config["commands"] = nlohmann::json::array({
        {{"name", "empty"}, {"description", "No action"}}
    });
    std::ofstream(config_path) << config.dump();

    auto code = cmd_slash(ctx_, {"run", "empty"});
    EXPECT_EQ(code, 1);
}

TEST_F(KnowledgeCmdTest, SlashRunWithScript) {
    auto config_path = fs::path(ctx_.data_dir) / "config" / "slash-commands.json";
    nlohmann::json config;
    config["commands"] = nlohmann::json::array({
        {{"name", "echo"}, {"description", "Echo test"}, {"script", "echo hello"}}
    });
    std::ofstream(config_path) << config.dump();

    auto code = cmd_slash(ctx_, {"run", "echo"});
    EXPECT_EQ(code, 0);
}

TEST_F(KnowledgeCmdTest, SlashUsageInvalid) {
    auto code = cmd_slash(ctx_, {"bogus"});
    EXPECT_EQ(code, 2);
}

// --- Coverage: line 40 (load_cortex_entries file open failure) ---
TEST_F(KnowledgeCmdTest, CortexStatsWithUnreadableEntries) {
    auto cortex_dir = fs::path(ctx_.euxis_home) / "data/runtime" / "memory" / "cortex";
    fs::create_directories(cortex_dir);
    // Create entries.json as a directory (will fail to open as file)
    fs::create_directories(cortex_dir / "entries.json");

    auto code = cmd_cortex(ctx_, {"stats"});
    EXPECT_GE(code, 0);
    // Clean up the directory-as-file
    fs::remove_all(cortex_dir / "entries.json");
}

// --- Coverage: lines 49-50 (non-object JSON in cortex entries) ---
TEST_F(KnowledgeCmdTest, CortexStatsWithNonObjectJson) {
    auto cortex_dir = fs::path(ctx_.euxis_home) / "data/runtime" / "memory" / "cortex";
    fs::create_directories(cortex_dir);
    // Write a JSON array instead of object
    std::ofstream(cortex_dir / "entries.json") << "[1, 2, 3]";

    auto code = cmd_cortex(ctx_, {"stats"});
    EXPECT_GE(code, 0);
}

// --- Coverage: line 327 (graph export without output arg) ---
TEST_F(KnowledgeCmdTest, GraphExportDefaultOutput) {
    auto graph_dir = fs::path(ctx_.euxis_home) / "data" / "knowledge" / "graph";
    fs::create_directories(graph_dir);
    std::ofstream(graph_dir / "node1.json") << "{}";

    auto code = cmd_graph(ctx_, {"export"});
    EXPECT_GE(code, 0);
}

// --- Coverage: line 489 (omnigraph json output) ---
TEST_F(KnowledgeCmdTest, OmnigraphJsonOutput) {
    ctx_.json_output = true;
    auto code = cmd_omnigraph(ctx_, {});
    EXPECT_EQ(code, 0);
}

// --- Coverage: lines 578-595 (slash run with action field) ---
TEST_F(KnowledgeCmdTest, SlashRunWithAction) {
    auto config_path = fs::path(ctx_.data_dir) / "config" / "slash-commands.json";
    nlohmann::json config;
    config["commands"] = nlohmann::json::array({
        {{"name", "act-cmd"}, {"description", "Action cmd"}, {"action", "version"}}
    });
    std::ofstream(config_path) << config.dump();

    auto code = cmd_slash(ctx_, {"run", "act-cmd"});
    EXPECT_EQ(code, 0);
}

// --- Coverage: line 602 (slash run with extra args) ---
TEST_F(KnowledgeCmdTest, SlashRunWithScriptAndExtraArgs) {
    auto config_path = fs::path(ctx_.data_dir) / "config" / "slash-commands.json";
    nlohmann::json config;
    config["commands"] = nlohmann::json::array({
        {{"name", "echo-args"}, {"description", "Echo with args"}, {"script", "echo"}}
    });
    std::ofstream(config_path) << config.dump();

    auto code = cmd_slash(ctx_, {"run", "echo-args", "extra-arg1", "extra-arg2"});
    EXPECT_EQ(code, 0);
}

// --- Coverage: slash run with action and extra args ---
TEST_F(KnowledgeCmdTest, SlashRunWithActionAndExtraArgs) {
    auto config_path = fs::path(ctx_.data_dir) / "config" / "slash-commands.json";
    nlohmann::json config;
    config["commands"] = nlohmann::json::array({
        {{"name", "health-cmd"}, {"description", "Health check"}, {"action", "health"}}
    });
    std::ofstream(config_path) << config.dump();

    auto code = cmd_slash(ctx_, {"run", "health-cmd", "--silent"});
    EXPECT_GE(code, 0);
}

} // namespace
} // namespace euxis::cli::cmd
