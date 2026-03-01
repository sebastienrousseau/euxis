#include <euxis/etx/registry.hpp>

#include <gtest/gtest.h>

namespace euxis::etx {
namespace {

class FleetRegistryTest : public ::testing::Test {
protected:
    FleetRegistry registry_;
};

TEST_F(FleetRegistryTest, LoadDefaultsPopulatesAgents) {
    EXPECT_FALSE(registry_.agents().empty());
    EXPECT_EQ(registry_.agents().size(), 8);
}

TEST_F(FleetRegistryTest, FindExistingAgent) {
    const auto* agent = registry_.find("code-agent");
    ASSERT_NE(agent, nullptr);
    EXPECT_EQ(agent->name, "Code Agent");
    EXPECT_EQ(agent->type, "agent");
}

TEST_F(FleetRegistryTest, FindReturnsNullForMissing) {
    EXPECT_EQ(registry_.find("nonexistent-agent"), nullptr);
}

TEST_F(FleetRegistryTest, FilterByName) {
    auto results = registry_.filter("Code");
    EXPECT_FALSE(results.empty());
    bool found = false;
    for (const auto& a : results) {
        if (a.id == "code-agent") found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(FleetRegistryTest, FilterByDescription) {
    auto results = registry_.filter("WASM");
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0].id, "code-agent");
}

TEST_F(FleetRegistryTest, FilterById) {
    auto results = registry_.filter("security-agent");
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].name, "Security Agent");
}

TEST_F(FleetRegistryTest, FilterCaseInsensitive) {
    auto upper = registry_.filter("CODE");
    auto lower = registry_.filter("code");
    EXPECT_EQ(upper.size(), lower.size());
    EXPECT_FALSE(upper.empty());
}

TEST_F(FleetRegistryTest, EmptyFilterReturnsAll) {
    auto results = registry_.filter("");
    EXPECT_EQ(results.size(), registry_.agents().size());
}

TEST_F(FleetRegistryTest, AgentTypesPresent) {
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

TEST_F(FleetRegistryTest, AgentStatusesPresent) {
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

TEST_F(FleetRegistryTest, RefreshReloadsAgents) {
    size_t before = registry_.agents().size();
    registry_.refresh();
    EXPECT_EQ(registry_.agents().size(), before);
}

} // namespace
} // namespace euxis::etx
