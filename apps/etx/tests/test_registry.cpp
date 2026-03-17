#include <euxis/etx/registry.hpp>
#include <euxis/etx/config.hpp>

#include <gtest/gtest.h>
#include <QDir>
#include <QFile>

namespace euxis::etx {
namespace {

// Test with defaults (no data_dir → falls back to 8 hardcoded agents)
class FleetRegistryDefaultTest : public ::testing::Test {
protected:
    FleetRegistry registry_{QString{}}; // empty data_dir → defaults
};

TEST_F(FleetRegistryDefaultTest, LoadDefaultsPopulatesAgents) {
    EXPECT_FALSE(registry_.agents().empty());
    EXPECT_EQ(registry_.agents().size(), 8);
}

TEST_F(FleetRegistryDefaultTest, FindExistingAgent) {
    const auto* agent = registry_.find("code-agent");
    ASSERT_NE(agent, nullptr);
    EXPECT_EQ(agent->name, "Code Agent");
    EXPECT_EQ(agent->type, "agent");
}

TEST_F(FleetRegistryDefaultTest, FindReturnsNullForMissing) {
    EXPECT_EQ(registry_.find("nonexistent-agent"), nullptr);
}

TEST_F(FleetRegistryDefaultTest, FilterByName) {
    auto results = registry_.filter("Code");
    EXPECT_FALSE(results.empty());
    bool found = false;
    for (const auto& a : results) {
        if (a.id == "code-agent") found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(FleetRegistryDefaultTest, FilterByDescription) {
    auto results = registry_.filter("WASM");
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0].id, "code-agent");
}

TEST_F(FleetRegistryDefaultTest, FilterById) {
    auto results = registry_.filter("security-agent");
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].name, "Security Agent");
}

TEST_F(FleetRegistryDefaultTest, FilterCaseInsensitive) {
    auto upper = registry_.filter("CODE");
    auto lower = registry_.filter("code");
    EXPECT_EQ(upper.size(), lower.size());
    EXPECT_FALSE(upper.empty());
}

TEST_F(FleetRegistryDefaultTest, EmptyFilterReturnsAll) {
    auto results = registry_.filter("");
    EXPECT_EQ(results.size(), registry_.agents().size());
}

TEST_F(FleetRegistryDefaultTest, AgentTypesPresent) {
    bool has_agent = false, has_squad = false, has_combo = false;
    for (const auto& a : registry_.agents()) {
        if (a.type == "agent") has_agent = true;
        if (a.type == "squad") has_squad = true;
        if (a.type == "combo") has_combo = true;
    }
    EXPECT_TRUE(has_agent);
    EXPECT_TRUE(has_squad);
    EXPECT_TRUE(has_combo);
}

TEST_F(FleetRegistryDefaultTest, AgentStatusesPresent) {
    bool has_idle = false, has_running = false, has_error = false;
    for (const auto& a : registry_.agents()) {
        if (a.status == "idle") has_idle = true;
        if (a.status == "running") has_running = true;
        if (a.status == "error") has_error = true;
    }
    EXPECT_TRUE(has_idle);
    EXPECT_TRUE(has_running);
    EXPECT_TRUE(has_error);
}

TEST_F(FleetRegistryDefaultTest, RefreshReloadsAgents) {
    size_t before = registry_.agents().size();
    registry_.refresh();
    EXPECT_EQ(registry_.agents().size(), before);
}

TEST_F(FleetRegistryDefaultTest, CountMethods) {
    EXPECT_EQ(registry_.agent_count(), 8);
    EXPECT_EQ(registry_.squad_count(), 0); // defaults have no squads
    EXPECT_EQ(registry_.combo_count(), 0); // defaults have no combos
}

TEST_F(FleetRegistryDefaultTest, InvalidDirFallsBackToDefaults) {
    FleetRegistry reg("/nonexistent/path/that/does/not/exist");
    EXPECT_EQ(reg.agent_count(), 8);
    EXPECT_NE(reg.find("code-agent"), nullptr);
}

// Test with real data (skip if data is not available)
class FleetRegistryRealDataTest : public ::testing::Test {
protected:
    void SetUp() override {
        data_dir_ = ETXConfig::data_dir();
        QDir dir(data_dir_ + "/agents");
        if (!dir.exists()) {
            GTEST_SKIP() << "data not available at " << data_dir_.toStdString();
        }
    }
    QString data_dir_;
};

TEST_F(FleetRegistryRealDataTest, LoadsRealAgents) {
    FleetRegistry reg(data_dir_);
    EXPECT_GT(reg.agent_count(), 8);
}

TEST_F(FleetRegistryRealDataTest, LoadsSquadsAndCombos) {
    FleetRegistry reg(data_dir_);
    EXPECT_GT(reg.squad_count(), 0);
    EXPECT_GT(reg.combo_count(), 0);
}

TEST_F(FleetRegistryRealDataTest, LoadsPlaybooks) {
    FleetRegistry reg(data_dir_);
    EXPECT_GT(static_cast<int>(reg.playbooks().size()), 0);
}

TEST_F(FleetRegistryRealDataTest, FilterByTier) {
    FleetRegistry reg(data_dir_);
    auto core = reg.filter_by_tier("core");
    auto fleet = reg.filter_by_tier("fleet");
    EXPECT_GT(core.size(), 0);
    EXPECT_GT(fleet.size(), 0);
    EXPECT_EQ(core.size() + fleet.size(), static_cast<size_t>(reg.agent_count()));
}

TEST_F(FleetRegistryRealDataTest, FindSquad) {
    FleetRegistry reg(data_dir_);
    const auto* squad = reg.find_squad("vision");
    if (squad) {
        EXPECT_EQ(squad->name, "Vision Squad");
        EXPECT_FALSE(squad->members.isEmpty());
    }
}

// =============================================================================
// FleetRegistry — filter_by_tier, filter_by_tag, find_squad with defaults
// (covers lines 131, 134-148 in registry.cpp)
// =============================================================================

TEST_F(FleetRegistryDefaultTest, FilterByTierCore) {
    auto core = registry_.filter_by_tier("core");
    EXPECT_FALSE(core.empty());
    for (const auto& a : core) {
        EXPECT_EQ(a.tier, "core");
    }
}

TEST_F(FleetRegistryDefaultTest, FilterByTierFleet) {
    auto fleet = registry_.filter_by_tier("fleet");
    EXPECT_FALSE(fleet.empty());
    for (const auto& a : fleet) {
        EXPECT_EQ(a.tier, "fleet");
    }
}

TEST_F(FleetRegistryDefaultTest, FilterByTierNonExistent) {
    auto results = registry_.filter_by_tier("nonexistent");
    EXPECT_TRUE(results.empty());
}

TEST_F(FleetRegistryDefaultTest, FilterByTagEmpty) {
    // Default agents have no tags, so filter_by_tag returns empty
    auto results = registry_.filter_by_tag("any-tag");
    EXPECT_TRUE(results.empty());
}

TEST_F(FleetRegistryDefaultTest, FindSquadNonExistent) {
    // Default registry has no squads (only agents with type "squad")
    EXPECT_EQ(registry_.find_squad("bridge-squad"), nullptr);
    EXPECT_EQ(registry_.find_squad("nonexistent"), nullptr);
}

// =============================================================================
// FleetRegistry — filter by tag search path (lines 105-110)
// Uses a temporary JSON registry with tags to hit the tag filter branch
// =============================================================================

class FleetRegistryTagTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory with a registry.json that has tagged agents
        tmp_dir_ = QDir::tempPath() + "/euxis-etx-test-tags";
        QDir().mkpath(tmp_dir_ + "/agents");

        QFile file(tmp_dir_ + "/agents/registry.json");
        if (file.open(QIODevice::WriteOnly)) {
            QByteArray json_data = R"({
                "agents": [
                    {
                        "id": "tagged-agent",
                        "tier": "core",
                        "tags": ["security", "audit"],
                        "capability_tags": ["scan", "verify"]
                    },
                    {
                        "id": "untagged-agent",
                        "tier": "fleet"
                    }
                ]
            })";
            file.write(json_data);
            file.close();
        }

        registry_ = new FleetRegistry(tmp_dir_);
    }

    void TearDown() override {
        delete registry_;
        // Clean up temp files
        QDir(tmp_dir_).removeRecursively();
    }

    QString tmp_dir_;
    FleetRegistry* registry_{nullptr};
};

TEST_F(FleetRegistryTagTest, FilterByTagFindsTaggedAgent) {
    auto results = registry_->filter_by_tag("security");
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].id, "tagged-agent");
}

TEST_F(FleetRegistryTagTest, FilterByTagNoMatch) {
    auto results = registry_->filter_by_tag("nonexistent-tag");
    EXPECT_TRUE(results.empty());
}

TEST_F(FleetRegistryTagTest, FilterByQueryMatchesTag) {
    // filter() should also match against tags (lines 105-110)
    auto results = registry_->filter("security");
    EXPECT_FALSE(results.empty());
    bool found_tagged = false;
    for (const auto& a : results) {
        if (a.id == "tagged-agent") found_tagged = true;
    }
    EXPECT_TRUE(found_tagged);
}

TEST_F(FleetRegistryTagTest, AgentsHaveCapabilityTags) {
    const auto* agent = registry_->find("tagged-agent");
    ASSERT_NE(agent, nullptr);
    EXPECT_TRUE(agent->capability_tags.contains("scan"));
    EXPECT_TRUE(agent->capability_tags.contains("verify"));
}

// =============================================================================
// FleetRegistry — load error handling (lines 206-208, 253-255, 281-283)
// Bad JSON should be caught and not crash
// =============================================================================

class FleetRegistryBadJsonTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmp_dir_ = QDir::tempPath() + "/euxis-etx-test-badjson";
        QDir().mkpath(tmp_dir_ + "/agents");
        QDir().mkpath(tmp_dir_ + "/config/playbooks");
    }

    void TearDown() override {
        QDir(tmp_dir_).removeRecursively();
    }

    QString tmp_dir_;
};

TEST_F(FleetRegistryBadJsonTest, MalformedAgentRegistryFallsBack) {
    QFile file(tmp_dir_ + "/agents/registry.json");
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("{ this is not valid JSON }}}");
    file.close();

    FleetRegistry reg(tmp_dir_);
    // Should fall back to defaults
    EXPECT_EQ(reg.agent_count(), 8);
}

TEST_F(FleetRegistryBadJsonTest, MalformedSquadsJsonDoesNotCrash) {
    // Write valid agents but bad squads
    {
        QFile file(tmp_dir_ + "/agents/registry.json");
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(R"({"agents": [{"id": "test", "tier": "core"}]})");
        file.close();
    }
    {
        QFile file(tmp_dir_ + "/agents/squads.json");
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write("not json at all");
        file.close();
    }

    FleetRegistry reg(tmp_dir_);
    EXPECT_EQ(reg.agent_count(), 1);
    EXPECT_EQ(reg.squad_count(), 0);
}

TEST_F(FleetRegistryBadJsonTest, MalformedPlaybookJsonSkipped) {
    // Write valid agents
    {
        QFile file(tmp_dir_ + "/agents/registry.json");
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(R"({"agents": [{"id": "test", "tier": "core"}]})");
        file.close();
    }
    // Write a bad playbook file
    {
        QFile file(tmp_dir_ + "/config/playbooks/bad.json");
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write("{{{invalid json");
        file.close();
    }

    FleetRegistry reg(tmp_dir_);
    EXPECT_EQ(reg.agent_count(), 1);
    EXPECT_EQ(static_cast<int>(reg.playbooks().size()), 0);
}

// =============================================================================
// FleetRegistry — load_squads with valid data (squads + combos)
// =============================================================================

class FleetRegistrySquadTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmp_dir_ = QDir::tempPath() + "/euxis-etx-test-squads";
        QDir().mkpath(tmp_dir_ + "/agents");

        // Write valid agent registry
        {
            QFile file(tmp_dir_ + "/agents/registry.json");
            ASSERT_TRUE(file.open(QIODevice::WriteOnly));
            file.write(R"({"agents": [{"id": "agent-a", "tier": "core"}, {"id": "agent-b", "tier": "fleet"}]})");
            file.close();
        }

        // Write squads and combos
        {
            QFile file(tmp_dir_ + "/agents/squads.json");
            ASSERT_TRUE(file.open(QIODevice::WriteOnly));
            file.write(R"({
                "squads": [
                    {"id": "test-squad", "name": "Test Squad", "purpose": "Testing", "lead": "agent-a", "members": ["agent-a", "agent-b"]}
                ],
                "combos": [
                    {"id": "test-combo", "name": "Test Combo", "description": "A combo", "chain": ["agent-a", "agent-b"]}
                ]
            })");
            file.close();
        }

        registry_ = new FleetRegistry(tmp_dir_);
    }

    void TearDown() override {
        delete registry_;
        QDir(tmp_dir_).removeRecursively();
    }

    QString tmp_dir_;
    FleetRegistry* registry_{nullptr};
};

TEST_F(FleetRegistrySquadTest, FindSquadReturnsSquad) {
    const auto* squad = registry_->find_squad("test-squad");
    ASSERT_NE(squad, nullptr);
    EXPECT_EQ(squad->name, "Test Squad");
    EXPECT_EQ(squad->lead, "agent-a");
    EXPECT_EQ(squad->members.size(), 2);
}

TEST_F(FleetRegistrySquadTest, FindSquadNonExistentReturnsNull) {
    EXPECT_EQ(registry_->find_squad("nonexistent"), nullptr);
}

TEST_F(FleetRegistrySquadTest, SquadCountCorrect) {
    EXPECT_EQ(registry_->squad_count(), 1);
}

TEST_F(FleetRegistrySquadTest, ComboCountCorrect) {
    EXPECT_EQ(registry_->combo_count(), 1);
    EXPECT_FALSE(registry_->combos().empty());
    EXPECT_EQ(registry_->combos()[0].id, "test-combo");
    EXPECT_EQ(registry_->combos()[0].chain.size(), 2);
}

// =============================================================================
// FleetRegistry — load_playbooks with valid data
// =============================================================================

class FleetRegistryPlaybookTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmp_dir_ = QDir::tempPath() + "/euxis-etx-test-playbooks";
        QDir().mkpath(tmp_dir_ + "/agents");
        QDir().mkpath(tmp_dir_ + "/config/playbooks");

        // Write valid agent registry
        {
            QFile file(tmp_dir_ + "/agents/registry.json");
            ASSERT_TRUE(file.open(QIODevice::WriteOnly));
            file.write(R"({"agents": [{"id": "agent-a", "tier": "core"}]})");
            file.close();
        }

        // Write a valid playbook
        {
            QFile file(tmp_dir_ + "/config/playbooks/deploy.json");
            ASSERT_TRUE(file.open(QIODevice::WriteOnly));
            file.write(R"({
                "id": "deploy",
                "name": "Deploy Playbook",
                "description": "A deploy playbook",
                "phases": [{"name": "build"}, {"name": "test"}, {"name": "deploy"}]
            })");
            file.close();
        }

        registry_ = new FleetRegistry(tmp_dir_);
    }

    void TearDown() override {
        delete registry_;
        QDir(tmp_dir_).removeRecursively();
    }

    QString tmp_dir_;
    FleetRegistry* registry_{nullptr};
};

TEST_F(FleetRegistryPlaybookTest, PlaybooksLoaded) {
    EXPECT_EQ(static_cast<int>(registry_->playbooks().size()), 1);
    EXPECT_EQ(registry_->playbooks()[0].id, "deploy");
    EXPECT_EQ(registry_->playbooks()[0].name, "Deploy Playbook");
    EXPECT_EQ(registry_->playbooks()[0].phase_count, 3);
}

// =============================================================================
// FleetRegistry — read_agent_role coverage (lines 14-36)
// =============================================================================

class FleetRegistryRoleTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmp_dir_ = QDir::tempPath() + "/euxis-etx-test-role";
        QDir().mkpath(tmp_dir_ + "/agents/prompts");

        // Create a prompt file with YAML frontmatter containing role
        {
            QFile file(tmp_dir_ + "/agents/prompts/agent-with-role.md");
            ASSERT_TRUE(file.open(QIODevice::WriteOnly));
            file.write("---\nrole: Expert Code Reviewer\ntier: core\n---\n\nYou are an expert...\n");
            file.close();
        }

        // Create a prompt file without role
        {
            QFile file(tmp_dir_ + "/agents/prompts/agent-no-role.md");
            ASSERT_TRUE(file.open(QIODevice::WriteOnly));
            file.write("---\ntier: core\n---\n\nNo role here.\n");
            file.close();
        }

        // Registry JSON referencing these paths
        {
            QFile file(tmp_dir_ + "/agents/registry.json");
            ASSERT_TRUE(file.open(QIODevice::WriteOnly));
            file.write(R"({
                "agents": [
                    {"id": "agent-with-role", "tier": "core", "path": "agents/prompts/agent-with-role.md"},
                    {"id": "agent-no-role", "tier": "core", "path": "agents/prompts/agent-no-role.md"},
                    {"id": "agent-bad-path", "tier": "core", "path": "agents/prompts/nonexistent.md"}
                ]
            })");
            file.close();
        }

        registry_ = new FleetRegistry(tmp_dir_);
    }

    void TearDown() override {
        delete registry_;
        QDir(tmp_dir_).removeRecursively();
    }

    QString tmp_dir_;
    FleetRegistry* registry_{nullptr};
};

TEST_F(FleetRegistryRoleTest, AgentWithRoleHasDescription) {
    const auto* agent = registry_->find("agent-with-role");
    ASSERT_NE(agent, nullptr);
    EXPECT_EQ(agent->description, "Expert Code Reviewer");
}

TEST_F(FleetRegistryRoleTest, AgentNoRoleUsesNameAsDescription) {
    const auto* agent = registry_->find("agent-no-role");
    ASSERT_NE(agent, nullptr);
    // No role found, should use title-cased name
    EXPECT_EQ(agent->description, "Agent No Role");
}

TEST_F(FleetRegistryRoleTest, AgentBadPathUsesNameAsDescription) {
    const auto* agent = registry_->find("agent-bad-path");
    ASSERT_NE(agent, nullptr);
    // File doesn't exist, should use title-cased name
    EXPECT_EQ(agent->description, "Agent Bad Path");
}

} // namespace
} // namespace euxis::etx
