/// @file
/// @brief Tests for failover taxonomy, credential pool, and jittered backoff.

#include <gtest/gtest.h>

#include <atomic>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "euxis/core/credential_pool.hpp"

// NOLINTBEGIN(bugprone-unchecked-optional-access) — gtest ASSERT_TRUE
// guards are invisible to clang-tidy's dataflow; tests can blanket-
// disable per docs/development/clang-tidy-policy.md.

namespace euxis::core {
namespace {

// ---------------------------------------------------------------------------
// classify_failure
// ---------------------------------------------------------------------------

TEST(ClassifyFailureTest, AuthIsClassified) {
    EXPECT_EQ(classify_failure(401, "unauthorized"),    FailoverReason::Auth);
    EXPECT_EQ(classify_failure(401, ""),                FailoverReason::Auth);
}

TEST(ClassifyFailureTest, BillingIsClassified) {
    EXPECT_EQ(classify_failure(402, "quota exhausted"), FailoverReason::Billing);
}

TEST(ClassifyFailureTest, RateLimitIsClassified) {
    EXPECT_EQ(classify_failure(429, "slow down"),       FailoverReason::RateLimit);
}

TEST(ClassifyFailureTest, FourHundredContextOverflowDetected) {
    EXPECT_EQ(classify_failure(400, "context length too long"),
              FailoverReason::ContextOverflow);
    EXPECT_EQ(classify_failure(400, "max_tokens exceeded"),
              FailoverReason::ContextOverflow);
    EXPECT_EQ(classify_failure(400, "request too long"),
              FailoverReason::ContextOverflow);
    EXPECT_EQ(classify_failure(400, "TOKEN LIMIT reached"),
              FailoverReason::ContextOverflow);
}

TEST(ClassifyFailureTest, FourHundredModelNotFoundDetected) {
    EXPECT_EQ(classify_failure(400, "model_not_found"),
              FailoverReason::ModelNotFound);
    EXPECT_EQ(classify_failure(400, "the model is deprecated"),
              FailoverReason::ModelNotFound);
    EXPECT_EQ(classify_failure(404, "no such model"),
              FailoverReason::ModelNotFound);
}

TEST(ClassifyFailureTest, FourHundredUnknownFallsBackToUnknown) {
    EXPECT_EQ(classify_failure(400, "bad request"),
              FailoverReason::Unknown);
}

TEST(ClassifyFailureTest, ServerErrorsAreClassified) {
    EXPECT_EQ(classify_failure(500, "internal"), FailoverReason::ServerError);
    EXPECT_EQ(classify_failure(502, "bad gw"),   FailoverReason::ServerError);
    EXPECT_EQ(classify_failure(503, ""),         FailoverReason::ServerError);
}

TEST(ClassifyFailureTest, ZeroStatusIsNetwork) {
    EXPECT_EQ(classify_failure(0, "connection refused"),
              FailoverReason::Network);
}

TEST(ClassifyFailureTest, UnknownStatusIsUnknown) {
    EXPECT_EQ(classify_failure(418, "I'm a teapot"), FailoverReason::Unknown);
}

// ---------------------------------------------------------------------------
// recovery_for
// ---------------------------------------------------------------------------

TEST(RecoveryForTest, MapsEachReasonToAnAction) {
    EXPECT_EQ(recovery_for(FailoverReason::Auth),
              RecoveryAction::RotateCredential);
    EXPECT_EQ(recovery_for(FailoverReason::Billing),
              RecoveryAction::RotateCredential);
    EXPECT_EQ(recovery_for(FailoverReason::RateLimit),
              RecoveryAction::Retry);
    EXPECT_EQ(recovery_for(FailoverReason::ContextOverflow),
              RecoveryAction::CompactContext);
    EXPECT_EQ(recovery_for(FailoverReason::ServerError),
              RecoveryAction::Retry);
    EXPECT_EQ(recovery_for(FailoverReason::ModelNotFound),
              RecoveryAction::SwitchModel);
    EXPECT_EQ(recovery_for(FailoverReason::Network),
              RecoveryAction::Retry);
    EXPECT_EQ(recovery_for(FailoverReason::Unknown),
              RecoveryAction::Abort);
    EXPECT_EQ(recovery_for(FailoverReason::None),
              RecoveryAction::Abort);
}

TEST(ReasonNameTest, StableLowerCaseRendering) {
    EXPECT_EQ(reason_name(FailoverReason::Auth),            "auth");
    EXPECT_EQ(reason_name(FailoverReason::RateLimit),       "rate_limit");
    EXPECT_EQ(reason_name(FailoverReason::ContextOverflow), "context_overflow");
    EXPECT_EQ(reason_name(FailoverReason::ModelNotFound),   "model_not_found");
}

// ---------------------------------------------------------------------------
// CredentialPool
// ---------------------------------------------------------------------------

auto pool_of(std::vector<std::string> ids) -> CredentialPool {
    std::vector<Credential> creds;
    for (auto& id : ids) {
        Credential c;
        c.id = std::move(id);
        c.secret = "secret-for-" + c.id;
        creds.push_back(std::move(c));
    }
    return CredentialPool{std::move(creds)};
}

TEST(CredentialPoolTest, RoundRobinAcrossAllCredentials) {
    auto p = pool_of({"k1", "k2", "k3"});
    std::set<std::string> seen;
    for (int i = 0; i < 9; ++i) {
        auto id = p.next_available(0);
        ASSERT_TRUE(id.has_value());
        seen.insert(*id);
    }
    EXPECT_EQ(seen.size(), 3u);
    EXPECT_TRUE(seen.count("k1"));
    EXPECT_TRUE(seen.count("k2"));
    EXPECT_TRUE(seen.count("k3"));
}

TEST(CredentialPoolTest, SkipsCooledDownCredentials) {
    auto p = pool_of({"k1", "k2"});
    EXPECT_TRUE(p.cool_down("k1", 100, 1000));  // cooled until 1100

    // Query at time=500: k1 still cooled, only k2 may be returned.
    for (int i = 0; i < 10; ++i) {
        auto id = p.next_available(500);
        ASSERT_TRUE(id.has_value());
        EXPECT_EQ(*id, "k2");
    }
}

TEST(CredentialPoolTest, ReturnsNulloptWhenAllCooled) {
    auto p = pool_of({"k1", "k2"});
    EXPECT_TRUE(p.cool_down("k1", 0, 5000));
    EXPECT_TRUE(p.cool_down("k2", 0, 5000));
    auto id = p.next_available(1000);
    EXPECT_FALSE(id.has_value());
}

TEST(CredentialPoolTest, CooldownExpires) {
    auto p = pool_of({"k1"});
    EXPECT_TRUE(p.cool_down("k1", 100, 1000));  // cooled until 1100
    EXPECT_FALSE(p.next_available(500).has_value());
    EXPECT_TRUE(p.next_available(1100).has_value()) << "boundary inclusive";
    EXPECT_TRUE(p.next_available(2000).has_value());
}

TEST(CredentialPoolTest, MarkHealthyClearsCooldown) {
    auto p = pool_of({"k1"});
    EXPECT_TRUE(p.cool_down("k1", 0, 999999));
    EXPECT_FALSE(p.next_available(0).has_value());

    EXPECT_TRUE(p.mark_healthy("k1"));
    EXPECT_TRUE(p.next_available(0).has_value());
}

TEST(CredentialPoolTest, CoolDownUnknownIdReturnsFalse) {
    auto p = pool_of({"k1"});
    EXPECT_FALSE(p.cool_down("does-not-exist", 0, 1000));
    EXPECT_FALSE(p.mark_healthy("nope"));
}

TEST(CredentialPoolTest, AvailableCountReflectsCooldown) {
    auto p = pool_of({"k1", "k2", "k3"});
    EXPECT_EQ(p.size(),                 3u);
    EXPECT_EQ(p.available_count(0),     3u);

    EXPECT_TRUE(p.cool_down("k1", 0, 1000));
    EXPECT_TRUE(p.cool_down("k2", 0, 1000));
    EXPECT_EQ(p.available_count(500),   1u);
    EXPECT_EQ(p.available_count(2000),  3u) << "after cooldown expires";
}

TEST(CredentialPoolTest, EmptyPoolReturnsNullopt) {
    CredentialPool p{{}};
    EXPECT_EQ(p.size(), 0u);
    EXPECT_FALSE(p.next_available(0).has_value());
}

// 8 threads × 1000 attempts on a 3-key pool — must never crash and must
// never return an id that isn't in the pool.
TEST(CredentialPoolTest, ConcurrentNextAvailableIsSafe) {
    auto p = pool_of({"k1", "k2", "k3"});
    constexpr std::size_t kThreads = 8;
    constexpr std::size_t kPerThread = 1000;

    std::atomic<std::size_t> bad{0};
    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (std::size_t t = 0; t < kThreads; ++t) {
        workers.emplace_back([&] {
            for (std::size_t i = 0; i < kPerThread; ++i) {
                auto id = p.next_available(0);
                if (!id.has_value()) {
                    bad.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                if (*id != "k1" && *id != "k2" && *id != "k3") {
                    bad.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& w : workers) w.join();
    EXPECT_EQ(bad.load(), 0u);
}

// ---------------------------------------------------------------------------
// jittered_backoff_ms
// ---------------------------------------------------------------------------

TEST(JitteredBackoffTest, ZeroBaseProducesZero) {
    EXPECT_EQ(jittered_backoff_ms(0, /*base*/ 0, /*cap*/ 1000, /*seed*/ 1), 0);
    EXPECT_EQ(jittered_backoff_ms(5, /*base*/ 0, /*cap*/ 1000, /*seed*/ 1), 0);
}

TEST(JitteredBackoffTest, BoundedByCap) {
    for (std::size_t attempt = 0; attempt < 40; ++attempt) {
        const auto ms = jittered_backoff_ms(attempt, 250, 30000, /*seed*/ 42);
        EXPECT_GE(ms,   0);
        EXPECT_LE(ms,   30000);
    }
}

TEST(JitteredBackoffTest, DeterministicForFixedSeed) {
    const auto a = jittered_backoff_ms(3, 250, 30000, /*seed*/ 12345);
    const auto b = jittered_backoff_ms(3, 250, 30000, /*seed*/ 12345);
    EXPECT_EQ(a, b);

    // Different seed → different value (almost surely).
    const auto c = jittered_backoff_ms(3, 250, 30000, /*seed*/ 54321);
    EXPECT_NE(a, c);
}

TEST(JitteredBackoffTest, DifferentAttemptsProduceDifferentJitter) {
    const auto a0 = jittered_backoff_ms(0, 250, 30000, /*seed*/ 7);
    const auto a1 = jittered_backoff_ms(1, 250, 30000, /*seed*/ 7);
    const auto a2 = jittered_backoff_ms(2, 250, 30000, /*seed*/ 7);
    EXPECT_NE(a0, a1);
    EXPECT_NE(a1, a2);
}

TEST(JitteredBackoffTest, CapBelowBaseClampsToBase) {
    // Caller supplied cap < base — we coerce cap up to base so the
    // schedule is still well-defined.
    const auto ms = jittered_backoff_ms(0, /*base*/ 500, /*cap*/ 100,
                                         /*seed*/ 1);
    EXPECT_GE(ms, 0);
    EXPECT_LE(ms, 500);
}

} // namespace
} // namespace euxis::core

// NOLINTEND(bugprone-unchecked-optional-access)
