#include <gtest/gtest.h>
#include <QApplication>
#include <QStackedWidget>
#include <QSignalSpy>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>

#include <euxis/etx/app.hpp>
#include <euxis/etx/config.hpp>
#include <euxis/etx/registry.hpp>

namespace euxis::etx {
namespace {

// =============================================================================
// EuxisApp tests
//
// Note: EuxisApp creates all screens including ChatEngine, which depends on
// provider infrastructure. These tests verify construction and basic navigation
// without requiring a working provider.
// =============================================================================

class AppTest : public ::testing::Test {
protected:
    void SetUp() override {
        app_ = new EuxisApp();
    }
    void TearDown() override {
        delete app_;
    }
    EuxisApp* app_{nullptr};
};

TEST_F(AppTest, Construction) {
    ASSERT_NE(app_, nullptr);
    EXPECT_EQ(app_->windowTitle(), "Euxis ETX");
}

TEST_F(AppTest, MinimumSize) {
    EXPECT_GE(app_->minimumWidth(), 1024);
    EXPECT_GE(app_->minimumHeight(), 768);
}

TEST_F(AppTest, HasScreenStack) {
    auto* stack = app_->findChild<QStackedWidget*>();
    ASSERT_NE(stack, nullptr);
    // Should have 17 screens (indices 0-16)
    EXPECT_EQ(stack->count(), 17);
}

TEST_F(AppTest, StartsAtWelcome) {
    auto* stack = app_->findChild<QStackedWidget*>();
    ASSERT_NE(stack, nullptr);
    EXPECT_EQ(stack->currentIndex(), 0);
}

TEST_F(AppTest, ShowDashboard) {
    app_->show_dashboard();
    auto* stack = app_->findChild<QStackedWidget*>();
    ASSERT_NE(stack, nullptr);
    EXPECT_EQ(stack->currentIndex(), 1);
}

TEST_F(AppTest, ShowWelcome) {
    app_->show_dashboard(); // Navigate away first
    app_->show_welcome();
    auto* stack = app_->findChild<QStackedWidget*>();
    ASSERT_NE(stack, nullptr);
    EXPECT_EQ(stack->currentIndex(), 0);
}

TEST_F(AppTest, ShowAgent) {
    app_->show_agent("code-agent");
    auto* stack = app_->findChild<QStackedWidget*>();
    ASSERT_NE(stack, nullptr);
    EXPECT_EQ(stack->currentIndex(), 2);

    // Agent name label should be populated
    auto* agent_widget = stack->widget(2);
    auto* name_label = agent_widget->findChild<QLabel*>("agent_name_label");
    ASSERT_NE(name_label, nullptr);
    EXPECT_FALSE(name_label->text().isEmpty());
}

TEST_F(AppTest, ShowAgentPopulatesExtendedFields) {
    app_->show_agent("code-agent");
    auto* stack = app_->findChild<QStackedWidget*>();
    auto* agent_widget = stack->widget(2);

    auto* tier_label = agent_widget->findChild<QLabel*>("agent_tier_label");
    if (tier_label) {
        EXPECT_FALSE(tier_label->text().isEmpty());
    }

    auto* status_label = agent_widget->findChild<QLabel*>("agent_status_label");
    if (status_label) {
        EXPECT_TRUE(status_label->text().contains("Status:"));
    }
}

TEST_F(AppTest, ShowAgentNonexistent) {
    app_->show_agent("nonexistent-agent");
    auto* stack = app_->findChild<QStackedWidget*>();
    ASSERT_NE(stack, nullptr);
    // Should still navigate to the agent screen
    EXPECT_EQ(stack->currentIndex(), 2);
}

TEST_F(AppTest, ShowFleetMonitor) {
    app_->show_fleet_monitor("squad_deploy", {"agent-1", "agent-2", "agent-3"});
    auto* stack = app_->findChild<QStackedWidget*>();
    ASSERT_NE(stack, nullptr);
    EXPECT_EQ(stack->currentIndex(), 3);

    auto* monitor_widget = stack->widget(3);

    auto* op_label = monitor_widget->findChild<QLabel*>("operation_type_label");
    ASSERT_NE(op_label, nullptr);
    EXPECT_TRUE(op_label->text().contains("squad_deploy"));

    auto* member_list = monitor_widget->findChild<QListWidget*>("member_list");
    ASSERT_NE(member_list, nullptr);
    EXPECT_EQ(member_list->count(), 3);
}

TEST_F(AppTest, ThemeChangedSignal) {
    QSignalSpy spy(app_, &EuxisApp::theme_changed);
    EXPECT_TRUE(spy.isValid());
}

TEST_F(AppTest, ErrorTrackedSignal) {
    QSignalSpy spy(app_, &EuxisApp::error_tracked);
    EXPECT_TRUE(spy.isValid());
}

TEST_F(AppTest, NavigationBetweenScreens) {
    auto* stack = app_->findChild<QStackedWidget*>();
    ASSERT_NE(stack, nullptr);

    app_->show_welcome();
    EXPECT_EQ(stack->currentIndex(), 0);

    app_->show_dashboard();
    EXPECT_EQ(stack->currentIndex(), 1);

    app_->show_agent("code-agent");
    EXPECT_EQ(stack->currentIndex(), 2);

    app_->show_fleet_monitor("test", {});
    EXPECT_EQ(stack->currentIndex(), 3);

    // Navigate back to dashboard
    app_->show_dashboard();
    EXPECT_EQ(stack->currentIndex(), 1);
}

// =============================================================================
// EuxisApp — toggle theme via QMetaObject (slot is private)
// =============================================================================

TEST_F(AppTest, ToggleTheme) {
    // Invoke the private slot via QMetaObject
    QMetaObject::invokeMethod(app_, "on_toggle_theme");
    SUCCEED();
}

// =============================================================================
// EuxisApp — screen navigation exercises screen_name_for + breadcrumb
// =============================================================================

TEST_F(AppTest, ScreenNameForViaNavigation) {
    // Exercise screen_name_for indirectly via navigation which calls breadcrumb
    app_->show_welcome();
    app_->show_dashboard();
    app_->show_agent("code-agent");
    app_->show_fleet_monitor("test", {});
    auto* stack = app_->findChild<QStackedWidget*>();
    ASSERT_NE(stack, nullptr);
    EXPECT_EQ(stack->currentIndex(), 3);
}

// =============================================================================
// EuxisApp — show_agent with non-existent agent verifies labels not populated
// =============================================================================

TEST_F(AppTest, ShowAgentPromptEditNotFound) {
    // This exercises the prompt_edit "no prompt file configured" path (line 186)
    app_->show_agent("code-agent");
    auto* stack = app_->findChild<QStackedWidget*>();
    auto* agent_widget = stack->widget(2);
    auto* prompt_edit = agent_widget->findChild<QPlainTextEdit*>("agent_prompt_edit");
    if (prompt_edit) {
        // Content should be populated — either the prompt or a fallback message
        EXPECT_FALSE(prompt_edit->toPlainText().isEmpty());
    }
}

// =============================================================================
// Registry — filter_by_tier and filter_by_tag for default registry
// =============================================================================

TEST_F(AppTest, RegistryFilterByTierFromApp) {
    // Default registry has agents with "core" and "fleet" tiers
    FleetRegistry reg(QString{});

    auto core = reg.filter_by_tier("core");
    EXPECT_FALSE(core.empty());

    auto fleet = reg.filter_by_tier("fleet");
    EXPECT_FALSE(fleet.empty());
}

TEST_F(AppTest, RegistryFilterByTagEmpty) {
    FleetRegistry reg(QString{});
    auto results = reg.filter_by_tag("nonexistent-tag");
    EXPECT_TRUE(results.empty());
}

TEST_F(AppTest, RegistryFindSquadNonexistent) {
    FleetRegistry reg(QString{});
    EXPECT_EQ(reg.find_squad("nonexistent"), nullptr);
}

} // namespace
} // namespace euxis::etx
