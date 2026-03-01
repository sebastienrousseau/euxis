#include <gtest/gtest.h>
#include "euxis/adapters/registry.hpp"

namespace euxis::adapters {
namespace {

TEST(RegistryTest, EmptyConfigReturnsNoAdapters) {
    auto adapters = build_adapters({});
    EXPECT_TRUE(adapters.empty());
}

TEST(RegistryTest, EnabledSlackCreatesAdapter) {
    nlohmann::json config = {
        {"gateway", {{"channels", {{"slack", {
            {"enabled", true}, {"token", "t"}, {"app_token", "a"}
        }}}}}},
    };
    auto adapters = build_adapters(config);
    EXPECT_EQ(adapters.count("slack"), 1);
}

TEST(RegistryTest, DisabledAdapterNotCreated) {
    nlohmann::json config = {
        {"gateway", {{"channels", {{"slack", {{"enabled", false}}}}}}},
    };
    auto adapters = build_adapters(config);
    EXPECT_EQ(adapters.count("slack"), 0);
}

} // namespace
} // namespace euxis::adapters
