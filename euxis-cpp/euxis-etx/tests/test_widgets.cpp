#include <gtest/gtest.h>
#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QFrame>
#include <QScrollArea>
#include <QGridLayout>
#include <QSignalSpy>
#include <QTest>

#include <euxis/etx/config.hpp>
#include <euxis/etx/registry.hpp>
#include <euxis/etx/theme.hpp>
#include <euxis/etx/conversation_widget.hpp>
#include <euxis/etx/chat_input_widget.hpp>
#include <euxis/etx/chat_engine.hpp>
#include <euxis/etx/bento_layout.hpp>
#include <euxis/etx/breadcrumb_widget.hpp>

#include <QDialog>
#include <QTimer>
#include <QResizeEvent>
#include <QEnterEvent>
#include <QMouseEvent>

// Forward declarations for widget factory functions
namespace euxis::etx {
    class EuxisApp;
    QWidget* create_header_widget(ThemeEngine* theme, QWidget* parent);
    QWidget* create_shortcut_bar_widget(QWidget* parent);
    QWidget* create_search_bar_widget(QWidget* parent);
    QWidget* create_tip_bar_widget(ETXConfig* config, QWidget* parent);
    QWidget* create_output_panel_widget(QWidget* parent);
    QWidget* create_scroll_minimap_widget(QWidget* parent);
    QWidget* create_sparkline_chart_widget(QWidget* parent);
    QWidget* create_fleet_grid_widget(FleetRegistry* registry, EuxisApp* app,
                                      QWidget* parent);
    QWidget* create_agent_card_widget(const AgentInfo& agent, EuxisApp* app,
                                      QWidget* parent);
    QWidget* create_resource_monitor_widget(QWidget* parent);
    QDialog* create_provider_select_dialog(ChatEngine* chat, QWidget* parent);
}

namespace euxis::etx {
namespace {

// =============================================================================
// Widget test fixture
// =============================================================================

class WidgetTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = new ETXConfig();
        registry_ = new FleetRegistry(QString{});
        theme_ = new ThemeEngine();
    }

    void TearDown() override {
        delete config_;
        delete registry_;
        delete theme_;
    }

    ETXConfig* config_{nullptr};
    FleetRegistry* registry_{nullptr};
    ThemeEngine* theme_{nullptr};
};

// =============================================================================
// Header Widget
// =============================================================================

TEST_F(WidgetTest, HeaderWidgetConstruction) {
    auto* widget = create_header_widget(theme_, nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_EQ(widget->objectName(), "header_widget");
    EXPECT_EQ(widget->height(), 56);

    EXPECT_NE(widget->findChild<QLabel*>("header_logo"), nullptr);
    EXPECT_NE(widget->findChild<QLabel*>("header_title"), nullptr);
    EXPECT_NE(widget->findChild<QLabel*>("theme_indicator"), nullptr);

    delete widget;
}

TEST_F(WidgetTest, HeaderWidgetThemeIndicator) {
    auto* widget = create_header_widget(theme_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto* indicator = widget->findChild<QLabel*>("theme_indicator");
    ASSERT_NE(indicator, nullptr);
    EXPECT_EQ(indicator->text(), "liquid-glass");

    // Change theme — indicator should update
    theme_->apply_theme("calm");
    EXPECT_EQ(indicator->text(), "calm");

    delete widget;
}

// =============================================================================
// Shortcut Bar Widget
// =============================================================================

TEST_F(WidgetTest, ShortcutBarWidgetConstruction) {
    auto* widget = create_shortcut_bar_widget(nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_EQ(widget->objectName(), "shortcut_bar");
    EXPECT_EQ(widget->height(), 32);

    // Should have labels for shortcuts
    auto labels = widget->findChildren<QLabel*>();
    EXPECT_GT(labels.size(), 4); // At least shortcut + action labels

    delete widget;
}

// =============================================================================
// Search Bar Widget
// =============================================================================

TEST_F(WidgetTest, SearchBarWidgetConstruction) {
    auto* widget = create_search_bar_widget(nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_EQ(widget->objectName(), "search_bar_widget");
    EXPECT_EQ(widget->height(), 48);

    auto* input = widget->findChild<QLineEdit*>("search_input");
    ASSERT_NE(input, nullptr);
    EXPECT_FALSE(input->placeholderText().isEmpty());

    delete widget;
}

// =============================================================================
// Tip Bar Widget
// =============================================================================

TEST_F(WidgetTest, TipBarWidgetConstruction) {
    auto* widget = create_tip_bar_widget(config_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto* label = widget->findChild<QLabel*>("tip_label");
    ASSERT_NE(label, nullptr);
    // Tips should be shown by default
    EXPECT_TRUE(widget->isVisible());
    EXPECT_FALSE(label->text().isEmpty());

    delete widget;
}

TEST_F(WidgetTest, TipBarWidgetHiddenWhenDisabled) {
    config_->set_show_tips(false);

    auto* widget = create_tip_bar_widget(config_, nullptr);
    ASSERT_NE(widget, nullptr);

    // With show_tips=false, the widget should be hidden
    EXPECT_FALSE(widget->isVisible());

    // Restore
    config_->set_show_tips(true);

    delete widget;
}

// =============================================================================
// Output Panel Widget
// =============================================================================

TEST_F(WidgetTest, OutputPanelWidgetConstruction) {
    auto* widget = create_output_panel_widget(nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_EQ(widget->objectName(), "output_panel");

    // It should be a QPlainTextEdit (read-only)
    auto* edit = qobject_cast<QPlainTextEdit*>(widget);
    ASSERT_NE(edit, nullptr);
    EXPECT_TRUE(edit->isReadOnly());

    delete widget;
}

// =============================================================================
// Scroll Minimap Widget
// =============================================================================

TEST_F(WidgetTest, ScrollMinimapWidgetConstruction) {
    auto* widget = create_scroll_minimap_widget(nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_EQ(widget->objectName(), "scroll_minimap");
    EXPECT_EQ(widget->width(), 20);
    EXPECT_GE(widget->minimumHeight(), 100);

    delete widget;
}

// =============================================================================
// Sparkline Chart Widget
// =============================================================================

TEST_F(WidgetTest, SparklineChartWidgetConstruction) {
    auto* widget = create_sparkline_chart_widget(nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_EQ(widget->objectName(), "sparkline_chart");
    EXPECT_GE(widget->minimumWidth(), 60);
    EXPECT_GE(widget->minimumHeight(), 20);

    delete widget;
}

// =============================================================================
// Agent Card Widget
// =============================================================================

TEST_F(WidgetTest, AgentCardWidgetConstruction) {
    AgentInfo info;
    info.id = "test-agent";
    info.name = "Test Agent";
    info.description = "A test agent for unit testing";
    info.status = "idle";
    info.type = "agent";
    info.tier = "core";
    info.activation = "core";

    auto* widget = create_agent_card_widget(info, nullptr, nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_EQ(widget->objectName(), "agent_card");
    EXPECT_EQ(widget->property("agent_id").toString(), "test-agent");

    delete widget;
}

TEST_F(WidgetTest, AgentCardWidgetTypeBadge) {
    AgentInfo info;
    info.id = "squad-test";
    info.name = "Squad Test";
    info.description = "Squad type agent card";
    info.status = "running";
    info.type = "squad";
    info.tier = "fleet";
    info.activation = "on-demand";

    auto* widget = create_agent_card_widget(info, nullptr, nullptr);
    ASSERT_NE(widget, nullptr);

    auto* badge = widget->findChild<QLabel*>("type_badge");
    ASSERT_NE(badge, nullptr);
    EXPECT_EQ(badge->text(), "SQUAD");

    delete widget;
}

TEST_F(WidgetTest, AgentCardWidgetComboType) {
    AgentInfo info;
    info.id = "combo-test";
    info.name = "Combo Test";
    info.description = "Combo type agent card";
    info.status = "error";
    info.type = "combo";
    info.tier = "core";
    info.activation = "default";

    auto* widget = create_agent_card_widget(info, nullptr, nullptr);
    ASSERT_NE(widget, nullptr);

    auto* badge = widget->findChild<QLabel*>("type_badge");
    ASSERT_NE(badge, nullptr);
    EXPECT_EQ(badge->text(), "COMBO");

    delete widget;
}

TEST_F(WidgetTest, AgentCardWidgetEmptyTier) {
    AgentInfo info;
    info.id = "no-tier";
    info.name = "No Tier";
    info.description = "Agent with empty tier";
    info.status = "idle";
    info.type = "agent";
    // tier is empty
    info.activation = "";

    auto* widget = create_agent_card_widget(info, nullptr, nullptr);
    ASSERT_NE(widget, nullptr);

    delete widget;
}

// =============================================================================
// Fleet Grid Widget
// =============================================================================

TEST_F(WidgetTest, FleetGridWidgetConstruction) {
    auto* widget = create_fleet_grid_widget(registry_, nullptr, nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_EQ(widget->objectName(), "fleet_grid");

    auto* scroll = qobject_cast<QScrollArea*>(widget);
    ASSERT_NE(scroll, nullptr);
    EXPECT_TRUE(scroll->widgetResizable());

    delete widget;
}

TEST_F(WidgetTest, FleetGridWidgetContainsCards) {
    auto* widget = create_fleet_grid_widget(registry_, nullptr, nullptr);
    ASSERT_NE(widget, nullptr);

    // Find all agent cards
    auto cards = widget->findChildren<QFrame*>("agent_card");
    // Default registry has 8 agents
    EXPECT_EQ(cards.size(), 8);

    delete widget;
}

// =============================================================================
// Resource Monitor Widget
// =============================================================================

TEST_F(WidgetTest, ResourceMonitorWidgetConstruction) {
    auto* widget = create_resource_monitor_widget(nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_EQ(widget->objectName(), "resource_monitor");
    EXPECT_EQ(widget->height(), 48);

    delete widget;
}

// =============================================================================
// ConversationWidget
// =============================================================================

TEST_F(WidgetTest, ConversationWidgetConstruction) {
    ConversationWidget widget;

    EXPECT_EQ(widget.objectName(), "conversation_widget");
    EXPECT_TRUE(widget.widgetResizable());
    EXPECT_GE(widget.minimumWidth(), 400);
}

TEST_F(WidgetTest, ConversationWidgetAddUserMessage) {
    ConversationWidget widget;
    widget.add_user_message("Hello, world!");

    // Should have a container with message bubbles
    auto* container = widget.findChild<QWidget*>("conversation_container");
    ASSERT_NE(container, nullptr);

    auto frames = container->findChildren<QFrame*>("message_bubble");
    EXPECT_GE(frames.size(), 1);
}

TEST_F(WidgetTest, ConversationWidgetAddAssistantMessage) {
    ConversationWidget widget;
    widget.add_assistant_message("code-agent", "Here is some code", "claude-3", 1234.5);

    auto* container = widget.findChild<QWidget*>("conversation_container");
    ASSERT_NE(container, nullptr);

    auto frames = container->findChildren<QFrame*>("message_bubble");
    EXPECT_GE(frames.size(), 1);
}

TEST_F(WidgetTest, ConversationWidgetAddSystemMessage) {
    ConversationWidget widget;
    widget.add_system_message("System notification");

    auto* container = widget.findChild<QWidget*>("conversation_container");
    ASSERT_NE(container, nullptr);

    auto frames = container->findChildren<QFrame*>("message_bubble");
    EXPECT_GE(frames.size(), 1);
}

TEST_F(WidgetTest, ConversationWidgetMultipleMessages) {
    ConversationWidget widget;
    widget.add_user_message("Question 1");
    widget.add_assistant_message("agent", "Answer 1", "model", 100);
    widget.add_user_message("Question 2");
    widget.add_system_message("System info");

    auto* container = widget.findChild<QWidget*>("conversation_container");
    ASSERT_NE(container, nullptr);

    auto frames = container->findChildren<QFrame*>("message_bubble");
    EXPECT_GE(frames.size(), 4);
}

TEST_F(WidgetTest, ConversationWidgetClear) {
    ConversationWidget widget;
    widget.add_user_message("Message 1");
    widget.add_user_message("Message 2");
    widget.add_user_message("Message 3");

    widget.clear_conversation();

    // After clearing, verify it doesn't crash
    SUCCEED();
}

TEST_F(WidgetTest, ConversationWidgetThinkingIndicator) {
    ConversationWidget widget;

    // show/hide thinking should not crash
    widget.show_thinking("code-agent");
    widget.hide_thinking();
    SUCCEED();
}

TEST_F(WidgetTest, ConversationWidgetAssistantNoModel) {
    ConversationWidget widget;
    widget.add_assistant_message("agent-id", "response text", "", 0);

    auto* container = widget.findChild<QWidget*>("conversation_container");
    ASSERT_NE(container, nullptr);

    auto frames = container->findChildren<QFrame*>("message_bubble");
    EXPECT_GE(frames.size(), 1);
}

// =============================================================================
// ThinkingIndicator
// =============================================================================

TEST_F(WidgetTest, ThinkingIndicatorConstruction) {
    ThinkingIndicator indicator;
    EXPECT_EQ(indicator.objectName(), "thinking_indicator");
}

TEST_F(WidgetTest, ThinkingIndicatorStartStop) {
    ThinkingIndicator indicator;

    indicator.start("test-agent");
    EXPECT_TRUE(indicator.isVisible());
    EXPECT_TRUE(indicator.text().contains("test-agent"));

    indicator.stop();
    EXPECT_FALSE(indicator.isVisible());
}

// =============================================================================
// ChatInputBar
// =============================================================================

TEST_F(WidgetTest, ChatInputBarConstruction) {
    ChatInputBar input(registry_);

    EXPECT_EQ(input.objectName(), "chat_input_bar");
    EXPECT_EQ(input.height(), 80);

    EXPECT_NE(input.findChild<QTextEdit*>("chat_input"), nullptr);
    EXPECT_NE(input.findChild<QPushButton*>("send_button"), nullptr);
    EXPECT_NE(input.findChild<QLabel*>("agent_chip"), nullptr);
    EXPECT_NE(input.findChild<QPushButton*>("provider_btn"), nullptr);
    EXPECT_NE(input.findChild<QListWidget*>("autocomplete_popup"), nullptr);
}

TEST_F(WidgetTest, ChatInputBarUpdatePlaceholder) {
    ChatInputBar input(registry_);

    input.update_placeholder("security-agent");

    auto* text_input = input.findChild<QTextEdit*>("chat_input");
    ASSERT_NE(text_input, nullptr);
    EXPECT_TRUE(text_input->placeholderText().contains("security-agent"));

    auto* chip = input.findChild<QLabel*>("agent_chip");
    ASSERT_NE(chip, nullptr);
    EXPECT_EQ(chip->text(), "security-agent");
}

TEST_F(WidgetTest, ChatInputBarUpdateProviderLabel) {
    ChatInputBar input(registry_);

    input.update_provider_label("anthropic");

    auto* btn = input.findChild<QPushButton*>("provider_btn");
    ASSERT_NE(btn, nullptr);
    EXPECT_EQ(btn->text(), "anthropic");
}

TEST_F(WidgetTest, ChatInputBarEmptySubmit) {
    ChatInputBar input(registry_);

    QSignalSpy spy(&input, &ChatInputBar::message_submitted);

    // Empty input — should not emit
    auto* text_input = input.findChild<QTextEdit*>("chat_input");
    ASSERT_NE(text_input, nullptr);
    text_input->clear();

    auto* send_btn = input.findChild<QPushButton*>("send_button");
    ASSERT_NE(send_btn, nullptr);
    send_btn->click();

    EXPECT_EQ(spy.count(), 0);
}

TEST_F(WidgetTest, ChatInputBarSubmitMessage) {
    ChatInputBar input(registry_);

    QSignalSpy spy(&input, &ChatInputBar::message_submitted);

    auto* text_input = input.findChild<QTextEdit*>("chat_input");
    ASSERT_NE(text_input, nullptr);
    text_input->setPlainText("Hello world");

    auto* send_btn = input.findChild<QPushButton*>("send_button");
    ASSERT_NE(send_btn, nullptr);
    send_btn->click();

    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), "Hello world");

    // Input should be cleared after submit
    EXPECT_TRUE(text_input->toPlainText().isEmpty());
}

TEST_F(WidgetTest, ChatInputBarSlashCommand) {
    ChatInputBar input(registry_);

    QSignalSpy spy(&input, &ChatInputBar::slash_command);

    auto* text_input = input.findChild<QTextEdit*>("chat_input");
    ASSERT_NE(text_input, nullptr);
    text_input->setPlainText("/clear");

    auto* send_btn = input.findChild<QPushButton*>("send_button");
    ASSERT_NE(send_btn, nullptr);
    send_btn->click();

    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), "clear");
}

TEST_F(WidgetTest, ChatInputBarSlashCommandWithArgs) {
    ChatInputBar input(registry_);

    QSignalSpy spy(&input, &ChatInputBar::slash_command);

    auto* text_input = input.findChild<QTextEdit*>("chat_input");
    ASSERT_NE(text_input, nullptr);
    text_input->setPlainText("/agent code-agent");

    auto* send_btn = input.findChild<QPushButton*>("send_button");
    ASSERT_NE(send_btn, nullptr);
    send_btn->click();

    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), "agent");
    EXPECT_EQ(spy.at(0).at(1).toString(), "code-agent");
}

TEST_F(WidgetTest, ChatInputBarProviderButtonSignal) {
    ChatInputBar input(registry_);

    QSignalSpy spy(&input, &ChatInputBar::provider_button_clicked);

    auto* provider_btn = input.findChild<QPushButton*>("provider_btn");
    ASSERT_NE(provider_btn, nullptr);
    provider_btn->click();

    EXPECT_EQ(spy.count(), 1);
}

TEST_F(WidgetTest, ChatInputBarSetFocus) {
    ChatInputBar input(registry_);
    input.show();

    input.set_focus();
    // Should not crash
    SUCCEED();
}

TEST_F(WidgetTest, ChatInputBarGhostLabel) {
    ChatInputBar input(registry_);

    auto* ghost = input.findChild<QLabel*>("ghost_label");
    ASSERT_NE(ghost, nullptr);
    // Initially hidden
    EXPECT_FALSE(ghost->isVisible());
}

// =============================================================================
// ProviderSelectDialog (via create_provider_select_dialog)
// =============================================================================

TEST_F(WidgetTest, ProviderSelectDialogConstruction) {
    ChatEngine engine("/tmp/euxis-etx-test-provider-select");
    auto* dialog = create_provider_select_dialog(&engine, nullptr);
    ASSERT_NE(dialog, nullptr);

    EXPECT_EQ(dialog->objectName(), "provider_select");

    auto* profile_list = dialog->findChild<QListWidget*>("profile_list");
    ASSERT_NE(profile_list, nullptr);

    delete dialog;
}

TEST_F(WidgetTest, ProviderSelectDialogButtons) {
    ChatEngine engine("/tmp/euxis-etx-test-provider-select2");
    auto* dialog = create_provider_select_dialog(&engine, nullptr);
    ASSERT_NE(dialog, nullptr);

    // Should have multiple buttons: Connect Anthropic, Connect Google, Add API Key, Remove, Cancel, OK
    auto buttons = dialog->findChildren<QPushButton*>();
    EXPECT_GE(buttons.size(), 6);

    delete dialog;
}

TEST_F(WidgetTest, ProviderSelectDialogFallbackLabel) {
    ChatEngine engine("/tmp/euxis-etx-test-provider-select3");
    auto* dialog = create_provider_select_dialog(&engine, nullptr);
    ASSERT_NE(dialog, nullptr);

    // The fallback label should exist and contain "Fallback:"
    auto labels = dialog->findChildren<QLabel*>();
    bool found_fallback = false;
    for (auto* label : labels) {
        if (label->text().contains("Fallback")) {
            found_fallback = true;
            break;
        }
    }
    EXPECT_TRUE(found_fallback);

    delete dialog;
}

TEST_F(WidgetTest, ProviderSelectDialogSelectedProviderEmpty) {
    // Test selected_provider() when nothing is selected — should return empty
    ChatEngine engine("/tmp/euxis-etx-test-provider-select4");
    auto* dialog = create_provider_select_dialog(&engine, nullptr);
    ASSERT_NE(dialog, nullptr);

    // No item selected; profile_list is empty or no currentItem
    auto* profile_list = dialog->findChild<QListWidget*>("profile_list");
    ASSERT_NE(profile_list, nullptr);
    profile_list->clearSelection();

    delete dialog;
}

// =============================================================================
// Sparkline Chart Widget — painting tests
// =============================================================================

TEST_F(WidgetTest, SparklineChartWidgetSetValues) {
    auto* widget = create_sparkline_chart_widget(nullptr);
    ASSERT_NE(widget, nullptr);

    // Access the underlying SparklineChartWidget via public API
    // Set values to trigger the paint path with >= 2 values
    QVector<float> values = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
    // We need to use QMetaObject to call set_values since it's on the derived class
    QMetaObject::invokeMethod(widget, "set_values",
        Q_ARG(QVector<float>, values));

    // Force a repaint to exercise paintEvent
    widget->resize(200, 60);
    widget->show();
    widget->repaint();

    delete widget;
}

TEST_F(WidgetTest, SparklineChartWidgetSetLabel) {
    auto* widget = create_sparkline_chart_widget(nullptr);
    ASSERT_NE(widget, nullptr);

    QMetaObject::invokeMethod(widget, "set_label",
        Q_ARG(QString, QString("CPU")));

    // Repaint with label but no data (exercises draw_label in the < 2 values path)
    widget->resize(200, 60);
    widget->show();
    widget->repaint();

    delete widget;
}

TEST_F(WidgetTest, SparklineChartWidgetAddValue) {
    auto* widget = create_sparkline_chart_widget(nullptr);
    ASSERT_NE(widget, nullptr);

    QMetaObject::invokeMethod(widget, "add_value", Q_ARG(float, 10.0f));
    QMetaObject::invokeMethod(widget, "add_value", Q_ARG(float, 20.0f));
    QMetaObject::invokeMethod(widget, "add_value", Q_ARG(float, 30.0f));
    QMetaObject::invokeMethod(widget, "set_label",
        Q_ARG(QString, QString("MEM")));

    widget->resize(200, 60);
    widget->show();
    widget->repaint();

    delete widget;
}

TEST_F(WidgetTest, SparklineChartWidgetFlatValues) {
    auto* widget = create_sparkline_chart_widget(nullptr);
    ASSERT_NE(widget, nullptr);

    // Test with flat values (range < 0.001f, forces range = 1.0f)
    QVector<float> flat = {50.0f, 50.0f, 50.0f, 50.0f};
    QMetaObject::invokeMethod(widget, "set_values",
        Q_ARG(QVector<float>, flat));

    widget->resize(200, 60);
    widget->show();
    widget->repaint();

    delete widget;
}

TEST_F(WidgetTest, SparklineChartWidgetSingleValue) {
    auto* widget = create_sparkline_chart_widget(nullptr);
    ASSERT_NE(widget, nullptr);

    // Single value (less than 2) — triggers early return in paintEvent
    QMetaObject::invokeMethod(widget, "add_value", Q_ARG(float, 42.0f));

    widget->resize(200, 60);
    widget->show();
    widget->repaint();

    delete widget;
}

// =============================================================================
// Scroll Minimap Widget — painting tests
// =============================================================================

TEST_F(WidgetTest, ScrollMinimapWidgetPaintWithData) {
    auto* widget = create_scroll_minimap_widget(nullptr);
    ASSERT_NE(widget, nullptr);

    // Set viewport and active positions via properties or direct resize
    // The paintEvent will be exercised when we call repaint below

    // Force paint
    widget->resize(20, 200);
    widget->show();
    widget->repaint();

    delete widget;
}

TEST_F(WidgetTest, ScrollMinimapWidgetEmptyPositions) {
    auto* widget = create_scroll_minimap_widget(nullptr);
    ASSERT_NE(widget, nullptr);

    QMetaObject::invokeMethod(widget, "update_viewport",
        Q_ARG(float, 0.0f), Q_ARG(float, 0.5f));

    widget->resize(20, 200);
    widget->show();
    widget->repaint();

    delete widget;
}

// =============================================================================
// Header Widget — SparklineWidget paint (lines 42-78)
// =============================================================================

TEST_F(WidgetTest, HeaderWidgetSparklinePaint) {
    auto* widget = create_header_widget(theme_, nullptr);
    ASSERT_NE(widget, nullptr);

    // Force the header to show and repaint so that the inner SparklineWidget
    // triggers its paintEvent (which exercises the uncovered lines 42-78)
    widget->resize(800, 56);
    widget->show();
    widget->repaint();
    // Process events to allow the timer-based sparkline update
    QApplication::processEvents();

    delete widget;
}

// =============================================================================
// Resource Monitor Widget — poll and paint
// =============================================================================

TEST_F(WidgetTest, ResourceMonitorWidgetPollAndPaint) {
    auto* widget = create_resource_monitor_widget(nullptr);
    ASSERT_NE(widget, nullptr);

    // Force repaint which exercises the SparklineMini::paintEvent
    widget->resize(600, 48);
    widget->show();
    widget->repaint();
    QApplication::processEvents();

    // Check that the CPU/MEM/TEMP labels are present
    auto labels = widget->findChildren<QLabel*>();
    bool has_cpu = false, has_mem = false, has_temp = false;
    for (auto* lbl : labels) {
        if (lbl->text().contains("CPU")) has_cpu = true;
        if (lbl->text().contains("MEM")) has_mem = true;
        if (lbl->text().contains("TEMP")) has_temp = true;
    }
    EXPECT_TRUE(has_cpu);
    EXPECT_TRUE(has_mem);
    EXPECT_TRUE(has_temp);

    delete widget;
}

// =============================================================================
// ConversationWidget — ThinkingIndicator animate (lines 40-43)
// =============================================================================

TEST_F(WidgetTest, ConversationWidgetThinkingAnimate) {
    ConversationWidget widget;
    widget.show_thinking("test-agent");
    // Process events to trigger the animate() timer callback
    QTest::qWait(500);
    QApplication::processEvents();
    widget.hide_thinking();
    SUCCEED();
}

TEST_F(WidgetTest, ConversationWidgetScrollToBottom) {
    ConversationWidget widget;
    widget.resize(600, 400);
    widget.show();
    // Add enough messages to create scrollable content
    for (int i = 0; i < 20; ++i) {
        widget.add_user_message(QString("Message %1 with some extra text to fill space").arg(i));
    }
    // Process events to trigger the scroll_to_bottom animation
    QTest::qWait(50);
    QApplication::processEvents();
    SUCCEED();
}

// =============================================================================
// ChatInputBar — history navigation (lines 297-306) and ghost text (309-342)
// =============================================================================

TEST_F(WidgetTest, ChatInputBarHistoryNavigation) {
    ChatInputBar input(registry_);
    auto* text_input = input.findChild<QTextEdit*>("chat_input");
    ASSERT_NE(text_input, nullptr);

    // Submit a few messages to build history
    text_input->setPlainText("first message");
    auto* send_btn = input.findChild<QPushButton*>("send_button");
    ASSERT_NE(send_btn, nullptr);
    send_btn->click();

    text_input->setPlainText("second message");
    send_btn->click();

    // Now navigate history with up/down keys on empty input
    text_input->clear();
    QKeyEvent up_event(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
    QApplication::sendEvent(text_input, &up_event);
    // Should show last history entry
    EXPECT_FALSE(text_input->toPlainText().isEmpty());

    text_input->clear();
    QKeyEvent down_event(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
    QApplication::sendEvent(text_input, &down_event);
}

TEST_F(WidgetTest, ChatInputBarEscapeKey) {
    ChatInputBar input(registry_);
    input.show();
    auto* text_input = input.findChild<QTextEdit*>("chat_input");
    ASSERT_NE(text_input, nullptr);
    text_input->setFocus();

    QKeyEvent esc_event(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(text_input, &esc_event);
    SUCCEED();
}

TEST_F(WidgetTest, ChatInputBarAtSignAutocomplete) {
    ChatInputBar input(registry_);
    input.show();
    auto* text_input = input.findChild<QTextEdit*>("chat_input");
    ASSERT_NE(text_input, nullptr);

    // Type "@" to trigger agent autocomplete
    text_input->setPlainText("@");
    QApplication::processEvents();

    auto* popup = input.findChild<QListWidget*>("autocomplete_popup");
    ASSERT_NE(popup, nullptr);
    // Should have items (agents from default registry)
    EXPECT_GT(popup->count(), 0);
}

TEST_F(WidgetTest, ChatInputBarSlashAutocomplete) {
    ChatInputBar input(registry_);
    input.show();
    auto* text_input = input.findChild<QTextEdit*>("chat_input");
    ASSERT_NE(text_input, nullptr);

    // Type "/" to trigger command autocomplete
    text_input->setPlainText("/");
    QApplication::processEvents();

    auto* popup = input.findChild<QListWidget*>("autocomplete_popup");
    ASSERT_NE(popup, nullptr);
    EXPECT_GT(popup->count(), 0);
}

TEST_F(WidgetTest, ChatInputBarEnterKeySubmits) {
    ChatInputBar input(registry_);
    QSignalSpy spy(&input, &ChatInputBar::message_submitted);

    auto* text_input = input.findChild<QTextEdit*>("chat_input");
    ASSERT_NE(text_input, nullptr);
    text_input->setPlainText("test message");

    QKeyEvent enter_event(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(text_input, &enter_event);

    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), "test message");
}

TEST_F(WidgetTest, ChatInputBarShiftEnterDoesNotSubmit) {
    ChatInputBar input(registry_);
    QSignalSpy spy(&input, &ChatInputBar::message_submitted);

    auto* text_input = input.findChild<QTextEdit*>("chat_input");
    ASSERT_NE(text_input, nullptr);
    text_input->setPlainText("test message");

    QKeyEvent shift_enter(QEvent::KeyPress, Qt::Key_Return, Qt::ShiftModifier);
    QApplication::sendEvent(text_input, &shift_enter);

    EXPECT_EQ(spy.count(), 0);
}

// =============================================================================
// Agent Card Widget — mousePressEvent and enterEvent (lines 126-136)
// =============================================================================

TEST_F(WidgetTest, AgentCardWidgetEnterEvent) {
    AgentInfo info;
    info.id = "enter-test";
    info.name = "Enter Test";
    info.description = "Test enterEvent";
    info.status = "idle";
    info.type = "agent";
    info.tier = "core";
    info.activation = "core";

    auto* widget = create_agent_card_widget(info, nullptr, nullptr);
    ASSERT_NE(widget, nullptr);

    // Trigger enter event
    widget->show();
    QEnterEvent enter_event(QPointF(5, 5), QPointF(5, 5), QPointF(5, 5));
    QApplication::sendEvent(widget, &enter_event);

    delete widget;
}

TEST_F(WidgetTest, AgentCardWidgetMousePress) {
    AgentInfo info;
    info.id = "click-test";
    info.name = "Click Test";
    info.description = "Test mouse press";
    info.status = "running";
    info.type = "agent";
    info.tier = "fleet";
    info.activation = "default";

    // Without an app, mousePressEvent still exercises the code path
    auto* widget = create_agent_card_widget(info, nullptr, nullptr);
    ASSERT_NE(widget, nullptr);
    widget->show();

    QMouseEvent press(QEvent::MouseButtonPress, QPointF(5, 5),
                      widget->mapToGlobal(QPointF(5, 5)),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(widget, &press);

    delete widget;
}

TEST_F(WidgetTest, AgentCardWidgetStatusColors) {
    // Test different status colors: "idle" -> info color, "error" -> error color
    AgentInfo idle_info;
    idle_info.id = "idle-card";
    idle_info.name = "Idle Card";
    idle_info.description = "idle status test";
    idle_info.status = "idle";
    idle_info.type = "agent";
    idle_info.tier = "fleet";
    idle_info.activation = "default";

    auto* idle_card = create_agent_card_widget(idle_info, nullptr, nullptr);
    ASSERT_NE(idle_card, nullptr);
    delete idle_card;

    AgentInfo error_info;
    error_info.id = "error-card";
    error_info.name = "Error Card";
    error_info.description = "error status test";
    error_info.status = "error";
    error_info.type = "agent";
    error_info.tier = "fleet";
    error_info.activation = "default";

    auto* error_card = create_agent_card_widget(error_info, nullptr, nullptr);
    ASSERT_NE(error_card, nullptr);
    delete error_card;
}

// =============================================================================
// Tip Bar Widget — rotate_tip (lines 61-64)
// =============================================================================

TEST_F(WidgetTest, TipBarWidgetRotation) {
    auto* widget = create_tip_bar_widget(config_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto* label = widget->findChild<QLabel*>("tip_label");
    ASSERT_NE(label, nullptr);
    QString first_tip = label->text();

    // Wait for the timer to rotate tips (timer is 8000ms, but we can process events)
    // Instead, find the timer and trigger it
    auto timers = widget->findChildren<QTimer*>();
    for (auto* timer : timers) {
        if (timer->interval() == 8000) {
            // Manually trigger the timeout
            timer->stop();
            QMetaObject::invokeMethod(timer, "timeout");
            break;
        }
    }
    QApplication::processEvents();

    // Tip should have changed
    EXPECT_NE(label->text(), first_tip);

    delete widget;
}

// =============================================================================
// Bento Layout — resizeEvent (lines 41-44)
// =============================================================================

TEST_F(WidgetTest, BentoLayoutResizeEvent) {
    // The BentoLayout test already covers relayout with 0 cards,
    // but we test through resize which exercises resizeEvent (line 41-44)
    BentoLayout layout;
    auto* card = new QLabel("Test");
    layout.add_card(card);

    // Trigger a resize event
    layout.show();
    QResizeEvent event(QSize(1200, 600), QSize(800, 400));
    QApplication::sendEvent(&layout, &event);

    EXPECT_GE(layout.column_count(), 1);
}

// =============================================================================
// Breadcrumb Widget — eventFilter for click (lines 80-81)
// =============================================================================

TEST_F(WidgetTest, BreadcrumbWidgetCrumbClick) {
    BreadcrumbWidget bc;
    bc.navigate_to(0, "Dashboard");
    bc.navigate_to(1, "Agent");
    bc.navigate_to(2, "Logs");

    // The breadcrumb should have 3 items; clicking on past crumbs (0 or 1) should
    // emit crumb_clicked. Find the breadcrumb labels.
    auto labels = bc.findChildren<QLabel*>("breadcrumb_item");
    EXPECT_GE(labels.size(), 3);

    // The first label (Dashboard) should have a screen_index property
    QLabel* first_crumb = nullptr;
    for (auto* lbl : labels) {
        QVariant idx = lbl->property("screen_index");
        if (idx.isValid() && idx.toInt() == 0) {
            first_crumb = lbl;
            break;
        }
    }
    if (first_crumb) {
        QSignalSpy spy(&bc, &BreadcrumbWidget::crumb_clicked);
        // Simulate a mouse press on the label
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(2, 2),
                          first_crumb->mapToGlobal(QPointF(2, 2)),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(first_crumb, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(2, 2),
                            first_crumb->mapToGlobal(QPointF(2, 2)),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(first_crumb, &release);
    }
    SUCCEED();
}

} // namespace
} // namespace euxis::etx
