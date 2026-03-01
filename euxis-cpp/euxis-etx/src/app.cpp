#include <euxis/etx/app.hpp>
#include <euxis/etx/theme.hpp>
#include <euxis/etx/config.hpp>
#include <euxis/etx/registry.hpp>

#include <QApplication>
#include <QMessageBox>
#include <QShortcut>
#include <QKeySequence>
#include <QInputDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QDialog>
#include <QDialogButtonBox>

// Forward declarations for screen widgets
namespace euxis::etx {
    QWidget* create_welcome_screen(EuxisApp* app, QWidget* parent);
    QWidget* create_dashboard_screen(EuxisApp* app, FleetRegistry* registry,
                                     ThemeEngine* theme, ETXConfig* config,
                                     QWidget* parent);
    QWidget* create_agent_screen(QWidget* parent);
    QWidget* create_about_screen(QWidget* parent);
    QWidget* create_fleet_monitor_screen(QWidget* parent);
    QWidget* create_settings_screen(ThemeEngine* theme, ETXConfig* config,
                                    QWidget* parent);
    QWidget* create_help_screen(QWidget* parent);
    QWidget* create_logs_screen(QWidget* parent);
    QWidget* create_playbooks_screen(QWidget* parent);
    QWidget* create_approvals_screen(QWidget* parent);
    QWidget* create_cortex_screen(QWidget* parent);
    QWidget* create_error_details_screen(QWidget* parent);
    QWidget* create_metrics_screen(QWidget* parent);
    QWidget* create_omnigraph_screen(QWidget* parent);
    QWidget* create_squad_detail_screen(QWidget* parent);
    QWidget* create_time_travel_screen(QWidget* parent);
    QWidget* create_tool_runner_screen(QWidget* parent);
}

namespace euxis::etx {

EuxisApp::EuxisApp(QWidget* parent)
    : QMainWindow(parent)
    , screen_stack_(new QStackedWidget(this))
    , theme_engine_(new ThemeEngine(this))
    , config_(new ETXConfig())
    , registry_(new FleetRegistry(this))
{
    setWindowTitle("Euxis ETX");
    setMinimumSize(1024, 768);
    setCentralWidget(screen_stack_);

    // Index 0: Welcome
    screen_stack_->addWidget(create_welcome_screen(this, screen_stack_));
    // Index 1: Dashboard
    screen_stack_->addWidget(create_dashboard_screen(this, registry_, theme_engine_,
                                                      config_, screen_stack_));
    // Index 2: Agent
    screen_stack_->addWidget(create_agent_screen(screen_stack_));
    // Index 3: Fleet Monitor
    screen_stack_->addWidget(create_fleet_monitor_screen(screen_stack_));
    // Index 4: About
    screen_stack_->addWidget(create_about_screen(screen_stack_));
    // Index 5: Settings
    screen_stack_->addWidget(create_settings_screen(theme_engine_, config_,
                                                     screen_stack_));
    // Index 6: Help
    screen_stack_->addWidget(create_help_screen(screen_stack_));
    // Index 7: Logs
    screen_stack_->addWidget(create_logs_screen(screen_stack_));
    // Index 8: Playbooks
    screen_stack_->addWidget(create_playbooks_screen(screen_stack_));
    // Index 9: Approvals
    screen_stack_->addWidget(create_approvals_screen(screen_stack_));
    // Index 10: Cortex
    screen_stack_->addWidget(create_cortex_screen(screen_stack_));
    // Index 11: Error Details
    screen_stack_->addWidget(create_error_details_screen(screen_stack_));
    // Index 12: Metrics
    screen_stack_->addWidget(create_metrics_screen(screen_stack_));
    // Index 13: OmniGraph
    screen_stack_->addWidget(create_omnigraph_screen(screen_stack_));
    // Index 14: Squad Detail
    screen_stack_->addWidget(create_squad_detail_screen(screen_stack_));
    // Index 15: Time Travel
    screen_stack_->addWidget(create_time_travel_screen(screen_stack_));
    // Index 16: Tool Runner
    screen_stack_->addWidget(create_tool_runner_screen(screen_stack_));

    setup_shortcuts();

    // Apply saved theme
    theme_engine_->apply_theme(config_->theme());

    // Load fleet data
    registry_->refresh();

    // Start at welcome
    show_welcome();

    connect(theme_engine_, &ThemeEngine::theme_applied,
            this, &EuxisApp::theme_changed);
}

void EuxisApp::show_dashboard() {
    screen_stack_->setCurrentIndex(1);
}

void EuxisApp::show_welcome() {
    screen_stack_->setCurrentIndex(0);
}

void EuxisApp::show_agent(const std::string& agent_id) {
    // Update agent screen with agent info
    auto* agent_widget = screen_stack_->widget(2);
    auto* name_label = agent_widget->findChild<QLabel*>("agent_name_label");
    auto* desc_label = agent_widget->findChild<QLabel*>("agent_desc_label");
    auto* status_label = agent_widget->findChild<QLabel*>("agent_status_label");

    const auto* info = registry_->find(QString::fromStdString(agent_id));
    if (info && name_label && desc_label && status_label) {
        name_label->setText(info->name);
        desc_label->setText(info->description);
        status_label->setText("Status: " + info->status);
    }

    screen_stack_->setCurrentIndex(2);
}

void EuxisApp::show_fleet_monitor(const std::string& operation_type,
                                   const std::vector<std::string>& members) {
    auto* monitor_widget = screen_stack_->widget(3);
    auto* op_label = monitor_widget->findChild<QLabel*>("operation_type_label");
    auto* member_list = monitor_widget->findChild<QListWidget*>("member_list");

    if (op_label) {
        op_label->setText("Operation: " + QString::fromStdString(operation_type));
    }
    if (member_list) {
        member_list->clear();
        for (const auto& m : members) {
            member_list->addItem(QString::fromStdString(m));
        }
    }

    screen_stack_->setCurrentIndex(3);
}

void EuxisApp::setup_shortcuts() {
    // Ctrl+K -> command palette
    auto* ctrl_k = new QShortcut(QKeySequence("Ctrl+K"), this);
    connect(ctrl_k, &QShortcut::activated, this, &EuxisApp::on_command_palette);

    // Ctrl+P -> command palette
    auto* ctrl_p = new QShortcut(QKeySequence("Ctrl+P"), this);
    connect(ctrl_p, &QShortcut::activated, this, &EuxisApp::on_command_palette);

    // / -> command palette
    auto* slash = new QShortcut(QKeySequence("/"), this);
    connect(slash, &QShortcut::activated, this, &EuxisApp::on_command_palette);

    // Ctrl+Q -> quit
    auto* ctrl_q = new QShortcut(QKeySequence("Ctrl+Q"), this);
    connect(ctrl_q, &QShortcut::activated, this, &QWidget::close);

    // F3 -> toggle theme
    auto* f3 = new QShortcut(QKeySequence("F3"), this);
    connect(f3, &QShortcut::activated, this, &EuxisApp::on_toggle_theme);

    // F5 -> refresh
    auto* f5 = new QShortcut(QKeySequence("F5"), this);
    connect(f5, &QShortcut::activated, this, &EuxisApp::on_refresh);
}

void EuxisApp::on_toggle_theme() {
    theme_engine_->cycle_theme();
    config_->set_theme(theme_engine_->current_theme());
}

void EuxisApp::on_deploy_agent(const QString& agent_id, const QString& task) {
    QMessageBox::information(this, "Deploy Agent",
        QString("Deploying agent '%1' with task:\n%2").arg(agent_id, task));
}

void EuxisApp::on_deploy_squad(const QString& squad_id, const QString& task) {
    QMessageBox::information(this, "Deploy Squad",
        QString("Deploying squad '%1' with task:\n%2").arg(squad_id, task));
}

void EuxisApp::on_refresh() {
    registry_->refresh();
}

void EuxisApp::on_command_palette() {
    QDialog dialog(this);
    dialog.setWindowTitle("Command Palette");
    dialog.setMinimumWidth(500);

    auto* layout = new QVBoxLayout(&dialog);

    auto* search = new QLineEdit(&dialog);
    search->setPlaceholderText("Type a command...");
    search->setObjectName("palette_search");
    layout->addWidget(search);

    auto* list = new QListWidget(&dialog);
    list->setObjectName("palette_list");

    QStringList commands = {
        "Go to Dashboard",
        "Go to Welcome",
        "Go to Settings",
        "Go to About",
        "Go to Help",
        "Go to Logs",
        "Go to Playbooks",
        "Go to Approvals",
        "Go to Cortex",
        "Go to Error Details",
        "Go to Metrics",
        "Go to OmniGraph",
        "Go to Squad Detail",
        "Go to Time Travel",
        "Go to Tool Runner",
        "Toggle Theme",
        "Refresh Fleet",
        "Quit"
    };
    list->addItems(commands);
    layout->addWidget(list);

    auto filter_list = [&](const QString& text) {
        for (int i = 0; i < list->count(); ++i) {
            auto* item = list->item(i);
            item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
        }
    };

    connect(search, &QLineEdit::textChanged, filter_list);

    auto execute_command = [&](QListWidgetItem* item) {
        const QString cmd = item->text();
        dialog.accept();

        if (cmd == "Go to Dashboard") show_dashboard();
        else if (cmd == "Go to Welcome") show_welcome();
        else if (cmd == "Go to Settings") screen_stack_->setCurrentIndex(5);
        else if (cmd == "Go to About") screen_stack_->setCurrentIndex(4);
        else if (cmd == "Go to Help") screen_stack_->setCurrentIndex(6);
        else if (cmd == "Go to Logs") screen_stack_->setCurrentIndex(7);
        else if (cmd == "Go to Playbooks") screen_stack_->setCurrentIndex(8);
        else if (cmd == "Go to Approvals") screen_stack_->setCurrentIndex(9);
        else if (cmd == "Go to Cortex") screen_stack_->setCurrentIndex(10);
        else if (cmd == "Go to Error Details") screen_stack_->setCurrentIndex(11);
        else if (cmd == "Go to Metrics") screen_stack_->setCurrentIndex(12);
        else if (cmd == "Go to OmniGraph") screen_stack_->setCurrentIndex(13);
        else if (cmd == "Go to Squad Detail") screen_stack_->setCurrentIndex(14);
        else if (cmd == "Go to Time Travel") screen_stack_->setCurrentIndex(15);
        else if (cmd == "Go to Tool Runner") screen_stack_->setCurrentIndex(16);
        else if (cmd == "Toggle Theme") on_toggle_theme();
        else if (cmd == "Refresh Fleet") on_refresh();
        else if (cmd == "Quit") close();
    };

    connect(list, &QListWidget::itemDoubleClicked, execute_command);
    connect(list, &QListWidget::itemActivated, execute_command);

    search->setFocus();
    dialog.exec();
}

void EuxisApp::track_error(const QString& severity, const QString& message) {
    error_count_++;
    emit error_tracked(severity, message);

    if (error_count_ > 5) {
        suggest_calm_theme();
    }
}

void EuxisApp::trigger_focus_transition() {
    theme_engine_->apply_theme("focused");
    config_->set_theme("focused");
}

void EuxisApp::suggest_calm_theme() {
    if (calm_suggested_) return;
    calm_suggested_ = true;

    auto result = QMessageBox::question(this, "Theme Suggestion",
        "Multiple errors detected. Would you like to switch to the Calm theme "
        "for reduced visual strain?",
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes) {
        theme_engine_->apply_theme("calm");
        config_->set_theme("calm");
    }
}

} // namespace euxis::etx
