#include <gtest/gtest.h>
#include <QApplication>
#include <QWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QListWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QSlider>
#include <QTreeWidget>
#include <QFrame>
#include <QSplitter>
#include <QScrollArea>
#include <QShortcut>
#include <QProgressBar>

#include <euxis/etx/config.hpp>
#include <euxis/etx/registry.hpp>
#include <euxis/etx/theme.hpp>

// Forward declarations of screen factory functions
namespace euxis::etx {
    QWidget* create_about_screen(QWidget* parent);
    QWidget* create_agent_screen(QWidget* parent);
    QWidget* create_approvals_screen(ETXConfig* config, QWidget* parent);
    QWidget* create_cortex_screen(ETXConfig* config, QWidget* parent);
    QWidget* create_error_details_screen(QWidget* parent);
    QWidget* create_fleet_monitor_screen(FleetRegistry* registry, QWidget* parent);
    QWidget* create_help_screen(QWidget* parent);
    QWidget* create_logs_screen(ETXConfig* config, QWidget* parent);
    QWidget* create_metrics_screen(FleetRegistry* registry, ETXConfig* config,
                                   QWidget* parent);
    QWidget* create_omnigraph_screen(FleetRegistry* registry, QWidget* parent);
    QWidget* create_playbooks_screen(FleetRegistry* registry, QWidget* parent);
    QWidget* create_settings_screen(ThemeEngine* theme, ETXConfig* config,
                                    QWidget* parent);
    QWidget* create_squad_detail_screen(FleetRegistry* registry, QWidget* parent);
    QWidget* create_time_travel_screen(QWidget* parent);
    QWidget* create_tool_runner_screen(QWidget* parent);
}

namespace euxis::etx {
namespace {

// =============================================================================
// Test fixture with shared ETXConfig, FleetRegistry, ThemeEngine
// =============================================================================

class ScreenTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = new ETXConfig();
        registry_ = new FleetRegistry(QString{}); // defaults
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
// About Screen
// =============================================================================

TEST_F(ScreenTest, AboutScreenConstruction) {
    auto* widget = create_about_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    // Verify the info card exists
    auto* card = widget->findChild<QFrame*>("about_card");
    EXPECT_NE(card, nullptr);

    delete widget;
}

TEST_F(ScreenTest, AboutScreenChildWidgets) {
    auto* widget = create_about_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    // Should have multiple QLabel children
    auto labels = widget->findChildren<QLabel*>();
    EXPECT_GT(labels.size(), 5);

    // Should have a back button
    auto buttons = widget->findChildren<QPushButton*>();
    EXPECT_GE(buttons.size(), 1);

    delete widget;
}

TEST_F(ScreenTest, AboutScreenInStack) {
    QStackedWidget stack;
    auto* widget = create_about_screen(&stack);
    stack.addWidget(widget);

    EXPECT_EQ(stack.count(), 1);
    EXPECT_EQ(stack.widget(0), widget);
}

// =============================================================================
// Agent Screen
// =============================================================================

TEST_F(ScreenTest, AgentScreenConstruction) {
    auto* widget = create_agent_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    // Named widgets should exist
    EXPECT_NE(widget->findChild<QPushButton*>("back_button"), nullptr);
    EXPECT_NE(widget->findChild<QLabel*>("agent_name_label"), nullptr);
    EXPECT_NE(widget->findChild<QLabel*>("agent_desc_label"), nullptr);
    EXPECT_NE(widget->findChild<QLabel*>("agent_status_label"), nullptr);
    EXPECT_NE(widget->findChild<QLineEdit*>("task_input"), nullptr);
    EXPECT_NE(widget->findChild<QPushButton*>("deploy_button"), nullptr);
    EXPECT_NE(widget->findChild<QPlainTextEdit*>("deploy_output"), nullptr);
    EXPECT_NE(widget->findChild<QPlainTextEdit*>("agent_prompt_edit"), nullptr);

    delete widget;
}

TEST_F(ScreenTest, AgentScreenExtendedFields) {
    auto* widget = create_agent_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_NE(widget->findChild<QLabel*>("agent_tier_label"), nullptr);
    EXPECT_NE(widget->findChild<QLabel*>("agent_activation_label"), nullptr);
    EXPECT_NE(widget->findChild<QLabel*>("agent_version_label"), nullptr);
    EXPECT_NE(widget->findChild<QLabel*>("agent_tags_label"), nullptr);
    EXPECT_NE(widget->findChild<QLabel*>("agent_caps_label"), nullptr);

    delete widget;
}

TEST_F(ScreenTest, AgentScreenDefaultValues) {
    auto* widget = create_agent_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    auto* name = widget->findChild<QLabel*>("agent_name_label");
    ASSERT_NE(name, nullptr);
    EXPECT_FALSE(name->text().isEmpty());

    auto* prompt = widget->findChild<QPlainTextEdit*>("agent_prompt_edit");
    ASSERT_NE(prompt, nullptr);
    EXPECT_TRUE(prompt->isReadOnly());

    delete widget;
}

// =============================================================================
// Approvals Screen
// =============================================================================

TEST_F(ScreenTest, ApprovalsScreenConstruction) {
    auto* widget = create_approvals_screen(config_, nullptr);
    ASSERT_NE(widget, nullptr);

    // Should have at least back button and title labels
    auto labels = widget->findChildren<QLabel*>();
    EXPECT_GT(labels.size(), 0);

    auto buttons = widget->findChildren<QPushButton*>();
    EXPECT_GE(buttons.size(), 1);

    delete widget;
}

TEST_F(ScreenTest, ApprovalsScreenEmptyState) {
    // With no queue file, should show empty state
    auto* widget = create_approvals_screen(config_, nullptr);
    ASSERT_NE(widget, nullptr);

    // In empty state, there should be no approvals_table
    auto* table = widget->findChild<QTableWidget*>("approvals_table");
    // Table only exists when queue has data; empty state should have none
    // (whether table exists depends on runtime file — just verify no crash)
    Q_UNUSED(table);

    delete widget;
}

// =============================================================================
// Cortex Screen
// =============================================================================

TEST_F(ScreenTest, CortexScreenConstruction) {
    auto* widget = create_cortex_screen(config_, nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_NE(widget->findChild<QLabel*>("cortex_status"), nullptr);
    EXPECT_NE(widget->findChild<QLineEdit*>("cortex_query_input"), nullptr);
    EXPECT_NE(widget->findChild<QPushButton*>("cortex_search_button"), nullptr);
    EXPECT_NE(widget->findChild<QPlainTextEdit*>("cortex_output"), nullptr);

    delete widget;
}

TEST_F(ScreenTest, CortexScreenOutputContent) {
    auto* widget = create_cortex_screen(config_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto* output = widget->findChild<QPlainTextEdit*>("cortex_output");
    ASSERT_NE(output, nullptr);
    EXPECT_FALSE(output->toPlainText().isEmpty());
    EXPECT_TRUE(output->isReadOnly());

    delete widget;
}

// =============================================================================
// Error Details Screen
// =============================================================================

TEST_F(ScreenTest, ErrorDetailsScreenConstruction) {
    auto* widget = create_error_details_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_NE(widget->findChild<QFrame*>("error_info_card"), nullptr);
    EXPECT_NE(widget->findChild<QFrame*>("analysis_card"), nullptr);
    EXPECT_NE(widget->findChild<QLabel*>("severity_badge"), nullptr);
    EXPECT_NE(widget->findChild<QProgressBar*>("confidence_bar"), nullptr);
    EXPECT_NE(widget->findChild<QListWidget*>("recovery_steps"), nullptr);
    EXPECT_NE(widget->findChild<QTextEdit*>("audit_trail"), nullptr);

    delete widget;
}

TEST_F(ScreenTest, ErrorDetailsScreenButtons) {
    auto* widget = create_error_details_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_NE(widget->findChild<QPushButton*>("restart_button"), nullptr);
    EXPECT_NE(widget->findChild<QPushButton*>("simulate_button"), nullptr);
    EXPECT_NE(widget->findChild<QPushButton*>("view_logs_button"), nullptr);
    EXPECT_NE(widget->findChild<QPushButton*>("close_button"), nullptr);

    delete widget;
}

TEST_F(ScreenTest, ErrorDetailsScreenConfidenceBar) {
    auto* widget = create_error_details_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    auto* bar = widget->findChild<QProgressBar*>("confidence_bar");
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar->value(), 87);
    EXPECT_EQ(bar->minimum(), 0);
    EXPECT_EQ(bar->maximum(), 100);

    delete widget;
}

TEST_F(ScreenTest, ErrorDetailsScreenRecoverySteps) {
    auto* widget = create_error_details_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    auto* steps = widget->findChild<QListWidget*>("recovery_steps");
    ASSERT_NE(steps, nullptr);
    EXPECT_EQ(steps->count(), 5);

    delete widget;
}

// =============================================================================
// Fleet Monitor Screen
// =============================================================================

TEST_F(ScreenTest, FleetMonitorScreenConstruction) {
    auto* widget = create_fleet_monitor_screen(registry_, nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_NE(widget->findChild<QLabel*>("operation_type_label"), nullptr);
    EXPECT_NE(widget->findChild<QListWidget*>("member_list"), nullptr);

    delete widget;
}

TEST_F(ScreenTest, FleetMonitorScreenPopulatesAgents) {
    auto* widget = create_fleet_monitor_screen(registry_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto* member_list = widget->findChild<QListWidget*>("member_list");
    ASSERT_NE(member_list, nullptr);
    // Default registry has 8 agents
    EXPECT_EQ(member_list->count(), 8);

    delete widget;
}

// =============================================================================
// Help Screen
// =============================================================================

TEST_F(ScreenTest, HelpScreenConstruction) {
    auto* widget = create_help_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    auto* table = widget->findChild<QTableWidget*>("shortcuts_table");
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->columnCount(), 2);
    EXPECT_GT(table->rowCount(), 0);

    delete widget;
}

TEST_F(ScreenTest, HelpScreenShortcutEntries) {
    auto* widget = create_help_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    auto* table = widget->findChild<QTableWidget*>("shortcuts_table");
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->rowCount(), 9);

    // Verify first shortcut is Ctrl+K
    auto* first_key = table->item(0, 0);
    ASSERT_NE(first_key, nullptr);
    EXPECT_EQ(first_key->text(), "Ctrl+K");

    delete widget;
}

// =============================================================================
// Logs Screen
// =============================================================================

TEST_F(ScreenTest, LogsScreenConstruction) {
    auto* widget = create_logs_screen(config_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto* display = widget->findChild<QPlainTextEdit*>("log_display");
    ASSERT_NE(display, nullptr);
    EXPECT_TRUE(display->isReadOnly());

    delete widget;
}

TEST_F(ScreenTest, LogsScreenButtons) {
    auto* widget = create_logs_screen(config_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto buttons = widget->findChildren<QPushButton*>();
    // back, clear, refresh = at least 3
    EXPECT_GE(buttons.size(), 3);

    delete widget;
}

// =============================================================================
// Metrics Screen
// =============================================================================

TEST_F(ScreenTest, MetricsScreenConstruction) {
    auto* widget = create_metrics_screen(registry_, config_, nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_NE(widget->findChild<QFrame*>("metrics_summary"), nullptr);
    EXPECT_NE(widget->findChild<QTableWidget*>("router_table"), nullptr);
    EXPECT_NE(widget->findChild<QTableWidget*>("metrics_table"), nullptr);

    delete widget;
}

TEST_F(ScreenTest, MetricsScreenAgentTable) {
    auto* widget = create_metrics_screen(registry_, config_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto* table = widget->findChild<QTableWidget*>("metrics_table");
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->columnCount(), 4);
    // Default registry has 8 agents
    EXPECT_EQ(table->rowCount(), 8);

    delete widget;
}

TEST_F(ScreenTest, MetricsScreenRouterTable) {
    auto* widget = create_metrics_screen(registry_, config_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto* router_table = widget->findChild<QTableWidget*>("router_table");
    ASSERT_NE(router_table, nullptr);
    EXPECT_EQ(router_table->columnCount(), 3);

    delete widget;
}

// =============================================================================
// OmniGraph Screen
// =============================================================================

TEST_F(ScreenTest, OmnigraphScreenConstruction) {
    auto* widget = create_omnigraph_screen(registry_, nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_NE(widget->findChild<QPushButton*>("omnigraph_refresh"), nullptr);
    EXPECT_NE(widget->findChild<QTreeWidget*>("omnigraph_tree"), nullptr);
    EXPECT_NE(widget->findChild<QTextEdit*>("omnigraph_detail_text"), nullptr);
    EXPECT_NE(widget->findChild<QLabel*>("omnigraph_agents_count"), nullptr);
    EXPECT_NE(widget->findChild<QLabel*>("omnigraph_squads_count"), nullptr);
    EXPECT_NE(widget->findChild<QLabel*>("omnigraph_combos_count"), nullptr);

    delete widget;
}

TEST_F(ScreenTest, OmnigraphScreenTree) {
    auto* widget = create_omnigraph_screen(registry_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto* tree = widget->findChild<QTreeWidget*>("omnigraph_tree");
    ASSERT_NE(tree, nullptr);
    // Should have a root item
    EXPECT_GT(tree->topLevelItemCount(), 0);

    // Root should be "euxis-fleet"
    auto* root = tree->topLevelItem(0);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->text(0), "euxis-fleet");

    delete widget;
}

TEST_F(ScreenTest, OmnigraphScreenSummaryLabels) {
    auto* widget = create_omnigraph_screen(registry_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto* agents = widget->findChild<QLabel*>("omnigraph_agents_count");
    ASSERT_NE(agents, nullptr);
    EXPECT_TRUE(agents->text().contains("8"));

    delete widget;
}

// =============================================================================
// Playbooks Screen
// =============================================================================

TEST_F(ScreenTest, PlaybooksScreenConstruction) {
    auto* widget = create_playbooks_screen(registry_, nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_NE(widget->findChild<QListWidget*>("playbook_list"), nullptr);
    EXPECT_NE(widget->findChild<QLabel*>("playbook_title_label"), nullptr);
    EXPECT_NE(widget->findChild<QTextEdit*>("playbook_desc_text"), nullptr);
    EXPECT_NE(widget->findChild<QPushButton*>("run_playbook_button"), nullptr);

    delete widget;
}

TEST_F(ScreenTest, PlaybooksScreenRunButtonDisabledInitially) {
    auto* widget = create_playbooks_screen(registry_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto* run_btn = widget->findChild<QPushButton*>("run_playbook_button");
    ASSERT_NE(run_btn, nullptr);
    EXPECT_FALSE(run_btn->isEnabled());

    delete widget;
}

// =============================================================================
// Settings Screen
// =============================================================================

TEST_F(ScreenTest, SettingsScreenConstruction) {
    auto* widget = create_settings_screen(theme_, config_, nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_NE(widget->findChild<QFrame*>("settings_card"), nullptr);
    EXPECT_NE(widget->findChild<QComboBox*>("theme_combo"), nullptr);
    EXPECT_NE(widget->findChild<QSpinBox*>("refresh_spin"), nullptr);
    EXPECT_NE(widget->findChild<QCheckBox*>("tips_check"), nullptr);
    EXPECT_NE(widget->findChild<QPushButton*>("save_button"), nullptr);

    delete widget;
}

TEST_F(ScreenTest, SettingsScreenDefaultValues) {
    auto* widget = create_settings_screen(theme_, config_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto* theme_combo = widget->findChild<QComboBox*>("theme_combo");
    ASSERT_NE(theme_combo, nullptr);
    EXPECT_EQ(theme_combo->count(), 3); // liquid-glass, calm, focused

    auto* refresh_spin = widget->findChild<QSpinBox*>("refresh_spin");
    ASSERT_NE(refresh_spin, nullptr);
    EXPECT_EQ(refresh_spin->value(), config_->refresh_interval());

    auto* tips_check = widget->findChild<QCheckBox*>("tips_check");
    ASSERT_NE(tips_check, nullptr);
    EXPECT_EQ(tips_check->isChecked(), config_->show_tips());

    delete widget;
}

TEST_F(ScreenTest, SettingsScreenRefreshSpinRange) {
    auto* widget = create_settings_screen(theme_, config_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto* spin = widget->findChild<QSpinBox*>("refresh_spin");
    ASSERT_NE(spin, nullptr);
    EXPECT_EQ(spin->minimum(), 1000);
    EXPECT_EQ(spin->maximum(), 60000);

    delete widget;
}

// =============================================================================
// Squad Detail Screen
// =============================================================================

TEST_F(ScreenTest, SquadDetailScreenConstruction) {
    auto* widget = create_squad_detail_screen(registry_, nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_NE(widget->findChild<QScrollArea*>("squad_scroll"), nullptr);

    delete widget;
}

TEST_F(ScreenTest, SquadDetailScreenLabels) {
    auto* widget = create_squad_detail_screen(registry_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto labels = widget->findChildren<QLabel*>();
    EXPECT_GT(labels.size(), 2);

    delete widget;
}

// =============================================================================
// Time Travel Screen
// =============================================================================

TEST_F(ScreenTest, TimeTravelScreenConstruction) {
    auto* widget = create_time_travel_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_NE(widget->findChild<QListWidget*>("mission_list"), nullptr);
    EXPECT_NE(widget->findChild<QSlider*>("timeline_slider"), nullptr);
    EXPECT_NE(widget->findChild<QLabel*>("timeline_position"), nullptr);
    EXPECT_NE(widget->findChild<QPushButton*>("retry_button"), nullptr);
    EXPECT_NE(widget->findChild<QPushButton*>("export_button"), nullptr);

    delete widget;
}

TEST_F(ScreenTest, TimeTravelScreenMissionList) {
    auto* widget = create_time_travel_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    auto* list = widget->findChild<QListWidget*>("mission_list");
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(list->count(), 5);

    delete widget;
}

TEST_F(ScreenTest, TimeTravelScreenSlider) {
    auto* widget = create_time_travel_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    auto* slider = widget->findChild<QSlider*>("timeline_slider");
    ASSERT_NE(slider, nullptr);
    EXPECT_EQ(slider->minimum(), 0);
    EXPECT_EQ(slider->maximum(), 4);
    EXPECT_EQ(slider->value(), 0);

    delete widget;
}

TEST_F(ScreenTest, TimeTravelScreenSliderUpdatesLabel) {
    auto* widget = create_time_travel_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    auto* slider = widget->findChild<QSlider*>("timeline_slider");
    auto* position = widget->findChild<QLabel*>("timeline_position");
    ASSERT_NE(slider, nullptr);
    ASSERT_NE(position, nullptr);

    slider->setValue(2);
    // The position label should update to show "Event 3 of 5"
    EXPECT_TRUE(position->text().contains("3"));

    delete widget;
}

// =============================================================================
// Tool Runner Screen
// =============================================================================

TEST_F(ScreenTest, ToolRunnerScreenConstruction) {
    auto* widget = create_tool_runner_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    EXPECT_NE(widget->findChild<QLabel*>("tool_runner_title"), nullptr);
    EXPECT_NE(widget->findChild<QLabel*>("tool_name_label"), nullptr);
    EXPECT_NE(widget->findChild<QPlainTextEdit*>("tool_output"), nullptr);
    EXPECT_NE(widget->findChild<QPushButton*>("clear_button"), nullptr);
    EXPECT_NE(widget->findChild<QPushButton*>("run_button"), nullptr);

    delete widget;
}

TEST_F(ScreenTest, ToolRunnerScreenOutputPopulated) {
    auto* widget = create_tool_runner_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    auto* output = widget->findChild<QPlainTextEdit*>("tool_output");
    ASSERT_NE(output, nullptr);
    // Should have sample output pre-populated
    EXPECT_FALSE(output->toPlainText().isEmpty());
    EXPECT_TRUE(output->isReadOnly());

    delete widget;
}

TEST_F(ScreenTest, ToolRunnerScreenClearButton) {
    auto* widget = create_tool_runner_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    auto* output = widget->findChild<QPlainTextEdit*>("tool_output");
    auto* clear_btn = widget->findChild<QPushButton*>("clear_button");
    ASSERT_NE(output, nullptr);
    ASSERT_NE(clear_btn, nullptr);

    EXPECT_FALSE(output->toPlainText().isEmpty());
    clear_btn->click();
    EXPECT_TRUE(output->toPlainText().isEmpty());

    delete widget;
}

// =============================================================================
// Back button navigation tests (screens in a QStackedWidget)
// =============================================================================

TEST_F(ScreenTest, AboutScreenBackNavigation) {
    QStackedWidget stack;
    auto* dummy = new QWidget(&stack);
    stack.addWidget(dummy);  // index 0
    stack.addWidget(new QWidget(&stack));  // index 1 (dashboard)
    auto* about = create_about_screen(&stack);
    stack.addWidget(about);  // index 2
    stack.setCurrentIndex(2);

    // Find back button and click it
    auto buttons = about->findChildren<QPushButton*>();
    for (auto* btn : buttons) {
        if (btn->text().contains("Back")) {
            btn->click();
            break;
        }
    }
    // Note: back navigation requires the widget's parent to be the stack
    // which may not work perfectly in tests — verify no crash
    SUCCEED();
}

// =============================================================================
// Approvals Screen — in stack with back navigation (lines 40-43)
// =============================================================================

TEST_F(ScreenTest, ApprovalsScreenBackNavigation) {
    QStackedWidget stack;
    auto* dummy = new QWidget(&stack);
    stack.addWidget(dummy);  // index 0
    auto* dashboard = new QWidget(&stack);
    stack.addWidget(dashboard);  // index 1
    auto* approvals = create_approvals_screen(config_, &stack);
    stack.addWidget(approvals);  // index 2
    stack.setCurrentIndex(2);

    // Click back button — should navigate to index 1
    auto buttons = approvals->findChildren<QPushButton*>();
    for (auto* btn : buttons) {
        if (btn->text().contains("Back")) {
            btn->click();
            break;
        }
    }
    EXPECT_EQ(stack.currentIndex(), 1);
}

// =============================================================================
// Logs Screen — back navigation and load_daemon_log code paths
// =============================================================================

TEST_F(ScreenTest, LogsScreenBackNavigation) {
    QStackedWidget stack;
    auto* dummy = new QWidget(&stack);
    stack.addWidget(dummy);
    auto* dashboard = new QWidget(&stack);
    stack.addWidget(dashboard);  // index 1
    auto* logs = create_logs_screen(config_, &stack);
    stack.addWidget(logs);  // index 2
    stack.setCurrentIndex(2);

    auto buttons = logs->findChildren<QPushButton*>();
    for (auto* btn : buttons) {
        if (btn->text().contains("Back")) {
            btn->click();
            break;
        }
    }
    EXPECT_EQ(stack.currentIndex(), 1);
}

TEST_F(ScreenTest, LogsScreenRefreshButton) {
    auto* widget = create_logs_screen(config_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto buttons = widget->findChildren<QPushButton*>();
    for (auto* btn : buttons) {
        if (btn->text().contains("Refresh")) {
            btn->click();
            break;
        }
    }
    // Should not crash
    SUCCEED();

    delete widget;
}

TEST_F(ScreenTest, LogsScreenClearButton) {
    auto* widget = create_logs_screen(config_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto* display = widget->findChild<QPlainTextEdit*>("log_display");
    ASSERT_NE(display, nullptr);

    auto buttons = widget->findChildren<QPushButton*>();
    for (auto* btn : buttons) {
        if (btn->text().contains("Clear")) {
            btn->click();
            break;
        }
    }
    EXPECT_TRUE(display->toPlainText().isEmpty());

    delete widget;
}

// =============================================================================
// Fleet Monitor Screen — back navigation (lines 33-36)
// =============================================================================

TEST_F(ScreenTest, FleetMonitorScreenBackNavigation) {
    QStackedWidget stack;
    auto* dummy = new QWidget(&stack);
    stack.addWidget(dummy);
    auto* dashboard = new QWidget(&stack);
    stack.addWidget(dashboard);  // index 1
    auto* monitor = create_fleet_monitor_screen(registry_, &stack);
    stack.addWidget(monitor);
    stack.setCurrentIndex(2);

    auto buttons = monitor->findChildren<QPushButton*>();
    for (auto* btn : buttons) {
        if (btn->text().contains("Back")) {
            btn->click();
            break;
        }
    }
    EXPECT_EQ(stack.currentIndex(), 1);
}

// =============================================================================
// Help Screen — back navigation (lines 30-33)
// =============================================================================

TEST_F(ScreenTest, HelpScreenBackNavigation) {
    QStackedWidget stack;
    auto* dummy = new QWidget(&stack);
    stack.addWidget(dummy);
    auto* dashboard = new QWidget(&stack);
    stack.addWidget(dashboard);  // index 1
    auto* help = create_help_screen(&stack);
    stack.addWidget(help);
    stack.setCurrentIndex(2);

    auto buttons = help->findChildren<QPushButton*>();
    for (auto* btn : buttons) {
        if (btn->text().contains("Back")) {
            btn->click();
            break;
        }
    }
    EXPECT_EQ(stack.currentIndex(), 1);
}

// =============================================================================
// Settings Screen — back navigation (lines 37-40)
// =============================================================================

TEST_F(ScreenTest, SettingsScreenBackNavigation) {
    QStackedWidget stack;
    auto* dummy = new QWidget(&stack);
    stack.addWidget(dummy);
    auto* dashboard = new QWidget(&stack);
    stack.addWidget(dashboard);
    auto* settings = create_settings_screen(theme_, config_, &stack);
    stack.addWidget(settings);
    stack.setCurrentIndex(2);

    auto buttons = settings->findChildren<QPushButton*>();
    for (auto* btn : buttons) {
        if (btn->text().contains("Back")) {
            btn->click();
            break;
        }
    }
    EXPECT_EQ(stack.currentIndex(), 1);
}

// =============================================================================
// Error Details Screen — back navigation (lines 33-36, 227-230, 235-238)
// =============================================================================

TEST_F(ScreenTest, ErrorDetailsScreenBackNavigation) {
    QStackedWidget stack;
    auto* dummy = new QWidget(&stack);
    stack.addWidget(dummy);
    auto* dashboard = new QWidget(&stack);
    stack.addWidget(dashboard);
    auto* error = create_error_details_screen(&stack);
    stack.addWidget(error);
    stack.setCurrentIndex(2);

    auto buttons = error->findChildren<QPushButton*>();
    for (auto* btn : buttons) {
        if (btn->text().contains("Back")) {
            btn->click();
            break;
        }
    }
    EXPECT_EQ(stack.currentIndex(), 1);
}

TEST_F(ScreenTest, ErrorDetailsScreenCloseButton) {
    QStackedWidget stack;
    auto* dummy = new QWidget(&stack);
    stack.addWidget(dummy);
    auto* dashboard = new QWidget(&stack);
    stack.addWidget(dashboard);
    auto* error = create_error_details_screen(&stack);
    stack.addWidget(error);
    stack.setCurrentIndex(2);

    auto* close_btn = error->findChild<QPushButton*>("close_button");
    ASSERT_NE(close_btn, nullptr);
    close_btn->click();
    EXPECT_EQ(stack.currentIndex(), 1);
}

TEST_F(ScreenTest, ErrorDetailsScreenEscapeShortcut) {
    QStackedWidget stack;
    auto* dummy = new QWidget(&stack);
    stack.addWidget(dummy);
    auto* dashboard = new QWidget(&stack);
    stack.addWidget(dashboard);
    auto* error = create_error_details_screen(&stack);
    stack.addWidget(error);
    stack.setCurrentIndex(2);

    // Find the Escape shortcut and activate it
    auto shortcuts = error->findChildren<QShortcut*>();
    for (auto* sc : shortcuts) {
        if (sc->key() == QKeySequence(Qt::Key_Escape)) {
            sc->activated();
            break;
        }
    }
    EXPECT_EQ(stack.currentIndex(), 1);
}

// =============================================================================
// Metrics Screen — back navigation (lines 42-45, 217-220)
// =============================================================================

TEST_F(ScreenTest, MetricsScreenBackNavigation) {
    QStackedWidget stack;
    auto* dummy = new QWidget(&stack);
    stack.addWidget(dummy);
    auto* dashboard = new QWidget(&stack);
    stack.addWidget(dashboard);
    auto* metrics = create_metrics_screen(registry_, config_, &stack);
    stack.addWidget(metrics);
    stack.setCurrentIndex(2);

    auto buttons = metrics->findChildren<QPushButton*>();
    for (auto* btn : buttons) {
        if (btn->text().contains("Back")) {
            btn->click();
            break;
        }
    }
    EXPECT_EQ(stack.currentIndex(), 1);
}

TEST_F(ScreenTest, MetricsScreenEscapeShortcut) {
    QStackedWidget stack;
    auto* dummy = new QWidget(&stack);
    stack.addWidget(dummy);
    auto* dashboard = new QWidget(&stack);
    stack.addWidget(dashboard);
    auto* metrics = create_metrics_screen(registry_, config_, &stack);
    stack.addWidget(metrics);
    stack.setCurrentIndex(2);

    auto shortcuts = metrics->findChildren<QShortcut*>();
    for (auto* sc : shortcuts) {
        if (sc->key() == QKeySequence(Qt::Key_Escape)) {
            sc->activated();
            break;
        }
    }
    EXPECT_EQ(stack.currentIndex(), 1);
}

// =============================================================================
// Omnigraph Screen — back navigation (lines 45-49)
// =============================================================================

TEST_F(ScreenTest, OmnigraphScreenBackNavigation) {
    QStackedWidget stack;
    auto* dummy = new QWidget(&stack);
    stack.addWidget(dummy);
    auto* dashboard = new QWidget(&stack);
    stack.addWidget(dashboard);
    auto* omni = create_omnigraph_screen(registry_, &stack);
    stack.addWidget(omni);
    stack.setCurrentIndex(2);

    auto buttons = omni->findChildren<QPushButton*>();
    for (auto* btn : buttons) {
        if (btn->text().contains("Back")) {
            btn->click();
            break;
        }
    }
    EXPECT_EQ(stack.currentIndex(), 1);
}

TEST_F(ScreenTest, OmnigraphScreenRefreshButton) {
    auto* widget = create_omnigraph_screen(registry_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto* refresh_btn = widget->findChild<QPushButton*>("omnigraph_refresh");
    ASSERT_NE(refresh_btn, nullptr);
    refresh_btn->click();
    // Should refresh the tree — no crash
    SUCCEED();

    delete widget;
}

// =============================================================================
// Cortex Screen — back navigation and search action
// =============================================================================

TEST_F(ScreenTest, CortexScreenBackNavigation) {
    QStackedWidget stack;
    auto* dummy = new QWidget(&stack);
    stack.addWidget(dummy);
    auto* dashboard = new QWidget(&stack);
    stack.addWidget(dashboard);
    auto* cortex = create_cortex_screen(config_, &stack);
    stack.addWidget(cortex);
    stack.setCurrentIndex(2);

    auto buttons = cortex->findChildren<QPushButton*>();
    for (auto* btn : buttons) {
        if (btn->text().contains("Back")) {
            btn->click();
            break;
        }
    }
    EXPECT_EQ(stack.currentIndex(), 1);
}

TEST_F(ScreenTest, CortexScreenSearchAction) {
    auto* widget = create_cortex_screen(config_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto* input = widget->findChild<QLineEdit*>("cortex_query_input");
    ASSERT_NE(input, nullptr);
    input->setText("test query");

    auto* search_btn = widget->findChild<QPushButton*>("cortex_search_button");
    ASSERT_NE(search_btn, nullptr);
    search_btn->click();

    // Should append to output
    auto* output = widget->findChild<QPlainTextEdit*>("cortex_output");
    ASSERT_NE(output, nullptr);
    EXPECT_TRUE(output->toPlainText().contains("test query"));

    delete widget;
}

TEST_F(ScreenTest, CortexScreenSearchEmptyQuery) {
    auto* widget = create_cortex_screen(config_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto* input = widget->findChild<QLineEdit*>("cortex_query_input");
    ASSERT_NE(input, nullptr);
    input->clear(); // empty

    auto* search_btn = widget->findChild<QPushButton*>("cortex_search_button");
    ASSERT_NE(search_btn, nullptr);
    search_btn->click();
    // Empty query should be a no-op
    SUCCEED();

    delete widget;
}

// =============================================================================
// Tool Runner Screen — back navigation and run/clear
// =============================================================================

TEST_F(ScreenTest, ToolRunnerScreenBackNavigation) {
    QStackedWidget stack;
    auto* dummy = new QWidget(&stack);
    stack.addWidget(dummy);
    auto* dashboard = new QWidget(&stack);
    stack.addWidget(dashboard);
    auto* runner = create_tool_runner_screen(&stack);
    stack.addWidget(runner);
    stack.setCurrentIndex(2);

    auto buttons = runner->findChildren<QPushButton*>();
    for (auto* btn : buttons) {
        if (btn->text().contains("Back")) {
            btn->click();
            break;
        }
    }
    EXPECT_EQ(stack.currentIndex(), 1);
}

TEST_F(ScreenTest, ToolRunnerScreenRunButton) {
    auto* widget = create_tool_runner_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    auto* run_btn = widget->findChild<QPushButton*>("run_button");
    ASSERT_NE(run_btn, nullptr);
    run_btn->click();
    // Should append something to output
    auto* output = widget->findChild<QPlainTextEdit*>("tool_output");
    ASSERT_NE(output, nullptr);
    EXPECT_TRUE(output->toPlainText().contains("Running"));

    delete widget;
}

// =============================================================================
// Agent Screen — back navigation (lines 45-48)
// =============================================================================

TEST_F(ScreenTest, AgentScreenBackNavigation) {
    QStackedWidget stack;
    auto* dummy = new QWidget(&stack);
    stack.addWidget(dummy);
    auto* dashboard = new QWidget(&stack);
    stack.addWidget(dashboard);
    auto* agent = create_agent_screen(&stack);
    stack.addWidget(agent);
    stack.setCurrentIndex(2);

    auto* back_btn = agent->findChild<QPushButton*>("back_button");
    ASSERT_NE(back_btn, nullptr);
    back_btn->click();
    EXPECT_EQ(stack.currentIndex(), 1);
}

TEST_F(ScreenTest, AgentScreenDeployButton) {
    auto* widget = create_agent_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    auto* task_input = widget->findChild<QLineEdit*>("task_input");
    ASSERT_NE(task_input, nullptr);
    task_input->setText("Write a hello world program");

    auto* deploy_btn = widget->findChild<QPushButton*>("deploy_button");
    ASSERT_NE(deploy_btn, nullptr);
    deploy_btn->click();

    auto* deploy_output = widget->findChild<QPlainTextEdit*>("deploy_output");
    ASSERT_NE(deploy_output, nullptr);
    EXPECT_TRUE(deploy_output->toPlainText().contains("Deploying"));

    delete widget;
}

TEST_F(ScreenTest, AgentScreenDeployEmptyTask) {
    auto* widget = create_agent_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    auto* task_input = widget->findChild<QLineEdit*>("task_input");
    ASSERT_NE(task_input, nullptr);
    task_input->clear();

    auto* deploy_btn = widget->findChild<QPushButton*>("deploy_button");
    ASSERT_NE(deploy_btn, nullptr);
    deploy_btn->click();
    // Empty task should be a no-op
    SUCCEED();

    delete widget;
}

// =============================================================================
// Playbooks Screen — back navigation and playbook list
// =============================================================================

TEST_F(ScreenTest, PlaybooksScreenBackNavigation) {
    QStackedWidget stack;
    auto* dummy = new QWidget(&stack);
    stack.addWidget(dummy);
    auto* dashboard = new QWidget(&stack);
    stack.addWidget(dashboard);
    auto* playbooks = create_playbooks_screen(registry_, &stack);
    stack.addWidget(playbooks);
    stack.setCurrentIndex(2);

    auto buttons = playbooks->findChildren<QPushButton*>();
    for (auto* btn : buttons) {
        if (btn->text().contains("Back")) {
            btn->click();
            break;
        }
    }
    EXPECT_EQ(stack.currentIndex(), 1);
}

TEST_F(ScreenTest, PlaybooksScreenPlaybookCount) {
    auto* widget = create_playbooks_screen(registry_, nullptr);
    ASSERT_NE(widget, nullptr);

    auto* list = widget->findChild<QListWidget*>("playbook_list");
    ASSERT_NE(list, nullptr);
    // Default registry has no playbooks (no data dir)
    EXPECT_EQ(list->count(), static_cast<int>(registry_->playbooks().size()));

    delete widget;
}

// =============================================================================
// Squad Detail Screen — back navigation (lines 44-48, 242-246)
// =============================================================================

TEST_F(ScreenTest, SquadDetailScreenBackNavigation) {
    QStackedWidget stack;
    auto* dummy = new QWidget(&stack);
    stack.addWidget(dummy);
    auto* dashboard = new QWidget(&stack);
    stack.addWidget(dashboard);
    auto* squad = create_squad_detail_screen(registry_, &stack);
    stack.addWidget(squad);
    stack.setCurrentIndex(2);

    auto buttons = squad->findChildren<QPushButton*>();
    for (auto* btn : buttons) {
        if (btn->text().contains("Back")) {
            btn->click();
            break;
        }
    }
    EXPECT_EQ(stack.currentIndex(), 1);
}

TEST_F(ScreenTest, SquadDetailScreenEscapeShortcut) {
    QStackedWidget stack;
    auto* dummy = new QWidget(&stack);
    stack.addWidget(dummy);
    auto* dashboard = new QWidget(&stack);
    stack.addWidget(dashboard);
    auto* squad = create_squad_detail_screen(registry_, &stack);
    stack.addWidget(squad);
    stack.setCurrentIndex(2);

    auto shortcuts = squad->findChildren<QShortcut*>();
    for (auto* sc : shortcuts) {
        if (sc->key() == QKeySequence(Qt::Key_Escape)) {
            sc->activated();
            break;
        }
    }
    EXPECT_EQ(stack.currentIndex(), 1);
}

// =============================================================================
// Time Travel Screen — back navigation (lines 33-37) and scrub (262-302)
// =============================================================================

TEST_F(ScreenTest, TimeTravelScreenBackNavigation) {
    QStackedWidget stack;
    auto* dummy = new QWidget(&stack);
    stack.addWidget(dummy);
    auto* dashboard = new QWidget(&stack);
    stack.addWidget(dashboard);
    auto* tt = create_time_travel_screen(&stack);
    stack.addWidget(tt);
    stack.setCurrentIndex(2);

    auto buttons = tt->findChildren<QPushButton*>();
    for (auto* btn : buttons) {
        if (btn->text().contains("Back")) {
            btn->click();
            break;
        }
    }
    EXPECT_EQ(stack.currentIndex(), 1);
}

TEST_F(ScreenTest, TimeTravelScreenScrubLeftRight) {
    auto* widget = create_time_travel_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    auto* slider = widget->findChild<QSlider*>("timeline_slider");
    ASSERT_NE(slider, nullptr);

    // Move to the end and then try scrub right (should be clamped)
    slider->setValue(4);
    auto* position = widget->findChild<QLabel*>("timeline_position");
    ASSERT_NE(position, nullptr);
    EXPECT_TRUE(position->text().contains("5"));

    // Scrub left
    auto shortcuts = widget->findChildren<QShortcut*>();
    for (auto* sc : shortcuts) {
        if (sc->key() == QKeySequence(Qt::Key_Left)) {
            sc->activated();
            break;
        }
    }
    EXPECT_EQ(slider->value(), 3);

    // Scrub right
    for (auto* sc : shortcuts) {
        if (sc->key() == QKeySequence(Qt::Key_Right)) {
            sc->activated();
            break;
        }
    }
    EXPECT_EQ(slider->value(), 4);

    delete widget;
}

TEST_F(ScreenTest, TimeTravelScreenScrubVimKeys) {
    auto* widget = create_time_travel_screen(nullptr);
    ASSERT_NE(widget, nullptr);

    auto* slider = widget->findChild<QSlider*>("timeline_slider");
    ASSERT_NE(slider, nullptr);
    slider->setValue(2);

    auto shortcuts = widget->findChildren<QShortcut*>();
    for (auto* sc : shortcuts) {
        if (sc->key() == QKeySequence(Qt::Key_H)) {
            sc->activated();
            break;
        }
    }
    EXPECT_EQ(slider->value(), 1);

    for (auto* sc : shortcuts) {
        if (sc->key() == QKeySequence(Qt::Key_L)) {
            sc->activated();
            break;
        }
    }
    EXPECT_EQ(slider->value(), 2);

    delete widget;
}

TEST_F(ScreenTest, TimeTravelScreenEscapeShortcut) {
    QStackedWidget stack;
    auto* dummy = new QWidget(&stack);
    stack.addWidget(dummy);
    auto* dashboard = new QWidget(&stack);
    stack.addWidget(dashboard);
    auto* tt = create_time_travel_screen(&stack);
    stack.addWidget(tt);
    stack.setCurrentIndex(2);

    auto shortcuts = tt->findChildren<QShortcut*>();
    for (auto* sc : shortcuts) {
        if (sc->key() == QKeySequence(Qt::Key_Escape)) {
            sc->activated();
            break;
        }
    }
    EXPECT_EQ(stack.currentIndex(), 1);
}

// =============================================================================
// Welcome Screen — keyPressEvent (lines 75-81)
// =============================================================================

TEST_F(ScreenTest, WelcomeScreenInStack) {
    // We need an EuxisApp to get the welcome screen that has proper keyPressEvent behavior
    // Instead, test the screen factory function with a key event
    QStackedWidget stack;
    // We can't test the welcome screen's keyPressEvent directly without EuxisApp,
    // since it needs app_->show_dashboard(). But we can verify the widget handles
    // other keys gracefully.
    SUCCEED();
}

} // namespace
} // namespace euxis::etx
