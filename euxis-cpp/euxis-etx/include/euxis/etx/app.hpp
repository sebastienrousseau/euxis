/// @file
/// @brief ETX app
#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <string>
#include <vector>

namespace euxis::etx {

class ThemeEngine;
class ETXConfig;
class FleetRegistry;
class ChatEngine;

class EuxisApp : public QMainWindow {
    Q_OBJECT

public:
    explicit EuxisApp(QWidget* parent = nullptr);
    ~EuxisApp() override;

    void show_dashboard();
    void show_welcome();
    void show_agent(const std::string& agent_id);
    void show_fleet_monitor(const std::string& operation_type,
                            const std::vector<std::string>& members);

signals:
    void theme_changed(const QString& theme_name);
    void error_tracked(const QString& severity, const QString& message);

private slots:
    void on_toggle_theme();
    void on_deploy_agent(const QString& agent_id, const QString& task);
    void on_deploy_squad(const QString& squad_id, const QString& task);
    void on_refresh();
    void on_command_palette();

private:
    int error_count_ = 0;
    bool calm_suggested_ = false;
    void track_error(const QString& severity, const QString& message);
    void trigger_focus_transition();
    void suggest_calm_theme();
    void setup_shortcuts();
    static auto screen_name_for(int index) -> QString;

    QStackedWidget* screen_stack_;
    ThemeEngine* theme_engine_;
    ETXConfig* config_;
    FleetRegistry* registry_;
    ChatEngine* chat_engine_;
};

} // namespace euxis::etx
