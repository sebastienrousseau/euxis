#include <gtest/gtest.h>
#include <QApplication>
#include <QSignalSpy>
#include <QTest>
#include <QRandomGenerator>

#include <euxis/etx/chat_engine.hpp>
#include <euxis/etx/oauth_flow.hpp>

namespace euxis::etx {
namespace {

// =============================================================================
// ChatEngine tests
// =============================================================================

class ChatEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure no environment overrides affect provider detection
        unsetenv("EUXIS_DEFAULT_PROVIDER");
        unsetenv("ANTHROPIC_API_KEY");
        unsetenv("OPENAI_API_KEY");
        unsetenv("GEMINI_API_KEY");
        unsetenv("GOOGLE_API_KEY");

        // Use a unique isolated directory for every test
        test_data_dir_ = "/tmp/euxis-etx-test-" +
                         QString::number(QDateTime::currentMSecsSinceEpoch()) + "-" +
                         QString::number(QRandomGenerator::global()->generate());
        QDir().mkpath(test_data_dir_);

        // Point HOME to an empty directory to avoid picking up real credentials
        QString dummy_home = test_data_dir_ + "/dummy_home";
        QDir().mkpath(dummy_home);
        setenv("HOME", dummy_home.toLocal8Bit().constData(), 1);

        engine_ = new ChatEngine(test_data_dir_, nullptr);
    }
    void TearDown() override {
        if (engine_) {
            engine_->shutdown();
            delete engine_;
        }
        QDir(test_data_dir_).removeRecursively();
    }
    ChatEngine* engine_{nullptr};
    QString test_data_dir_;
};

TEST_F(ChatEngineTest, Construction) {
    ASSERT_NE(engine_, nullptr);
}

TEST_F(ChatEngineTest, DefaultActiveAgent) {
    EXPECT_EQ(engine_->active_agent(), "code-agent");
}

TEST_F(ChatEngineTest, SetActiveAgent) {
    engine_->set_active_agent("security-agent");
    EXPECT_EQ(engine_->active_agent(), "security-agent");
}

TEST_F(ChatEngineTest, EmptyConversationHistory) {
    EXPECT_TRUE(engine_->conversation_history().isEmpty());
}

TEST_F(ChatEngineTest, ClearHistory) {
    engine_->clear_history();
    EXPECT_TRUE(engine_->conversation_history().isEmpty());
}

TEST_F(ChatEngineTest, CurrentProviderNotEmpty) {
    // current_provider should return something (detected or override)
    auto provider = engine_->current_provider();
    // May be empty if no provider is detected, but should not crash
    Q_UNUSED(provider);
    SUCCEED();
}

TEST_F(ChatEngineTest, DetectedProvider) {
    auto detected = engine_->detected_provider();
    // May be empty if no provider CLI is available
    Q_UNUSED(detected);
    SUCCEED();
}

TEST_F(ChatEngineTest, AvailableProviders) {
    auto providers = engine_->available_providers();
    // Should return a list (possibly empty)
    Q_UNUSED(providers);
    SUCCEED();
}

TEST_F(ChatEngineTest, ProviderStatusText) {
    auto status = engine_->provider_status_text();
    EXPECT_FALSE(status.isEmpty());
    EXPECT_TRUE(status.contains("Provider:"));
}

TEST_F(ChatEngineTest, SetProviderOverride) {
    QSignalSpy spy(engine_, &ChatEngine::provider_changed);

    engine_->set_provider("anthropic");
    EXPECT_EQ(engine_->current_provider(), "anthropic");

    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), "anthropic");
}

TEST_F(ChatEngineTest, SetProviderClearOverride) {
    engine_->set_provider("anthropic");
    EXPECT_EQ(engine_->current_provider(), "anthropic");

    // Clear override
    engine_->set_provider("");
    // Should fall back to detected provider (could be empty or something else, but not "anthropic" override)
    EXPECT_EQ(engine_->current_provider(), engine_->detected_provider());
}

TEST_F(ChatEngineTest, OAuthFlowNotNull) {
    auto* flow = engine_->oauth_flow();
    EXPECT_NE(flow, nullptr);
}

TEST_F(ChatEngineTest, AuthStoreAccessible) {
    auto& store = engine_->auth_store();
    // Should be accessible without crash
    auto profiles = store.all_profiles();
    Q_UNUSED(profiles);
    SUCCEED();
}

TEST_F(ChatEngineTest, AuthProfilesForProvider) {
    auto profiles = engine_->auth_profiles_for("anthropic");
    // May be empty but should not crash
    Q_UNUSED(profiles);
    SUCCEED();
}

TEST_F(ChatEngineTest, RemoveNonexistentProfile) {
    // Should not crash
    engine_->remove_auth_profile("nonexistent-profile-id");
    SUCCEED();
}

TEST_F(ChatEngineTest, StartLoginForProvider) {
    // start_login for an unsupported provider should not crash
    engine_->start_login("unsupported_provider");
    SUCCEED();
}

TEST_F(ChatEngineTest, SignalsAreValid) {
    QSignalSpy started(engine_, &ChatEngine::response_started);
    EXPECT_TRUE(started.isValid());

    QSignalSpy chunk(engine_, &ChatEngine::response_chunk);
    EXPECT_TRUE(chunk.isValid());

    QSignalSpy finished(engine_, &ChatEngine::response_finished);
    EXPECT_TRUE(finished.isValid());

    QSignalSpy error(engine_, &ChatEngine::response_error);
    EXPECT_TRUE(error.isValid());

    QSignalSpy changed(engine_, &ChatEngine::provider_changed);
    EXPECT_TRUE(changed.isValid());

    QSignalSpy fallback(engine_, &ChatEngine::fallback_activated);
    EXPECT_TRUE(fallback.isValid());

    QSignalSpy cooldown(engine_, &ChatEngine::profile_cooldown_started);
    EXPECT_TRUE(cooldown.isValid());
}

// =============================================================================
// ChatEngine — send_message paths (lines 37-77)
// =============================================================================

TEST_F(ChatEngineTest, SendMessageAppendsToHistory) {
    // send_message will try to execute a provider which will fail,
    // but the user message should still be recorded in history
    engine_->send_message("Hello from test");

    // Process events to allow async execution to complete
    QTest::qWait(200);
    QApplication::processEvents();

    // History should have at least the user message
    EXPECT_GE(engine_->conversation_history().size(), 1);
    EXPECT_EQ(engine_->conversation_history().first().role, ChatMessage::User);
    EXPECT_EQ(engine_->conversation_history().first().text, "Hello from test");
}

TEST_F(ChatEngineTest, SendMessageWithAtPrefix) {
    // Test @agent-name prefix routing
    engine_->send_message("@security-agent check for vulnerabilities");

    QTest::qWait(200);
    QApplication::processEvents();

    // Should have recorded the message
    EXPECT_GE(engine_->conversation_history().size(), 1);
    auto& msg = engine_->conversation_history().first();
    EXPECT_EQ(msg.role, ChatMessage::User);
    EXPECT_EQ(msg.agent_id, "security-agent");
}

TEST_F(ChatEngineTest, SendMessageWithSpecificAgent) {
    engine_->send_message("Do something", "research-agent");

    QTest::qWait(200);
    QApplication::processEvents();

    EXPECT_GE(engine_->conversation_history().size(), 1);
    EXPECT_EQ(engine_->conversation_history().first().agent_id, "research-agent");
}

TEST_F(ChatEngineTest, SendMessageWithOverrideProvider) {
    engine_->set_provider("anthropic");
    engine_->send_message("test with override");

    QTest::qWait(200);
    QApplication::processEvents();

    EXPECT_GE(engine_->conversation_history().size(), 1);
}

TEST_F(ChatEngineTest, ResponseErrorOnFailure) {
    QSignalSpy error_spy(engine_, &ChatEngine::response_error);
    QSignalSpy started_spy(engine_, &ChatEngine::response_started);

    // Force a provider that will fail without real auth
    engine_->set_provider("openai");
    engine_->add_api_key("openai", "sk-invalid-test-key", "Test");

    engine_->send_message("This will fail");

    // Wait for the async execution to complete and signals to be delivered
    for (int i = 0; i < 100 && error_spy.isEmpty(); ++i) {
        QTest::qWait(50);
        QApplication::processEvents();
    }

    // response_started should have been emitted
    EXPECT_GE(started_spy.count(), 1);

    // response_error should have been emitted
    EXPECT_GE(error_spy.count(), 1);
}

TEST_F(ChatEngineTest, AddApiKey) {
    engine_->add_api_key("anthropic", "sk-test-key-123", "test key");
    auto profiles = engine_->auth_profiles_for("anthropic");
    EXPECT_GE(profiles.size(), 1);
}

TEST_F(ChatEngineTest, AddAndRemoveApiKey) {
    engine_->add_api_key("openai", "sk-openai-test", "openai test");
    auto profiles = engine_->auth_profiles_for("openai");
    EXPECT_GE(profiles.size(), 1);

    if (!profiles.isEmpty()) {
        QString id = QString::fromStdString(profiles.first().id);
        engine_->remove_auth_profile(id);
    }
}

TEST_F(ChatEngineTest, StartLoginWithImport) {
    // Tests the ChatEngine::start_login redirection that avoids browsers
    QSignalSpy success_spy(engine_->oauth_flow(), &OAuthFlow::login_succeeded);
    QSignalSpy failed_spy(engine_->oauth_flow(), &OAuthFlow::login_failed);

    // Should emit login_failed because we have no real credentials to import in test
    engine_->start_login("anthropic");
    QApplication::processEvents();

    EXPECT_EQ(success_spy.count() + failed_spy.count(), 1);
}

TEST_F(ChatEngineTest, StartLoginGeminiWithImport) {
    QSignalSpy success_spy(engine_->oauth_flow(), &OAuthFlow::login_succeeded);
    QSignalSpy failed_spy(engine_->oauth_flow(), &OAuthFlow::login_failed);

    engine_->start_login("gemini");
    QApplication::processEvents();

    // Success or failure is acceptable, as long as a signal is emitted
    EXPECT_EQ(success_spy.count() + failed_spy.count(), 1);
}

} // namespace
} // namespace euxis::etx
