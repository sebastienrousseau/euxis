#include <gtest/gtest.h>
#include "euxis/cli/registry_client.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include <sqlite3.h>

namespace euxis::cli {
namespace {

class RegistryClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/euxis_test_registry_" + std::to_string(getpid());
        auto agents_dir = std::filesystem::path(test_dir_) / "agents";
        std::filesystem::create_directories(agents_dir);

        // Create a minimal registry.json
        nlohmann::json reg;
        reg["agents"] = nlohmann::json::array({
            {{"agent_id", "test-agent"}, {"role", "tester"}, {"version", "1.0"},
             {"tier", "code"}, {"tags", {"test", "ci"}}, {"capabilities", {"testing"}}},
            {{"agent_id", "helper"}, {"role", "helper"}, {"version", "2.0"},
             {"tier", "routine"}, {"tags", {"utility"}}, {"capabilities", {"help"}}}
        });
        std::ofstream(agents_dir / "registry.json") << reg.dump(2);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
};

TEST_F(RegistryClientTest, HasJson) {
    RegistryClient reg(test_dir_);
    EXPECT_TRUE(reg.has_json());
}

TEST_F(RegistryClientTest, ListAgents) {
    RegistryClient reg(test_dir_);
    auto agents = reg.list_agents();
    EXPECT_EQ(agents.size(), 2u);
}

TEST_F(RegistryClientTest, GetAgent) {
    RegistryClient reg(test_dir_);
    auto agent = reg.get_agent("test-agent");
    ASSERT_TRUE(agent.has_value());
    EXPECT_EQ(agent->id, "test-agent");
    EXPECT_EQ(agent->role, "tester");
    EXPECT_EQ(agent->tier, "code");
}

TEST_F(RegistryClientTest, GetAgentNotFound) {
    RegistryClient reg(test_dir_);
    auto agent = reg.get_agent("nonexistent");
    EXPECT_FALSE(agent.has_value());
}

TEST_F(RegistryClientTest, AgentCount) {
    RegistryClient reg(test_dir_);
    EXPECT_EQ(reg.agent_count(), 2);
}

TEST_F(RegistryClientTest, FindByTag) {
    RegistryClient reg(test_dir_);
    auto agents = reg.find_by_tag("test");
    EXPECT_EQ(agents.size(), 1u);
    EXPECT_EQ(agents[0].id, "test-agent");
}

TEST_F(RegistryClientTest, FindByTier) {
    RegistryClient reg(test_dir_);
    auto agents = reg.find_by_tier("routine");
    EXPECT_EQ(agents.size(), 1u);
    EXPECT_EQ(agents[0].id, "helper");
}

TEST_F(RegistryClientTest, FindByCapability) {
    RegistryClient reg(test_dir_);
    auto agents = reg.find_by_capability("testing");
    EXPECT_EQ(agents.size(), 1u);
}

TEST_F(RegistryClientTest, RegisterPlugin) {
    RegistryClient reg(test_dir_);
    nlohmann::json manifest = {{"agent_id", "custom"}, {"role", "custom"}};
    EXPECT_TRUE(reg.register_plugin("custom-agent", manifest));

    auto meta = std::filesystem::path(test_dir_) / "config" / "plugins" / "custom-agent.json";
    EXPECT_TRUE(std::filesystem::exists(meta));
}

TEST_F(RegistryClientTest, RegisterPluginInvalidId) {
    RegistryClient reg(test_dir_);
    EXPECT_FALSE(reg.register_plugin("bad/agent", {}));
    EXPECT_FALSE(reg.register_plugin("bad agent", {}));
}

TEST_F(RegistryClientTest, UnregisterPlugin) {
    RegistryClient reg(test_dir_);
    reg.register_plugin("to-remove", {});
    EXPECT_TRUE(reg.unregister_plugin("to-remove"));
    EXPECT_FALSE(reg.unregister_plugin("to-remove")); // already removed
}

TEST_F(RegistryClientTest, EmptyDataDir) {
    std::string empty_dir = "/tmp/euxis_test_empty_" + std::to_string(getpid());
    std::filesystem::create_directories(empty_dir);
    RegistryClient reg(empty_dir);
    EXPECT_EQ(reg.agent_count(), 0);
    EXPECT_TRUE(reg.list_agents().empty());
    std::filesystem::remove_all(empty_dir);
}

TEST_F(RegistryClientTest, HasSqliteReturnsFalseWithoutDb) {
    RegistryClient reg(test_dir_);
    EXPECT_FALSE(reg.has_sqlite());
}

TEST_F(RegistryClientTest, MoveConstruction) {
    RegistryClient reg(test_dir_);
    EXPECT_TRUE(reg.has_json());
    RegistryClient moved(std::move(reg));
    EXPECT_TRUE(moved.has_json());
    EXPECT_EQ(moved.agent_count(), 2);
}

TEST_F(RegistryClientTest, MoveAssignment) {
    RegistryClient reg(test_dir_);
    std::string other_dir = "/tmp/euxis_test_other_" + std::to_string(getpid());
    std::filesystem::create_directories(other_dir);
    RegistryClient other(other_dir);
    other = std::move(reg);
    EXPECT_TRUE(other.has_json());
    EXPECT_EQ(other.agent_count(), 2);
    std::filesystem::remove_all(other_dir);
}

TEST_F(RegistryClientTest, FindByTagNoMatch) {
    RegistryClient reg(test_dir_);
    auto agents = reg.find_by_tag("nonexistent-tag");
    EXPECT_TRUE(agents.empty());
}

TEST_F(RegistryClientTest, FindByTierNoMatch) {
    RegistryClient reg(test_dir_);
    auto agents = reg.find_by_tier("nonexistent-tier");
    EXPECT_TRUE(agents.empty());
}

TEST_F(RegistryClientTest, FindByCapabilityNoMatch) {
    RegistryClient reg(test_dir_);
    auto agents = reg.find_by_capability("nonexistent-cap");
    EXPECT_TRUE(agents.empty());
}

TEST_F(RegistryClientTest, GetAgentVerifiesAllFields) {
    RegistryClient reg(test_dir_);
    auto agent = reg.get_agent("test-agent");
    ASSERT_TRUE(agent.has_value());
    EXPECT_EQ(agent->version, "1.0");
    EXPECT_EQ(agent->tags.size(), 2u);
    EXPECT_EQ(agent->tags[0], "test");
    EXPECT_EQ(agent->tags[1], "ci");
    EXPECT_EQ(agent->capabilities.size(), 1u);
    EXPECT_EQ(agent->capabilities[0], "testing");
}

TEST_F(RegistryClientTest, GetAgentHelperFields) {
    RegistryClient reg(test_dir_);
    auto agent = reg.get_agent("helper");
    ASSERT_TRUE(agent.has_value());
    EXPECT_EQ(agent->version, "2.0");
    EXPECT_EQ(agent->tier, "routine");
    EXPECT_EQ(agent->tags.size(), 1u);
    EXPECT_EQ(agent->capabilities.size(), 1u);
    EXPECT_EQ(agent->capabilities[0], "help");
}

TEST_F(RegistryClientTest, ListSquadsWithoutSquadsJson) {
    RegistryClient reg(test_dir_);
    auto squads = reg.list_squads();
    EXPECT_TRUE(squads.empty());
}

TEST_F(RegistryClientTest, GetSquadNotFound) {
    RegistryClient reg(test_dir_);
    auto squad = reg.get_squad("nonexistent");
    EXPECT_FALSE(squad.has_value());
}

TEST_F(RegistryClientTest, ListSquadsWithSquadsJson) {
    // Create squads.json in agents dir
    nlohmann::json squads_json;
    squads_json["squads"] = nlohmann::json::array({
        {{"id", "alpha"}, {"name", "Alpha Squad"}, {"purpose", "Testing"},
         {"lead", "test-agent"}, {"members", {"test-agent", "helper"}}}
    });
    std::ofstream(std::filesystem::path(test_dir_) / "agents" / "squads.json")
        << squads_json.dump(2);

    RegistryClient reg(test_dir_);
    auto squads = reg.list_squads();
    ASSERT_EQ(squads.size(), 1u);
    EXPECT_EQ(squads[0].id, "alpha");
    EXPECT_EQ(squads[0].name, "Alpha Squad");
    EXPECT_EQ(squads[0].purpose, "Testing");
    EXPECT_EQ(squads[0].lead, "test-agent");
    EXPECT_EQ(squads[0].members.size(), 2u);
}

TEST_F(RegistryClientTest, GetSquadFound) {
    nlohmann::json squads_json;
    squads_json["squads"] = nlohmann::json::array({
        {{"id", "alpha"}, {"name", "Alpha"}, {"purpose", "Test"},
         {"lead", "lead-1"}, {"members", {"a", "b"}}},
        {{"id", "beta"}, {"name", "Beta"}, {"purpose", "Dev"},
         {"lead", "lead-2"}, {"members", {"c"}}}
    });
    std::ofstream(std::filesystem::path(test_dir_) / "agents" / "squads.json")
        << squads_json.dump(2);

    RegistryClient reg(test_dir_);
    auto squad = reg.get_squad("beta");
    ASSERT_TRUE(squad.has_value());
    EXPECT_EQ(squad->id, "beta");
    EXPECT_EQ(squad->name, "Beta");
    EXPECT_EQ(squad->lead, "lead-2");
}

TEST_F(RegistryClientTest, UnregisterPluginNonExistent) {
    RegistryClient reg(test_dir_);
    EXPECT_FALSE(reg.unregister_plugin("does-not-exist"));
}

TEST_F(RegistryClientTest, RegisterPluginWithSpecialValidChars) {
    RegistryClient reg(test_dir_);
    EXPECT_TRUE(reg.register_plugin("valid-agent_1", {{"key", "val"}}));
}

TEST_F(RegistryClientTest, RegisterPluginInvalidCharsRejected) {
    RegistryClient reg(test_dir_);
    EXPECT_FALSE(reg.register_plugin("bad@agent", {}));
    EXPECT_FALSE(reg.register_plugin("bad.agent", {}));
    EXPECT_FALSE(reg.register_plugin("bad#agent", {}));
}

TEST_F(RegistryClientTest, MalformedRegistryJsonHandled) {
    // Overwrite registry.json with invalid JSON
    std::ofstream(std::filesystem::path(test_dir_) / "agents" / "registry.json")
        << "{ broken json ";

    RegistryClient reg(test_dir_);
    // Should not crash, just have empty results
    EXPECT_EQ(reg.agent_count(), 0);
    EXPECT_TRUE(reg.list_agents().empty());
}

TEST_F(RegistryClientTest, RegistryWithIdField) {
    // Test parse_agent with "id" instead of "agent_id"
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"id", "alt-agent"}, {"role", "alt"}, {"version", "3.0"},
         {"tier", "reason"}, {"path", "prompts/alt.md"}}
    });
    std::ofstream(std::filesystem::path(test_dir_) / "agents" / "registry.json")
        << reg.dump(2);

    RegistryClient client(test_dir_);
    auto agent = client.get_agent("alt-agent");
    ASSERT_TRUE(agent.has_value());
    EXPECT_EQ(agent->id, "alt-agent");
    EXPECT_EQ(agent->prompt_path, "prompts/alt.md");
}

TEST_F(RegistryClientTest, FindByTagMultipleMatches) {
    // Create registry with multiple agents sharing a tag
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "a1"}, {"role", "r1"}, {"tags", {"shared", "x"}}},
        {{"agent_id", "a2"}, {"role", "r2"}, {"tags", {"shared", "y"}}},
        {{"agent_id", "a3"}, {"role", "r3"}, {"tags", {"other"}}}
    });
    std::ofstream(std::filesystem::path(test_dir_) / "agents" / "registry.json")
        << reg.dump(2);

    RegistryClient client(test_dir_);
    auto agents = client.find_by_tag("shared");
    EXPECT_EQ(agents.size(), 2u);
}

TEST_F(RegistryClientTest, FindByCapabilityMultipleMatches) {
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "a1"}, {"capabilities", {"code", "test"}}},
        {{"agent_id", "a2"}, {"capabilities", {"code"}}},
        {{"agent_id", "a3"}, {"capabilities", {"review"}}}
    });
    std::ofstream(std::filesystem::path(test_dir_) / "agents" / "registry.json")
        << reg.dump(2);

    RegistryClient client(test_dir_);
    auto agents = client.find_by_capability("code");
    EXPECT_EQ(agents.size(), 2u);
}

// ===========================================================================
// SQLite-backed tests: exercise lines 30-39, 90-108, 114-133, 147-169, 200-212
// ===========================================================================

class RegistryClientSqliteTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/euxis_test_registry_sqlite_" + std::to_string(getpid());
        auto agents_dir = std::filesystem::path(test_dir_) / "agents";
        std::filesystem::create_directories(agents_dir);

        // Create SQLite registry.db with agents, tags, and agent_tags tables
        auto db_path = agents_dir / "registry.db";
        sqlite3* db = nullptr;
        ASSERT_EQ(sqlite3_open(db_path.c_str(), &db), SQLITE_OK);

        const char* schema =
            "CREATE TABLE agents ("
            "  id TEXT PRIMARY KEY,"
            "  role TEXT,"
            "  version TEXT,"
            "  tier TEXT,"
            "  path TEXT"
            ");"
            "CREATE TABLE tags ("
            "  tag_id INTEGER PRIMARY KEY,"
            "  name TEXT UNIQUE"
            ");"
            "CREATE TABLE agent_tags ("
            "  agent_id TEXT,"
            "  tag_id INTEGER,"
            "  FOREIGN KEY(agent_id) REFERENCES agents(id),"
            "  FOREIGN KEY(tag_id) REFERENCES tags(tag_id)"
            ");"
            "INSERT INTO agents VALUES('sql-agent-1', 'coder', '1.0', 'code', 'prompts/sql1.md');"
            "INSERT INTO agents VALUES('sql-agent-2', 'reviewer', '2.0', 'reason', NULL);"
            "INSERT INTO agents VALUES('sql-agent-3', 'helper', '1.5', 'routine', 'prompts/sql3.md');"
            "INSERT INTO tags VALUES(1, 'backend');"
            "INSERT INTO tags VALUES(2, 'frontend');"
            "INSERT INTO agent_tags VALUES('sql-agent-1', 1);"
            "INSERT INTO agent_tags VALUES('sql-agent-1', 2);"
            "INSERT INTO agent_tags VALUES('sql-agent-3', 1);";

        char* err = nullptr;
        ASSERT_EQ(sqlite3_exec(db, schema, nullptr, nullptr, &err), SQLITE_OK)
            << (err ? err : "unknown");
        sqlite3_close(db);

        // Also write a JSON registry so we can verify SQLite takes priority
        nlohmann::json reg;
        reg["agents"] = nlohmann::json::array({
            {{"agent_id", "json-only-agent"}, {"role", "json"}, {"version", "0.1"},
             {"tier", "data"}, {"tags", {"json-tag"}}}
        });
        std::ofstream(agents_dir / "registry.json") << reg.dump(2);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
};

// --- Coverage: lines 30-39 (constructor opens SQLite) ---
TEST_F(RegistryClientSqliteTest, HasSqliteReturnsTrue) {
    RegistryClient reg(test_dir_);
    EXPECT_TRUE(reg.has_sqlite());
    EXPECT_TRUE(reg.has_json());
}

// --- Coverage: lines 90-108 (list_agents from SQLite) ---
TEST_F(RegistryClientSqliteTest, ListAgentsFromSqlite) {
    RegistryClient reg(test_dir_);
    auto agents = reg.list_agents();
    // Should get 3 agents from SQLite, not 1 from JSON
    ASSERT_EQ(agents.size(), 3u);
    // Ordered by agent_id
    EXPECT_EQ(agents[0].id, "sql-agent-1");
    EXPECT_EQ(agents[0].role, "coder");
    EXPECT_EQ(agents[0].version, "1.0");
    EXPECT_EQ(agents[0].tier, "code");
    EXPECT_EQ(agents[0].prompt_path, "prompts/sql1.md");

    EXPECT_EQ(agents[1].id, "sql-agent-2");
    EXPECT_EQ(agents[1].role, "reviewer");
    EXPECT_EQ(agents[1].tier, "reason");
    // NULL prompt_path should result in empty string
    EXPECT_TRUE(agents[1].prompt_path.empty());

    EXPECT_EQ(agents[2].id, "sql-agent-3");
    EXPECT_EQ(agents[2].prompt_path, "prompts/sql3.md");
}

// --- Coverage: lines 114-128 (get_agent found in SQLite) ---
TEST_F(RegistryClientSqliteTest, GetAgentFromSqlite) {
    RegistryClient reg(test_dir_);
    auto agent = reg.get_agent("sql-agent-1");
    ASSERT_TRUE(agent.has_value());
    EXPECT_EQ(agent->id, "sql-agent-1");
    EXPECT_EQ(agent->role, "coder");
    EXPECT_EQ(agent->version, "1.0");
    EXPECT_EQ(agent->tier, "code");
    EXPECT_EQ(agent->prompt_path, "prompts/sql1.md");
}

// --- Coverage: lines 119-130 (get_agent NOT found in SQLite, step returns SQLITE_DONE) ---
TEST_F(RegistryClientSqliteTest, GetAgentFromSqliteNotFound) {
    RegistryClient reg(test_dir_);
    // Agent not in SQLite DB and not in JSON either
    auto agent = reg.get_agent("nonexistent");
    EXPECT_FALSE(agent.has_value());
}

// --- Coverage: line 126 (get_agent with NULL prompt_path column) ---
TEST_F(RegistryClientSqliteTest, GetAgentNullPromptPath) {
    RegistryClient reg(test_dir_);
    auto agent = reg.get_agent("sql-agent-2");
    ASSERT_TRUE(agent.has_value());
    EXPECT_EQ(agent->id, "sql-agent-2");
    EXPECT_TRUE(agent->prompt_path.empty());
}

// --- Coverage: lines 147-169 (find_by_tag from SQLite with JOIN) ---
TEST_F(RegistryClientSqliteTest, FindByTagFromSqlite) {
    RegistryClient reg(test_dir_);
    auto agents = reg.find_by_tag("backend");
    ASSERT_EQ(agents.size(), 2u);
    // Both sql-agent-1 and sql-agent-3 have 'backend' tag
    std::vector<std::string> ids;
    for (const auto& a : agents) ids.push_back(a.id);
    EXPECT_NE(std::find(ids.begin(), ids.end(), "sql-agent-1"), ids.end());
    EXPECT_NE(std::find(ids.begin(), ids.end(), "sql-agent-3"), ids.end());
}

TEST_F(RegistryClientSqliteTest, FindByTagFromSqliteSingleMatch) {
    RegistryClient reg(test_dir_);
    auto agents = reg.find_by_tag("frontend");
    ASSERT_EQ(agents.size(), 1u);
    EXPECT_EQ(agents[0].id, "sql-agent-1");
}

TEST_F(RegistryClientSqliteTest, FindByTagFromSqliteNoMatch) {
    RegistryClient reg(test_dir_);
    auto agents = reg.find_by_tag("nonexistent-tag");
    EXPECT_TRUE(agents.empty());
}

// --- Coverage: lines 200-212 (agent_count from SQLite) ---
TEST_F(RegistryClientSqliteTest, AgentCountFromSqlite) {
    RegistryClient reg(test_dir_);
    EXPECT_EQ(reg.agent_count(), 3);
}

// --- Coverage: find_by_tier uses list_agents (SQLite path) ---
TEST_F(RegistryClientSqliteTest, FindByTierFromSqlite) {
    RegistryClient reg(test_dir_);
    auto agents = reg.find_by_tier("code");
    ASSERT_EQ(agents.size(), 1u);
    EXPECT_EQ(agents[0].id, "sql-agent-1");

    auto reason = reg.find_by_tier("reason");
    ASSERT_EQ(reason.size(), 1u);
    EXPECT_EQ(reason[0].id, "sql-agent-2");
}

// --- Coverage: find_by_capability uses list_agents (SQLite path) ---
TEST_F(RegistryClientSqliteTest, FindByCapabilityFromSqliteEmpty) {
    RegistryClient reg(test_dir_);
    // SQLite agents don't have capabilities loaded (SQLite path doesn't populate tags/caps)
    auto agents = reg.find_by_capability("anything");
    EXPECT_TRUE(agents.empty());
}

// --- Coverage: lines 35-38 (constructor with corrupt SQLite file) ---
// sqlite3_open_v2 may or may not fail on a corrupt file (it defers validation).
// If it succeeds, has_sqlite() is true but queries fail, falling to JSON.
// Either way, list_agents / agent_count must return the JSON data.
TEST(RegistryClientCorruptDbTest, CorruptSqliteStillReturnsAgents) {
    std::string test_dir = "/tmp/euxis_test_corrupt_db_" + std::to_string(getpid());
    auto agents_dir = std::filesystem::path(test_dir) / "agents";
    std::filesystem::create_directories(agents_dir);

    // Write invalid data as registry.db
    std::ofstream(agents_dir / "registry.db") << "this is not a sqlite database";

    // Write valid JSON
    nlohmann::json reg;
    reg["agents"] = nlohmann::json::array({
        {{"agent_id", "json-agent"}, {"role", "fallback"}, {"version", "1.0"}, {"tier", "code"}}
    });
    std::ofstream(agents_dir / "registry.json") << reg.dump(2);

    RegistryClient client(test_dir);
    EXPECT_TRUE(client.has_json());
    // Whether SQLite open succeeded or not, we should still get agents from JSON fallback
    auto agents = client.list_agents();
    // If SQLite open failed: list_agents uses JSON path, returns 1 agent.
    // If SQLite open succeeded but corrupt: prepare_v2 fails, falls to list_agents_json, returns 1.
    EXPECT_GE(agents.size(), 1u);
    // agent_count also falls back
    EXPECT_GE(client.agent_count(), 1);

    std::filesystem::remove_all(test_dir);
}

// --- Coverage: get_agent falls back to JSON when not in SQLite ---
TEST_F(RegistryClientSqliteTest, GetAgentFallsToJsonWhenSqliteMisses) {
    RegistryClient reg(test_dir_);
    // json-only-agent is only in registry.json, not in SQLite
    // get_agent: SQLite path returns no row, then checks JSON fallback (line 136-142)
    auto agent = reg.get_agent("json-only-agent");
    // This tests the JSON fallback after SQLite miss -- but get_agent with SQLite
    // returns nullopt if not found in SQLite (lines 129-130), then falls to JSON (136-142)
    ASSERT_TRUE(agent.has_value());
    EXPECT_EQ(agent->id, "json-only-agent");
    EXPECT_EQ(agent->role, "json");
}

// --- Coverage: move construction with SQLite handle ---
TEST_F(RegistryClientSqliteTest, MoveConstructionPreservesSqlite) {
    RegistryClient reg(test_dir_);
    EXPECT_TRUE(reg.has_sqlite());
    RegistryClient moved(std::move(reg));
    EXPECT_TRUE(moved.has_sqlite());
    EXPECT_EQ(moved.agent_count(), 3);
}

// --- Coverage: move assignment with SQLite handle ---
TEST_F(RegistryClientSqliteTest, MoveAssignmentPreservesSqlite) {
    RegistryClient reg(test_dir_);
    std::string other_dir = "/tmp/euxis_test_sqlite_other_" + std::to_string(getpid());
    std::filesystem::create_directories(other_dir);
    RegistryClient other(other_dir);
    EXPECT_FALSE(other.has_sqlite());
    other = std::move(reg);
    EXPECT_TRUE(other.has_sqlite());
    EXPECT_EQ(other.agent_count(), 3);
    std::filesystem::remove_all(other_dir);
}

// --- Issue #60: JsonCache regression ----------------------------------------
//
// First construction parses registry.json + squads.json from disk and
// populates the JsonCache. Second construction (with the registry files
// unchanged) restores from the cache without re-parsing. The on-disk
// cache database is what the test inspects, since the cache is owned
// inside RegistryClient::Impl.

} // namespace
} // namespace euxis::cli

#include "euxis/cache/json_cache.hpp"

namespace euxis::cli {
namespace {

class RegistryClientCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache_home_ = "/tmp/euxis_test_cachehome_" +
                      std::to_string(getpid()) + "_" +
                      std::to_string(::testing::UnitTest::GetInstance()
                                         ->current_test_info()
                                         ->result()
                                         ->total_part_count());
        std::filesystem::create_directories(cache_home_);
        prev_xdg_ = std::getenv("XDG_CACHE_HOME");
        ::setenv("XDG_CACHE_HOME", cache_home_.c_str(), /*overwrite=*/1);

        test_dir_ = "/tmp/euxis_test_regcache_" +
                    std::to_string(getpid()) + "_" +
                    std::to_string(::testing::UnitTest::GetInstance()
                                       ->current_test_info()
                                       ->result()
                                       ->total_part_count());
        auto agents_dir = std::filesystem::path(test_dir_) / "agents";
        std::filesystem::create_directories(agents_dir);

        nlohmann::json reg;
        reg["agents"] = nlohmann::json::array({
            {{"agent_id", "alpha"}, {"role", "tester"}, {"version", "1.0"},
             {"tier", "code"}, {"tags", {"ci"}}, {"capabilities", {"testing"}}},
        });
        std::ofstream(agents_dir / "registry.json") << reg.dump(2);

        nlohmann::json squads;
        squads["squads"] = nlohmann::json::array({
            {{"id", "s1"}, {"name", "S1"}, {"purpose", "test"},
             {"lead", "alpha"}, {"members", {"alpha"}}},
        });
        std::ofstream(agents_dir / "squads.json") << squads.dump(2);
    }

    void TearDown() override {
        if (prev_xdg_ != nullptr) {
            ::setenv("XDG_CACHE_HOME", prev_xdg_, /*overwrite=*/1);
        } else {
            ::unsetenv("XDG_CACHE_HOME");
        }
        std::filesystem::remove_all(test_dir_);
        std::filesystem::remove_all(cache_home_);
    }

    [[nodiscard]] auto cache_db_path() const -> std::filesystem::path {
        return std::filesystem::path{cache_home_} / "euxis" /
               "registry-cache.sqlite";
    }

    std::string test_dir_;
    std::string cache_home_;
    const char* prev_xdg_{nullptr};
};

// First construction must populate the cache with the registry.json
// payload; the entry must be readable through a sibling JsonCache.
TEST_F(RegistryClientCacheTest, FirstConstructionPopulatesJsonCache) {
    {
        RegistryClient reg(test_dir_);
        EXPECT_TRUE(reg.has_json());
        EXPECT_GE(reg.agent_count(), 1);
    }

    ASSERT_TRUE(std::filesystem::exists(cache_db_path()))
        << "Expected the cache db at " << cache_db_path();

    auto cache = euxis::cache::JsonCache::open(cache_db_path());
    ASSERT_TRUE(cache.has_value()) << cache.error().message;

    auto registry_path =
        std::filesystem::path{test_dir_} / "agents" / "registry.json";
    auto hit = cache->get(registry_path);
    ASSERT_TRUE(hit.has_value()) << hit.error().message;
    ASSERT_TRUE(hit->has_value())
        << "registry.json was not cached on first construction";

    EXPECT_EQ((**hit).at("agents").at(0).at("agent_id").get<std::string>(),
              "alpha");
    EXPECT_GE(cache->stats().hits, 1u);
}

// Second construction with the source files unchanged must NOT grow the
// cache db on disk. A grown db means the warm path re-parsed and
// re-stored — i.e. the cache lookup missed when it shouldn't have. This
// is the regression test #60 asks for.
TEST_F(RegistryClientCacheTest, SecondConstructionDoesNotRewriteCache) {
    {
        RegistryClient reg1(test_dir_);
        EXPECT_GE(reg1.agent_count(), 1);
    }
    ASSERT_TRUE(std::filesystem::exists(cache_db_path()));
    auto size_after_cold = std::filesystem::file_size(cache_db_path());

    {
        RegistryClient reg2(test_dir_);
        EXPECT_GE(reg2.agent_count(), 1);
    }
    auto size_after_warm = std::filesystem::file_size(cache_db_path());

    EXPECT_EQ(size_after_cold, size_after_warm)
        << "Warm construction grew the cache db from " << size_after_cold
        << " to " << size_after_warm
        << " bytes — JsonCache failed to hit on registry.json/squads.json";
}

// Editing the source file between constructions must invalidate the
// cache entry — the content-hash check inside JsonCache::get is the
// defence. Verifies the invariant the cache primitive promised.
TEST_F(RegistryClientCacheTest, SourceEditInvalidatesCacheEntry) {
    {
        RegistryClient reg(test_dir_);
        EXPECT_GE(reg.agent_count(), 1);
    }
    auto registry_path =
        std::filesystem::path{test_dir_} / "agents" / "registry.json";

    nlohmann::json reg2;
    reg2["agents"] = nlohmann::json::array({
        {{"agent_id", "alpha"}, {"role", "tester"}, {"version", "1.0"},
         {"tier", "code"}, {"tags", {"ci"}}, {"capabilities", {"testing"}}},
        {{"agent_id", "beta"}, {"role", "auditor"}, {"version", "1.1"},
         {"tier", "routine"}, {"tags", {"sec"}}, {"capabilities", {"audit"}}},
    });
    std::ofstream(registry_path) << reg2.dump(2);

    RegistryClient reg(test_dir_);
    EXPECT_GE(reg.agent_count(), 2)
        << "Edited registry.json was not re-parsed — cache returned stale";
}

} // namespace
} // namespace euxis::cli
