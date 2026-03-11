#include <gtest/gtest.h>
#include <QApplication>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkAccessManager>
#include <QTest>

#include <euxis/etx/oauth_flow.hpp>

namespace euxis::etx {
namespace {

// =============================================================================
// OAuthFlow tests
// =============================================================================

class OAuthFlowTest : public ::testing::Test {
protected:
    void SetUp() override {
        // DO NOT set client IDs here, to avoid triggering real browser flows in tests.
        // ChatEngine handles the real "start_login" by redirecting to local imports.
        setenv("EUXIS_TEST_SKIP_BROWSER", "1", 1);
        flow_ = new OAuthFlow();
    }
    void TearDown() override {
        delete flow_;
        unsetenv("EUXIS_TEST_SKIP_BROWSER");
    }
    OAuthFlow* flow_{nullptr};
};

TEST_F(OAuthFlowTest, Construction) {
    ASSERT_NE(flow_, nullptr);
}

TEST_F(OAuthFlowTest, CancelWithoutStart) {
    // Cancel should be safe even without starting
    flow_->cancel();
    SUCCEED();
}

TEST_F(OAuthFlowTest, LoginFailedSignalForUnsupportedProvider) {
    QSignalSpy failed_spy(flow_, &OAuthFlow::login_failed);

    flow_->start_login("unsupported_provider");

    ASSERT_EQ(failed_spy.count(), 1);
    EXPECT_EQ(failed_spy.at(0).at(0).toString(), "unsupported_provider");
    EXPECT_TRUE(failed_spy.at(0).at(1).toString().contains("Unsupported"));
}

TEST_F(OAuthFlowTest, LoginSucceededSignalIsValid) {
    QSignalSpy spy(flow_, &OAuthFlow::login_succeeded);
    EXPECT_TRUE(spy.isValid());
}

TEST_F(OAuthFlowTest, LoginFailedSignalIsValid) {
    QSignalSpy spy(flow_, &OAuthFlow::login_failed);
    EXPECT_TRUE(spy.isValid());
}

TEST_F(OAuthFlowTest, AnthropicWithoutClientIdFails) {
    // Without EUXIS_ANTHROPIC_CLIENT_ID env, start_login emits login_failed
    QSignalSpy failed_spy(flow_, &OAuthFlow::login_failed);
    flow_->start_login("anthropic");

    ASSERT_EQ(failed_spy.count(), 1);
    EXPECT_TRUE(failed_spy.at(0).at(1).toString().contains("Unsupported"));
}

TEST_F(OAuthFlowTest, GeminiWithoutClientIdFails) {
    QSignalSpy failed_spy(flow_, &OAuthFlow::login_failed);
    flow_->start_login("gemini");

    ASSERT_EQ(failed_spy.count(), 1);
    EXPECT_TRUE(failed_spy.at(0).at(1).toString().contains("Unsupported"));
}

TEST_F(OAuthFlowTest, GoogleWithoutClientIdFails) {
    QSignalSpy failed_spy(flow_, &OAuthFlow::login_failed);
    flow_->start_login("google");

    ASSERT_EQ(failed_spy.count(), 1);
    EXPECT_TRUE(failed_spy.at(0).at(1).toString().contains("Unsupported"));
}

TEST_F(OAuthFlowTest, CancelIsIdempotent) {
    flow_->cancel();
    flow_->cancel();
    SUCCEED();
}

TEST_F(OAuthFlowTest, EmptyProviderFails) {
    QSignalSpy failed_spy(flow_, &OAuthFlow::login_failed);

    flow_->start_login("");

    ASSERT_EQ(failed_spy.count(), 1);
    EXPECT_TRUE(failed_spy.at(0).at(1).toString().contains("Unsupported"));
}

} // namespace
} // namespace euxis::etx
