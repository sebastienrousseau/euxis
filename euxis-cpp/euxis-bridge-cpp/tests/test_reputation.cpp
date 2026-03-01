#include <gtest/gtest.h>
#include <euxis/bridge/reputation.hpp>

namespace euxis::bridge {

TEST(ReputationTest, DefaultScore) {
    ReputationStore store;
    EXPECT_DOUBLE_EQ(store.score("unknown-author"), 0.5);
}

TEST(ReputationTest, UpdateScore) {
    ReputationStore store;
    store.update("author-1", 0.8);

    auto rep = store.get("author-1");
    ASSERT_TRUE(rep.has_value());
    EXPECT_DOUBLE_EQ(rep->score, 0.8);
}

TEST(ReputationTest, FlagDecaysScore) {
    ReputationStore store;
    store.update("author-1", 0.8);
    store.flag("author-1");

    auto rep = store.get("author-1");
    ASSERT_TRUE(rep.has_value());
    EXPECT_LT(rep->score, 0.8);
    EXPECT_EQ(rep->skills_flagged, 1u);
}

TEST(ReputationTest, PublishIncrements) {
    ReputationStore store;
    store.publish("author-1");
    store.publish("author-1");

    auto rep = store.get("author-1");
    ASSERT_TRUE(rep.has_value());
    EXPECT_EQ(rep->skills_published, 2u);
}

TEST(ReputationTest, ScoreClamped) {
    ReputationStore store;
    store.update("author-1", 1.5);  // Over 1.0

    auto rep = store.get("author-1");
    ASSERT_TRUE(rep.has_value());
    EXPECT_LE(rep->score, 1.0);

    store.update("author-1", -0.5);  // Under 0.0
    rep = store.get("author-1");
    EXPECT_GE(rep->score, 0.0);
}

TEST(ReputationTest, MultipleFlagsDecay) {
    ReputationStore store;
    store.update("flagged", 0.5);
    for (int i = 0; i < 10; ++i) {
        store.flag("flagged");
    }

    auto rep = store.get("flagged");
    ASSERT_TRUE(rep.has_value());
    EXPECT_DOUBLE_EQ(rep->score, 0.0);  // Bottoms out at 0
    EXPECT_EQ(rep->skills_flagged, 10u);
}

TEST(ReputationTest, UnknownAuthorGet) {
    ReputationStore store;
    EXPECT_EQ(store.get("nobody"), std::nullopt);
}

TEST(ReputationTest, HasTimestamp) {
    ReputationStore store;
    store.update("author-1", 0.9);

    auto rep = store.get("author-1");
    ASSERT_TRUE(rep.has_value());
    EXPECT_FALSE(rep->last_updated.empty());
}

}  // namespace euxis::bridge
